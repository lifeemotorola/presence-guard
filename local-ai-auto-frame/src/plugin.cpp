#include <obs-module.h>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/dnn.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-local-ai", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "Local AI face auto-framing for asynchronous camera sources";
}
using Clock = std::chrono::steady_clock;

struct Filter {
	std::mutex mutex;
	std::condition_variable wake;
	bool stop = false, pending = false;
	cv::Mat job;
	float target_x = .5f, target_y = .5f;
	Clock::time_point last_seen{};
	std::thread worker;
	std::atomic<int> backend{0}, interval{100};
	std::atomic<double> zoom{1.6}, smoothing{0.12};
	std::string model;
	obs_source_frame *output = nullptr;
	float x = .5f, y = .5f;
	Clock::time_point last_submit{};
};

static void worker_loop(Filter *f)
{
	cv::Ptr<cv::FaceDetectorYN> detector;
	int loaded_backend = -1;
	while (true) {
		cv::Mat image;
		{
			std::unique_lock<std::mutex> lock(f->mutex);
			f->wake.wait(lock, [&] { return f->stop || f->pending; });
			if (f->stop)
				return;
			image = std::move(f->job);
			f->pending = false;
		}
		try {
			int requested = f->backend.load();
			if (!detector || requested != loaded_backend) {
				loaded_backend = requested;
				try {
					detector = cv::FaceDetectorYN::create(
						f->model, "", image.size(), 0.80f, 0.3f, 5000,
						requested ? cv::dnn::DNN_BACKEND_CUDA : cv::dnn::DNN_BACKEND_OPENCV,
						requested ? cv::dnn::DNN_TARGET_CUDA : cv::dnn::DNN_TARGET_CPU);
					blog(LOG_INFO, "[Local AI] detector initialized (%s)",
					     requested ? "CUDA requested" : "CPU");
				} catch (const cv::Exception &e) {
					blog(LOG_WARNING, "[Local AI] detector initialization failed: %s", e.what());
					detector.release();
				}
			}
			if (!detector)
				continue;
			detector->setInputSize(image.size());
			cv::Mat faces;
			try {
				detector->detect(image, faces);
			} catch (const cv::Exception &e) {
				if (!loaded_backend)
					throw;
				blog(LOG_WARNING, "[Local AI] CUDA inference failed, using CPU: %s", e.what());
				detector = cv::FaceDetectorYN::create(f->model, "", image.size(), 0.80f, 0.3f, 5000,
								      cv::dnn::DNN_BACKEND_OPENCV,
								      cv::dnn::DNN_TARGET_CPU);
				detector->detect(image, faces);
			}
			if (faces.rows > 0) {
				int best = 0;
				float area = 0;
				for (int i = 0; i < faces.rows; ++i) {
					float a = faces.at<float>(i, 2) * faces.at<float>(i, 3);
					if (a > area) {
						area = a;
						best = i;
					}
				}
				float cx = (faces.at<float>(best, 0) + faces.at<float>(best, 2) * .5f) / image.cols;
				// Put the face slightly above the center, leaving room for shoulders.
				float cy = (faces.at<float>(best, 1) + faces.at<float>(best, 3) * 1.0f) / image.rows;
				std::lock_guard<std::mutex> lock(f->mutex);
				f->target_x = std::clamp(cx, 0.f, 1.f);
				f->target_y = std::clamp(cy, 0.f, 1.f);
				f->last_seen = Clock::now();
			}
		} catch (const cv::Exception &e) {
			blog(LOG_WARNING, "[Local AI] inference: %s", e.what());
		}
	}
}
static const char *name(void *)
{
	return "Local AI Auto Frame";
}
static void update(void *data, obs_data_t *s)
{
	auto *f = static_cast<Filter *>(data);
	f->zoom = std::clamp(obs_data_get_double(s, "zoom"), 1.0, 3.0);
	f->smoothing = std::clamp(obs_data_get_double(s, "smoothing"), 0.01, 1.0);
	f->interval = std::clamp((int)obs_data_get_int(s, "interval"), 50, 1000);
	f->backend = (int)obs_data_get_int(s, "backend");
}
static void *create(obs_data_t *s, obs_source_t *)
{
	auto *f = new Filter;
	char *path = obs_module_file("face_detection_yunet_2023mar.onnx");
	if (path) {
		f->model = path;
		bfree(path);
	} else
		blog(LOG_ERROR, "[Local AI] model not found; reinstall plugin data");
	update(f, s);
	f->worker = std::thread(worker_loop, f);
	return f;
}
static void destroy(void *data)
{
	auto *f = static_cast<Filter *>(data);
	{
		std::lock_guard<std::mutex> lock(f->mutex);
		f->stop = true;
	}
	f->wake.notify_one();
	if (f->worker.joinable())
		f->worker.join();
	if (f->output)
		obs_source_frame_destroy(f->output);
	delete f;
}
static void defaults(obs_data_t *s)
{
	obs_data_set_default_double(s, "zoom", 1.6);
	obs_data_set_default_double(s, "smoothing", 0.12);
	obs_data_set_default_int(s, "interval", 100);
	obs_data_set_default_int(s, "backend", 0);
}
static obs_properties_t *properties(void *)
{
	obs_properties_t *p = obs_properties_create();
	obs_properties_add_float_slider(p, "zoom", "Zoom", 1.0, 3.0, 0.05);
	obs_properties_add_float_slider(p, "smoothing", "Follow speed (higher is faster)", 0.01, 1.0, 0.01);
	obs_properties_add_int_slider(p, "interval", "Detection interval (ms)", 50, 1000, 10);
	auto *list =
		obs_properties_add_list(p, "backend", "Inference backend", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(list, "CPU (recommended)", 0);
	obs_property_list_add_int(list, "NVIDIA CUDA (requires CUDA-enabled OpenCV)", 1);
	return p;
}
static obs_source_frame *video(void *data, obs_source_frame *frame)
{
	auto *f = static_cast<Filter *>(data);
	// This first version intentionally passes through unsupported OBS formats unchanged.
	if (!frame || !frame->data[0] || !frame->width || !frame->height ||
	    (frame->format != VIDEO_FORMAT_BGRA && frame->format != VIDEO_FORMAT_RGBA &&
	     frame->format != VIDEO_FORMAT_BGRX))
		return frame;
	try {
		cv::Mat input((int)frame->height, (int)frame->width, CV_8UC4, frame->data[0], frame->linesize[0]);
		auto now = Clock::now();
		if (now - f->last_submit >= std::chrono::milliseconds(f->interval.load())) {
			std::lock_guard<std::mutex> lock(f->mutex);
			if (!f->pending) {
				cv::Mat small;
				double scale = std::min(1.0, 320.0 / frame->width);
				cv::resize(input, small, cv::Size(), scale, scale, cv::INTER_AREA);
				cv::cvtColor(small, f->job,
					     frame->format == VIDEO_FORMAT_RGBA ? cv::COLOR_RGBA2BGR
										: cv::COLOR_BGRA2BGR);
				f->pending = true;
				f->last_submit = now;
				f->wake.notify_one();
			}
		}
		float tx = .5f, ty = .5f;
		{
			std::lock_guard<std::mutex> lock(f->mutex);
			if (now - f->last_seen < std::chrono::seconds(2)) {
				tx = f->target_x;
				ty = f->target_y;
			}
		}
		float a = (float)f->smoothing.load();
		f->x += (tx - f->x) * a;
		f->y += (ty - f->y) * a;
		double zoom = f->zoom.load();
		int w = std::max(1, (int)(frame->width / zoom)), h = std::max(1, (int)(frame->height / zoom));
		int left = std::clamp((int)(f->x * frame->width - w / 2), 0, (int)frame->width - w);
		int top = std::clamp((int)(f->y * frame->height - h / 2), 0, (int)frame->height - h);
		if (!f->output || f->output->width != frame->width || f->output->height != frame->height ||
		    f->output->format != frame->format) {
			if (f->output)
				obs_source_frame_destroy(f->output);
			f->output = obs_source_frame_create(frame->format, frame->width, frame->height);
		}
		if (!f->output)
			return frame;
		// Copy frame attributes separately: obs_source_frame_copy only copies pixels.
		f->output->timestamp = frame->timestamp;
		std::memcpy(f->output->color_matrix, frame->color_matrix, sizeof(frame->color_matrix));
		f->output->full_range = frame->full_range;
		f->output->max_luminance = frame->max_luminance;
		std::memcpy(f->output->color_range_min, frame->color_range_min, sizeof(frame->color_range_min));
		std::memcpy(f->output->color_range_max, frame->color_range_max, sizeof(frame->color_range_max));
		f->output->flip = frame->flip;
		f->output->flags = frame->flags;
		f->output->trc = frame->trc;
		cv::Mat output((int)frame->height, (int)frame->width, CV_8UC4, f->output->data[0],
			       f->output->linesize[0]);
		cv::resize(input(cv::Rect(left, top, w, h)), output, output.size(), 0, 0, cv::INTER_LINEAR);
		return f->output;
	} catch (const cv::Exception &e) {
		blog(LOG_WARNING, "[Local AI] video: %s", e.what());
		return frame;
	}
}
bool obs_module_load(void)
{
	obs_source_info info{};
	info.id = "local_ai_auto_frame";
	info.type = OBS_SOURCE_TYPE_FILTER;
	info.output_flags = OBS_SOURCE_ASYNC_VIDEO;
	info.get_name = name;
	info.create = create;
	info.destroy = destroy;
	info.update = update;
	info.get_defaults = defaults;
	info.get_properties = properties;
	info.filter_video = video;
	obs_register_source(&info);
	/* Say so in obs-studio/logs: without this line a plugin that OBS never
	 * discovered is indistinguishable from one that failed to register. */
	blog(LOG_INFO, "[Local AI] module loaded, filter id 'local_ai_auto_frame'");
	return true;
}

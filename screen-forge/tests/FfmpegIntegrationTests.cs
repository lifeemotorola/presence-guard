using System;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using ScreenForge.Core;
using ScreenForge.Recording;
using Xunit;

namespace ScreenForge.Tests;

/// <summary>Runs the real mux command on synthetic media; no screen or sound card is needed.</summary>
public class FfmpegIntegrationTests
{
    private static readonly string Ffmpeg =
        Environment.GetEnvironmentVariable("SCREENFORGE_FFMPEG") ?? "ffmpeg";

    [Theory]
    [InlineData("none")]
    [InlineData("one-short")]
    [InlineData("two-short")]
    [InlineData("long")]
    public async Task FinishedPictureIsNotCutToTheLengthOfTheAudio(string scenario)
    {
        string dir = Path.Combine(Path.GetTempPath(), "screenforge-mux-test-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(dir);
        try
        {
            string video = Path.Combine(dir, "take.mp4");
            string audio = Path.Combine(dir, "system.wav");
            string secondAudio = Path.Combine(dir, "mic.wav");
            string output = Path.Combine(dir, "finished.mp4");

            await Run("-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi",
                "-i", "testsrc2=size=160x90:rate=10:duration=8", "-c:v", "libx264", "-pix_fmt", "yuv420p",
                "-g", "20", "-an", "-movflags", "+frag_keyframe+empty_moov+default_base_moof", video);

            if (scenario != "none")
            {
                string seconds = scenario == "long" ? "12" : "3";
                await Run("-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi",
                    "-i", "sine=frequency=440:duration=" + seconds, "-c:a", "pcm_s16le", audio);
            }
            if (scenario == "two-short")
                await Run("-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi",
                    "-i", "sine=frequency=880:duration=2", "-c:a", "pcm_s16le", secondAudio);

            var request = new MuxRequest
            {
                VideoPath = video,
                SystemAudioPath = scenario == "none" ? null : audio,
                MicrophonePath = scenario == "two-short" ? secondAudio : null,
                OutputPath = output,
            };
            await Run(FfmpegArgs.Mux(request).ToArray());

            int inputFrames = await Frames(video);
            int outputFrames = await Frames(output);
            Assert.Equal(80, inputFrames);
            Assert.Equal(inputFrames, outputFrames);

            ProcessResult probe = await FfmpegProcess.RunAsync(Ffmpeg, FfmpegArgs.ProbeDuration(output), 20000);
            Assert.False(probe.TimedOut);
            TimeSpan? duration = FfmpegArgs.ParseDuration(probe.Output);
            Assert.NotNull(duration);
            Assert.InRange(duration.Value.TotalSeconds, 7.8, 8.3);
            if (scenario != "none")
                Assert.Contains("Audio: aac", probe.Output);
        }
        finally
        {
            Directory.Delete(dir, true);
        }
    }

    private static async Task<int> Frames(string path)
    {
        string output = await Run("-hide_banner", "-loglevel", "error", "-i", path,
            "-map", "0:v:0", "-f", "framemd5", "-");
        return output.Split('\n').Count(line => line.TrimStart().StartsWith("0,", StringComparison.Ordinal));
    }

    private static async Task<string> Run(params string[] args)
    {
        ProcessResult result = await FfmpegProcess.RunAsync(Ffmpeg, args, 30000);
        if (!result.Ok)
            throw new InvalidOperationException("ffmpeg test failed (" + result.ExitCode + "): " + result.Output);
        return result.Output;
    }
}

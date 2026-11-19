/*
MixBloom for OBS Studio
Copyright (C) 2026 Emmanuel Suah
Licensed under GPL-2.0-or-later; see LICENSE. No warranty.
*/
#include "mix-dock.hpp"

#include <QAbstractButton>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSlider>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include <chrono>
#include <cmath>
#include <memory>
#include <string>

namespace {
QString qstr(const std::string &value)
{
	return QString::fromUtf8(value.data(), static_cast<int>(value.size()));
}
std::string utf8(const QString &value)
{
	return value.toUtf8().toStdString();
}
QLabel *makeLabel(const QString &text, const char *name, QWidget *parent, bool wrap = false)
{
	auto *label = new QLabel(text, parent);
	label->setObjectName(name);
	label->setTextFormat(Qt::PlainText);
	label->setWordWrap(wrap);
	return label;
}

class BrandMark final : public QWidget {
public:
	explicit BrandMark(QWidget *parent = nullptr) : QWidget(parent) { setFixedSize(42, 42); }

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);
		QLinearGradient gradient(0, 0, 42, 42);
		gradient.setColorAt(0, QColor("#a0f4d5"));
		gradient.setColorAt(1, QColor("#54cfc7"));
		p.setPen(Qt::NoPen);
		p.setBrush(gradient);
		p.drawRoundedRect(rect(), 12, 12);
		p.setBrush(QColor("#102d32"));
		const int heights[] = {10, 18, 26, 17};
		for (int i = 0; i < 4; ++i)
			p.drawRoundedRect(QRectF(10 + i * 6, (42 - heights[i]) / 2.0, 4, heights[i]), 2, 2);
	}
};

bool confirmAction(QWidget *parent, const QString &title, const QString &text, const QString &detail)
{
	QMessageBox box(QMessageBox::Warning, title, text, QMessageBox::Yes | QMessageBox::No, parent);
	box.setTextFormat(Qt::PlainText);
	box.setInformativeText(detail);
	box.setDefaultButton(QMessageBox::No);
	return box.exec() == QMessageBox::Yes;
}
} // namespace

class SourceCard final : public QFrame {
public:
	SourceCard(const mixbloom::Source &source, QWidget *parent) : QFrame(parent), source_(source)
	{
		setObjectName("sourceCard");
		setProperty("sourceId", qstr(source.uuid));
		auto *layout = new QVBoxLayout(this);
		layout->setContentsMargins(14, 14, 14, 14);
		layout->setSpacing(8);

		auto *top = new QHBoxLayout;
		top->setSpacing(8);
		top->addWidget(makeLabel("AUDIO SOURCE", "overline", this));
		top->addStretch();
		status_ = makeLabel("", "sourceStatus", this);
		top->addWidget(status_);
		layout->addLayout(top);

		auto *titleRow = new QHBoxLayout;
		titleRow->setSpacing(8);
		title_ = makeLabel("", "sourceName", this, true);
		title_->setMinimumWidth(0);
		title_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		titleRow->addWidget(title_, 1);
		mute_ = new QPushButton(this);
		mute_->setObjectName("muteButton");
		mute_->setMinimumWidth(66);
		mute_->setMinimumHeight(33);
		titleRow->addWidget(mute_, 0, Qt::AlignTop);
		layout->addLayout(titleRow);

		auto *levelRow = new QHBoxLayout;
		levelRow->addWidget(makeLabel("LEVEL", "fieldLabel", this));
		levelRow->addStretch();
		db_ = makeLabel("", "levelValue", this);
		levelRow->addWidget(db_);
		layout->addLayout(levelRow);
		fader_ = new QSlider(Qt::Horizontal, this);
		fader_->setObjectName("fader");
		fader_->setRange(0, mixbloom::kFaderMax);
		fader_->setSingleStep(10);
		fader_->setPageStep(50);
		fader_->setMinimumHeight(24);
		fader_->setToolTip("Changes the OBS source level immediately. Maximum: 0 dB.");
		layout->addWidget(fader_);

		auto *peakRow = new QHBoxLayout;
		peakRow->addWidget(makeLabel("SOURCE PEAK", "smallLabel", this));
		peakRow->addStretch();
		peakRow->addWidget(makeLabel(QStringLiteral("−60   ·   −30   ·   0 dB"), "smallLabel", this));
		layout->addLayout(peakRow);
		meter_ = new QProgressBar(this);
		meter_->setObjectName("peakMeter");
		meter_->setRange(0, 100);
		meter_->setValue(0);
		meter_->setTextVisible(false);
		meter_->setFixedHeight(8);
		meter_->setToolTip(
			"Highest source channel after its fader; this does not verify stream or recording output.");
		layout->addWidget(meter_);
		meterNotice_ = makeLabel("Meter unavailable; audio controls still work.", "meterNotice", this, true);
		layout->addWidget(meterNotice_);

		auto *routeRow = new QHBoxLayout;
		routeRow->addWidget(makeLabel("OUTPUT TRACKS", "fieldLabel", this));
		routeRow->addStretch();
		routeState_ = makeLabel("", "routeState", this);
		routeRow->addWidget(routeState_);
		layout->addLayout(routeRow);
		auto *tracks = new QHBoxLayout;
		tracks->setSpacing(4);
		for (unsigned i = 1; i <= mixbloom::kTrackCount; ++i) {
			auto *track = new QPushButton(QString::number(i), this);
			track->setObjectName("trackButton");
			track->setProperty("trackNumber", static_cast<int>(i));
			track->setCheckable(true);
			track->setMinimumWidth(25);
			track->setMinimumHeight(31);
			track->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
			track->setToolTip(QString("Include this source on OBS audio track %1.").arg(i));
			tracks->addWidget(track);
			track_[i - 1] = track;
		}
		layout->addLayout(tracks);
		warning_ = makeLabel("", "sourceWarning", this, true);
		layout->addWidget(warning_);
		monitor_ = new QPushButton(this);
		monitor_->setObjectName("monitorButton");
		monitor_->setMinimumHeight(34);
		monitor_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		layout->addWidget(monitor_);

		tap_ = std::make_unique<mixbloom::MeterTap>(source.uuid);
		apply(source);
	}

	const mixbloom::Source &source() const { return source_; }
	QPushButton *muteButton() const { return mute_; }
	QPushButton *monitorButton() const { return monitor_; }
	QSlider *fader() const { return fader_; }
	QPushButton *trackButton(unsigned index) const { return track_[index - 1]; }

	void apply(const mixbloom::Source &source)
	{
		source_ = source;
		const QString name = qstr(source.name);
		title_->setText(name);
		title_->setAccessibleName("Audio source " + name);
		status_->setText(source.muted ? "MUTED" : "UNMUTED");
		status_->setProperty("isMuted", source.muted);
		status_->style()->unpolish(status_);
		status_->style()->polish(status_);
		mute_->setText(source.muted ? "Unmute" : "Mute");
		mute_->setAccessibleName((source.muted ? "Unmute " : "Mute ") + name);
		mute_->setToolTip("Mute or unmute this source on all OBS tracks.");
		const float db = mixbloom::gainToDb(source.gain);
		db_->setText(std::isinf(db) && db < 0 ? QStringLiteral("−∞ dB") : QString::number(db, 'f', 1) + " dB");
		fader_->setAccessibleName("Level for " + name);
		fader_->setEnabled(!source.volumeLocked);
		if (!fader_->isSliderDown()) {
			const QSignalBlocker blocker(fader_);
			fader_->setValue(mixbloom::gainToFader(source.gain));
		}
		meter_->setAccessibleName("Source peak for " + name);
		for (unsigned i = 1; i <= mixbloom::kTrackCount; ++i) {
			const QSignalBlocker blocker(track_[i - 1]);
			track_[i - 1]->setChecked(mixbloom::trackEnabled(source.tracks, i));
			track_[i - 1]->setAccessibleName(QString("Send %1 to OBS track %2").arg(name).arg(i));
		}
		const bool noTracks = (source.tracks & mixbloom::kTrackMask) == 0;
		routeState_->setText(noTracks ? "NO TRACKS" : "SELECTED");
		routeState_->setProperty("noTracks", noTracks);
		routeState_->style()->unpolish(routeState_);
		routeState_->style()->polish(routeState_);
		QStringList warnings;
		if (source.volumeLocked)
			warnings << "Level is locked in OBS.";
		if (source.gain > 1.0f)
			warnings << "Above 0 dB in OBS; moving the fader caps it at 0 dB.";
		if (noTracks)
			warnings << "No output tracks selected; this source may not be heard.";
		warning_->setText(warnings.join(" "));
		warning_->setVisible(!warnings.isEmpty());
		monitor_->setText(source.monitoring ? "Monitor: On" : "Monitor: Off");
		monitor_->setAccessibleName(
			QString("Monitoring for %1, currently %2").arg(name, source.monitoring ? "on" : "off"));
		monitor_->setEnabled(source.canMonitor || source.monitoring);
		monitor_->setToolTip(
			source.canMonitor
				? "Send this source to the OBS monitoring device as well as its selected output tracks."
				: "OBS marks this source as unsafe to monitor because it may cause feedback.");
		if (!tap_->attached()) {
			tap_ = std::make_unique<mixbloom::MeterTap>(source.uuid);
			lastSequence_ = 0;
			lastSample_ = {};
			meter_->setValue(0);
		}
		meterNotice_->setVisible(!tap_->attached());
	}

	void updateMeter()
	{
		const auto sample = tap_->sample();
		const auto now = std::chrono::steady_clock::now();
		if (sample.sequence != lastSequence_) {
			lastSequence_ = sample.sequence;
			lastSample_ = now;
			meter_->setValue(mixbloom::peakToPercent(sample.db));
		} else if (now - lastSample_ > std::chrono::milliseconds(700)) {
			meter_->setValue(0);
		}
	}

private:
	mixbloom::Source source_;
	QLabel *title_ = nullptr, *status_ = nullptr, *db_ = nullptr, *routeState_ = nullptr;
	QLabel *warning_ = nullptr, *meterNotice_ = nullptr;
	QPushButton *mute_ = nullptr, *monitor_ = nullptr, *track_[mixbloom::kTrackCount] = {};
	QSlider *fader_ = nullptr;
	QProgressBar *meter_ = nullptr;
	std::unique_ptr<mixbloom::MeterTap> tap_;
	uint64_t lastSequence_ = 0;
	std::chrono::steady_clock::time_point lastSample_{};
};

MixDock::MixDock(QWidget *parent) : QWidget(parent)
{
	buildUi();
	refreshNow();
	refreshTimer_ = new QTimer(this);
	refreshTimer_->setInterval(1000);
	connect(refreshTimer_, &QTimer::timeout, this, [this] { refreshNow(); });
	refreshTimer_->start();
	meterTimer_ = new QTimer(this);
	meterTimer_->setInterval(100);
	connect(meterTimer_, &QTimer::timeout, this, [this] { updateMeters(); });
	meterTimer_->start();
}

MixDock::~MixDock()
{
	/* Destroy taps before QWidget tears down its children and before OBS unloads. */
	clearCards();
}

void MixDock::buildUi()
{
	setObjectName("mixBloomRoot");
	setStyleSheet(R"(
		QWidget#mixBloomRoot, QWidget#mixBody { background: #0b111c; }
		QWidget { color: #e5edf5; font-family: 'Segoe UI'; font-size: 12px; }
		QScrollArea { border: none; background: transparent; }
		QFrame#hero { background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #183b41,stop:1 #142336);
		              border: 1px solid #32595b; border-radius: 14px; }
		QFrame#sourceCard, QFrame#emptyCard { background: #141e2c; border: 1px solid #2b3b4c; border-radius: 12px; }
		QFrame#noticeCard { background: #29291f; border: 1px solid #665b36; border-radius: 10px; }
		QLabel#eyebrow, QLabel#overline { color: #8ce4ca; font-size: 10px; font-weight: 700; letter-spacing: 1px; }
		QLabel#brand { color: #f5fffc; font-size: 24px; font-weight: 700; }
		QLabel#tagline { color: #c6dedc; font-size: 12px; }
		QLabel#heroFoot { color: #a1d8c9; font-size: 10px; font-weight: 600; }
		QLabel#notice { color: #f3dca8; font-size: 12px; }
		QLabel#sectionTitle { color: #f4f8fb; font-size: 17px; font-weight: 700; }
		QLabel#sectionCaption, QLabel#footer { color: #a9bdcd; font-size: 12px; }
		QLabel#sourceName { color: #f2f7fb; font-size: 16px; font-weight: 700; }
		QLabel#sourceStatus { color: #92e5ca; background: #203e38; border-radius: 5px; padding: 4px 7px; font-size: 10px; font-weight: 700; }
		QLabel#sourceStatus[isMuted="true"] { color: #ffd2b6; background: #52352d; }
		QLabel#fieldLabel { color: #c6d4df; font-size: 10px; font-weight: 700; letter-spacing: 1px; }
		QLabel#smallLabel { color: #8ba1b3; font-size: 10px; font-weight: 600; }
		QLabel#sourceWarning, QLabel#meterNotice { color: #f3c98f; font-size: 11px; }
		QLabel#levelValue { color: #a2eed7; font-size: 13px; font-weight: 700; }
		QLabel#routeState { color: #95b8ba; font-size: 10px; font-weight: 700; }
		QLabel#routeState[noTracks="true"] { color: #f4bc84; }
		QPushButton { background: #223345; color: #e9f4f6; border: 1px solid #3b5263; border-radius: 7px;
		              padding: 6px 9px; font-weight: 600; }
		QPushButton:hover { background: #2b4356; border-color: #75bfb5; }
		QPushButton:focus, QSlider:focus { outline: 1px solid #a0f4d5; }
		QPushButton:disabled { color: #6f8190; background: #192532; border-color: #2c3b48; }
		QPushButton#muteButton { background: #233544; }
		QPushButton#trackButton { padding: 3px; color: #a3b7c6; background: #101925; border: 1px solid #354858; }
		QPushButton#trackButton:checked { color: #092921; background: #8be2c7; border-color: #8be2c7; font-weight: 700; }
		QPushButton#trackButton:checked:hover { background: #b6f3df; }
		QPushButton#monitorButton { text-align: center; background: #1c2b3b; }
		QSlider::groove:horizontal { height: 4px; border-radius: 2px; background: #344858; }
		QSlider::sub-page:horizontal { background: #87e4c8; border-radius: 2px; }
		QSlider::handle:horizontal { width: 15px; margin: -6px 0; border-radius: 7px; background: #cbf8e9; border: 1px solid #6fd8bd; }
		QSlider::handle:horizontal:disabled { background: #586d76; border-color: #586d76; }
		QProgressBar#peakMeter { border: none; border-radius: 4px; background: #0c1722; }
		QProgressBar#peakMeter::chunk { border-radius: 4px; background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #52caa9,stop:0.70 #82e3bb,stop:0.88 #e1c46e,stop:1 #e98b77); }
		QScrollBar:vertical { background: #0b111c; width: 10px; }
		QScrollBar::handle:vertical { background: #405b68; min-height: 24px; border-radius: 4px; }
		QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
		QScrollBar:horizontal { background: #0b111c; height: 10px; }
		QScrollBar::handle:horizontal { background: #405b68; min-width: 24px; border-radius: 4px; }
		QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }
	)");

	auto *outer = new QVBoxLayout(this);
	outer->setContentsMargins(0, 0, 0, 0);
	auto *scroll = new QScrollArea(this);
	scroll->setObjectName("mixScroll");
	scroll->setWidgetResizable(true);
	scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	auto *body = new QWidget(scroll);
	body->setObjectName("mixBody");
	body->setMinimumWidth(232);
	auto *content = new QVBoxLayout(body);
	content->setContentsMargins(12, 12, 12, 12);
	content->setSpacing(11);

	auto *hero = new QFrame(body);
	hero->setObjectName("hero");
	auto *heroLayout = new QVBoxLayout(hero);
	heroLayout->setContentsMargins(16, 16, 16, 15);
	heroLayout->setSpacing(9);
	heroLayout->addWidget(makeLabel("A LITTLE MORE CONTROL", "eyebrow", hero));
	auto *brandRow = new QHBoxLayout;
	brandRow->setSpacing(10);
	brandRow->addWidget(new BrandMark(hero));
	auto *brandText = new QVBoxLayout;
	brandText->setSpacing(1);
	brandText->addWidget(makeLabel("MixBloom", "brand", hero));
	brandText->addWidget(makeLabel("Studio sound, beautifully routed.", "tagline", hero, true));
	brandRow->addLayout(brandText, 1);
	heroLayout->addLayout(brandRow);
	heroLayout->addWidget(makeLabel("YOUR OBS SOURCES  /  NO EXTRA HARDWARE", "heroFoot", hero, true));
	content->addWidget(hero);

	auto *noticeFrame = new QFrame(body);
	noticeFrame->setObjectName("noticeCard");
	auto *noticeLayout = new QVBoxLayout(noticeFrame);
	noticeLayout->setContentsMargins(12, 10, 12, 10);
	notice_ =
		makeLabel("Live controls: changes here immediately affect OBS audio, including streams and recordings.",
			  "notice", noticeFrame, true);
	notice_->setObjectName("notice");
	noticeLayout->addWidget(notice_);
	content->addWidget(noticeFrame);

	auto *section = new QHBoxLayout;
	auto *sectionText = new QVBoxLayout;
	sectionText->setSpacing(2);
	sectionText->addWidget(makeLabel("Your sources", "sectionTitle", body));
	count_ = makeLabel("", "sectionCaption", body, true);
	sectionText->addWidget(count_);
	section->addLayout(sectionText, 1);
	auto *refresh = new QPushButton("Refresh", body);
	refresh->setObjectName("refreshButton");
	refresh->setToolTip("Immediately refresh the list and current OBS values. It also refreshes automatically.");
	refresh->setMinimumHeight(34);
	section->addWidget(refresh, 0, Qt::AlignVCenter);
	content->addLayout(section);

	sourceList_ = new QWidget(body);
	sourceList_->setObjectName("sourceList");
	sourceLayout_ = new QVBoxLayout(sourceList_);
	sourceLayout_->setContentsMargins(0, 0, 0, 0);
	sourceLayout_->setSpacing(10);
	auto *emptyFrame = new QFrame(sourceList_);
	emptyFrame->setObjectName("emptyCard");
	auto *emptyLayout = new QVBoxLayout(emptyFrame);
	emptyLayout->setContentsMargins(16, 16, 16, 16);
	emptyLayout->addWidget(makeLabel("Start with a source", "sourceName", emptyFrame, true));
	emptyLayout->addWidget(makeLabel(
		"Add a microphone, desktop audio or another audio source in OBS. It will appear here automatically.",
		"sectionCaption", emptyFrame, true));
	empty_ = emptyFrame;
	sourceLayout_->addWidget(empty_);
	content->addWidget(sourceList_);
	content->addWidget(makeLabel(
		"Tracks must also be selected in OBS Settings → Output. A source can appear here even when it is not active in the current scene.",
		"footer", body, true));
	content->addStretch();
	scroll->setWidget(body);
	outer->addWidget(scroll);
	connect(refresh, &QPushButton::clicked, this, [this] { refreshNow(); });
}

void MixDock::connectCard(SourceCard *card)
{
	const QString id = qstr(card->source().uuid);
	connect(card->fader(), &QSlider::valueChanged, this, [this, id](int value) {
		if (suspended_)
			return;
		reportResult(mixbloom::setFader(utf8(id), value));
		/* Do not rebuild the card while the pointer is being dragged. */
	});
	connect(card->muteButton(), &QPushButton::clicked, this, [this, id, card] {
		if (suspended_)
			return;
		const bool next = !card->source().muted;
		reportResult(mixbloom::setMuted(utf8(id), next));
		refreshNow();
	});
	for (unsigned i = 1; i <= mixbloom::kTrackCount; ++i) {
		connect(card->trackButton(i), &QPushButton::clicked, this, [this, id, card, i](bool checked) {
			if (suspended_)
				return;
			const QString name = qstr(card->source().name);
			mixbloom::SourceIdentity identity(utf8(id));
			auto result = mixbloom::setTrack(utf8(id), i, checked);
			if (result == mixbloom::Result::ConfirmClearTracks) {
				QPointer<MixDock> guard(this);
				const bool confirmed = confirmAction(
					this, "Remove the last track?",
					QString("Remove %1 from all six OBS audio tracks?").arg(name),
					"This can remove the source from your stream or recording. You can cancel and keep the current routing.");
				if (!guard)
					return;
				if (!confirmed || suspended_ || !identity.stillCurrent()) {
					notice_->setText(
						"Routing unchanged. The source may have changed while confirming.");
					refreshNow();
					return;
				}
				result = mixbloom::setTrack(utf8(id), i, checked, true);
			}
			reportResult(result);
			refreshNow();
		});
	}
	connect(card->monitorButton(), &QPushButton::clicked, this, [this, id, card] {
		if (suspended_)
			return;
		const bool enable = !card->source().monitoring;
		const QString name = qstr(card->source().name);
		mixbloom::SourceIdentity identity(utf8(id));
		if (enable) {
			QPointer<MixDock> guard(this);
			const bool confirmed = confirmAction(
				this, "Enable audio monitoring?",
				QString("Send %1 to the OBS monitoring device?").arg(name),
				"Use headphones and check your monitoring device. Speakers or desktop capture can cause feedback. The source also remains on its selected output tracks.");
			if (!guard)
				return;
			if (!confirmed || suspended_ || !identity.stillCurrent()) {
				notice_->setText("Monitoring unchanged. Use headphones to avoid feedback.");
				refreshNow();
				return;
			}
		}
		reportResult(mixbloom::setMonitoring(utf8(id), enable));
		refreshNow();
	});
}

void MixDock::reportResult(mixbloom::Result result)
{
	switch (result) {
	case mixbloom::Result::Applied:
		notice_->setText("Updated in OBS. Changes here affect live streams and recordings.");
		break;
	case mixbloom::Result::Missing:
		notice_->setText("That source is no longer available. Refreshing the OBS source list.");
		break;
	case mixbloom::Result::Locked:
		notice_->setText("This source's level is locked in OBS. Unlock it in the OBS mixer to change it.");
		break;
	case mixbloom::Result::Unsupported:
		notice_->setText("OBS marks this source as unsafe to monitor. Monitoring was not enabled.");
		break;
	default:
		notice_->setText("The change was not applied. Check the source and try again.");
		break;
	}
}

void MixDock::refreshNow()
{
	if (suspended_)
		return;
	const auto sources = mixbloom::readSources();
	QSet<QString> seen;
	QStringList newOrder;
	for (const auto &source : sources) {
		const QString id = qstr(source.uuid);
		if (seen.contains(id))
			continue;
		seen.insert(id);
		newOrder.append(id);
		SourceCard *card = cards_.value(id, nullptr);
		if (!card) {
			card = new SourceCard(source, sourceList_);
			cards_.insert(id, card);
			connectCard(card);
		} else {
			card->apply(source);
		}
	}
	for (auto it = cards_.begin(); it != cards_.end();) {
		if (!seen.contains(it.key())) {
			SourceCard *card = it.value();
			sourceLayout_->removeWidget(card);
			card->hide();
			card->deleteLater();
			it = cards_.erase(it);
		} else {
			++it;
		}
	}
	if (newOrder != order_) {
		for (int i = 0; i < newOrder.size(); ++i)
			sourceLayout_->insertWidget(i, cards_.value(newOrder[i]));
		order_ = newOrder;
	}
	empty_->setVisible(newOrder.isEmpty());
	count_->setText(
		QString("%1 audio source%2  ·  auto-sync on").arg(newOrder.size()).arg(newOrder.size() == 1 ? "" : "s"));
}

void MixDock::updateMeters()
{
	if (suspended_)
		return;
	for (SourceCard *card : cards_)
		card->updateMeter();
}

void MixDock::clearCards()
{
	for (SourceCard *card : cards_) {
		sourceLayout_->removeWidget(card);
		delete card;
	}
	cards_.clear();
	order_.clear();
}

void MixDock::suspendForCollectionChange()
{
	suspended_ = true;
	/* Deleting a card can invalidate a sender during an active confirmation
	 * dialog. Disable and defer deletion; its tap is detached on destruction. */
	for (SourceCard *card : cards_) {
		sourceLayout_->removeWidget(card);
		card->setEnabled(false);
		card->hide();
		card->deleteLater();
	}
	cards_.clear();
	order_.clear();
	empty_->setVisible(false);
	count_->setText("Switching scene collection…");
	notice_->setText("Controls are paused while OBS changes scene collection.");
}

void MixDock::resumeAfterCollectionChange()
{
	suspended_ = false;
	notice_->setText("Live controls: changes here immediately affect OBS audio, including streams and recordings.");
	refreshNow();
}

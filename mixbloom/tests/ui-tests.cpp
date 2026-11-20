/*
MixBloom's real Qt dock against fake OBS; no stream, hardware or recording.
Copyright (C) 2026 Emmanuel Suah. GPL-2.0-or-later; no warranty.
*/
#include "mix-dock.hpp"
#include "fake-obs.hpp"
#include <obs-module.h>

#include <QApplication>
#include <QDialog>
#include <QEvent>
#include <QEventLoop>
#include <QFile>
#include <QFontMetrics>
#include <QFrame>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPluginLoader>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>
#include <QUrl>
#include <cmath>
#include <cstdio>
#include <functional>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

static int checks = 0, failures = 0;
static void check(bool value, const char *message)
{
	++checks;
	if (!value) {
		++failures;
		std::printf("FAIL  %s\n", message);
	} else
		std::printf("PASS  %s\n", message);
	std::fflush(stdout);
}

static QFrame *card(MixDock &dock, const QString &id)
{
	for (auto *frame : dock.findChildren<QFrame *>("sourceCard"))
		if (frame->property("sourceId").toString() == id && !frame->isHidden())
			return frame;
	return nullptr;
}

static QPushButton *track(QFrame *source, int number)
{
	for (auto *button : source->findChildren<QPushButton *>("trackButton"))
		if (button->property("trackNumber").toInt() == number)
			return button;
	return nullptr;
}

static void answerDialog(QMessageBox::StandardButton answer, const std::function<void()> &before = {})
{
	QTimer::singleShot(0, [answer, before] {
		for (QWidget *widget : QApplication::topLevelWidgets()) {
			auto *box = qobject_cast<QMessageBox *>(widget);
			if (box && box->isVisible()) {
				if (before)
					before();
				if (auto *button = box->button(answer))
					button->click();
				return;
			}
		}
	});
}

static void waitMs(int duration)
{
	QEventLoop loop;
	QTimer::singleShot(duration, &loop, &QEventLoop::quit);
	loop.exec();
}

/* Click an actual section link in the rendered guide, not just its handler. */
static bool clickGuideLink(QTextBrowser *browser, const QString &text, const QString &expectedLink)
{
	browser->verticalScrollBar()->setValue(0);
	QApplication::processEvents();
	QTextCursor cursor = browser->document()->find(text);
	if (cursor.isNull())
		return false;
	cursor.setPosition(cursor.selectionStart() + 2);
	const QPoint point = browser->cursorRect(cursor).center();
	if (!browser->viewport()->rect().contains(point) || browser->anchorAt(point) != expectedLink)
		return false;
	const QPoint global = browser->viewport()->mapToGlobal(point);
	QMouseEvent press(QEvent::MouseButtonPress, QPointF(point), QPointF(global), Qt::LeftButton, Qt::LeftButton,
			  Qt::NoModifier);
	QMouseEvent release(QEvent::MouseButtonRelease, QPointF(point), QPointF(global), Qt::LeftButton, Qt::NoButton,
			    Qt::NoModifier);
	QApplication::sendEvent(browser->viewport(), &press);
	QApplication::sendEvent(browser->viewport(), &release);
	QApplication::processEvents();
	return browser->verticalScrollBar()->value() > 0;
}

int main(int argc, char **argv)
{
#ifdef _WIN32
	if (!GetConsoleWindow()) {
		const HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE), err = GetStdHandle(STD_ERROR_HANDLE);
		if (AllocConsole()) {
			SetStdHandle(STD_OUTPUT_HANDLE, out);
			SetStdHandle(STD_ERROR_HANDLE, err);
		}
	}
	const QByteArray platformDir = qgetenv("QT_QPA_PLATFORM_PLUGIN_PATH");
	if (qgetenv("QT_QPA_PLATFORM") == "windows" && !platformDir.isEmpty()) {
		QPluginLoader platform(QString::fromLocal8Bit(platformDir) + "/qwindows.dll");
		if (!platform.load()) {
			std::fprintf(stderr, "Could not load Qt platform plugin: %s\n",
				     platform.errorString().toUtf8().constData());
			return 1;
		}
		platform.unload();
	}
#endif
	QApplication app(argc, argv);
	fakeReset();
	auto &mic = fakeAdd("mic", "Host microphone with a comfortably long descriptive name", OBS_SOURCE_AUDIO);
	auto &desktop = fakeAdd("desktop", "Desktop Audio", OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_SELF_MONITOR);
	desktop.tracks = 3;
	fakeAdd("video", "Camera without audio", OBS_SOURCE_VIDEO);
	{
		MixDock dock;
		dock.resize(300, 650);
		dock.show();
		app.processEvents();
		auto *micCard = card(dock, "mic");
		auto *desktopCard = card(dock, "desktop");
		check(micCard && desktopCard && !card(dock, "video"),
		      "audio sources appear; video-only source does not");
		check(fakeWrites() == 0, "opening and refreshing the dock does not change OBS audio");
		if (!micCard || !desktopCard)
			return 1;

		auto *help = dock.findChild<QPushButton *>("helpButton");
		check(help && help->isVisible() && help->isEnabled() && help->text().contains("Help") &&
			      help->accessibleName().contains("guide"),
		      "a visible, named Help button is available above the source list");
		check(QFile::exists(":/mixbloom/help/user-guide.html"), "the offline guide is embedded in the binary");
		if (!help)
			return 1;
		help->setFocus();
		QKeyEvent helpPress(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier);
		QKeyEvent helpRelease(QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier);
		QApplication::sendEvent(help, &helpPress);
		QApplication::sendEvent(help, &helpRelease);
		app.processEvents();
		auto *guide = dock.findChild<QDialog *>("mixBloomGuide");
		auto *browser = guide ? guide->findChild<QTextBrowser *>("guideContent") : nullptr;
		auto *guideClose = guide ? guide->findChild<QPushButton *>("guideClose") : nullptr;
		check(guide && guide->isVisible() && !guide->isModal() && browser && guideClose,
		      "keyboard activation opens a modeless user guide inside OBS");
		if (!guide || !browser || !guideClose)
			return 1;
		const QString manual = browser->toPlainText().toLower();
		check(manual.contains("quick start") && manual.contains("output tracks") &&
			      manual.contains("monitoring") && manual.contains("feedback") &&
			      manual.contains("troubleshooting") && manual.contains("install") &&
			      manual.contains("privacy"),
		      "the guide covers setup, controls, safety, troubleshooting, installation and limits");
		check(browser->verticalScrollBar()->maximum() > 0 && browser->isReadOnly(),
		      "the full user guide is scrollable and read-only");
		check(clickGuideLink(browser, "Monitoring", "#monitoring"),
		      "a section link navigates to its heading in the embedded guide");
		const int position = browser->verticalScrollBar()->value();
		const bool signalled = QMetaObject::invokeMethod(browser, "anchorClicked", Qt::DirectConnection,
								 Q_ARG(QUrl, QUrl("https://example.invalid/#privacy")));
		check(signalled && !browser->openLinks() && !browser->openExternalLinks() &&
			      browser->verticalScrollBar()->value() == position,
		      "external links are not followed by the offline guide");

		for (const QSize size : {QSize(300, 260), QSize(360, 320), QSize(650, 600)}) {
			guide->resize(size);
			app.processEvents();
			bool labelsFit = true;
			for (QLabel *label : guide->findChildren<QLabel *>()) {
				if (label->isVisible() && label->wordWrap() &&
				    label->height() < label->heightForWidth(label->width()))
					labelsFit = false;
			}
			const int closeTextWidth =
				QFontMetrics(guideClose->font()).horizontalAdvance(guideClose->text());
			check(guide->size() == size && labelsFit && guideClose->isVisible() &&
				      guideClose->width() >= closeTextWidth + 10 && browser->height() >= 75 &&
				      browser->horizontalScrollBar()->maximum() == 0 &&
				      browser->verticalScrollBar()->maximum() > 0,
			      size.width() == 300
				      ? "guide is usable without clipping at 300 × 260"
				      : (size.width() == 360 ? "guide is usable without clipping at 360 × 320"
							     : "guide is usable without clipping at 650 × 600"));
		}
		guideClose->click();
		check(!guide->isVisible(), "Close hides the user guide");
		help->click();
		app.processEvents();
		check(guide->isVisible() && dock.findChildren<QDialog *>("mixBloomGuide").size() == 1,
		      "Help reopens the same guide without creating duplicate windows");
		QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
		QApplication::sendEvent(browser, &escape);
		app.processEvents();
		check(!guide->isVisible(), "Escape closes the guide from its contents");
		check(fakeWrites() == 0, "opening, navigating and closing Help never writes OBS audio settings");

		auto *slider = micCard->findChild<QSlider *>("fader");
		auto *mute = micCard->findChild<QPushButton *>("muteButton");
		auto *monitor = micCard->findChild<QPushButton *>("monitorButton");
		auto *desktopMonitor = desktopCard->findChild<QPushButton *>("monitorButton");
		check(slider && mute && monitor && desktopMonitor && track(micCard, 1) && track(micCard, 6),
		      "all mixer, monitor and six routing controls exist");
		if (!slider || !mute || !monitor || !desktopMonitor || !track(micCard, 1) || !track(micCard, 6))
			return 1;
		check(!desktopMonitor->isEnabled(), "feedback-prone desktop monitoring is disabled");
		slider->setValue(500);
		check(std::fabs(mic.gain - 0.125f) < 0.000001f && desktop.gain == 1.0f,
		      "fader changes only its source");
		check(micCard->findChild<QLabel *>("levelValue")->text().startsWith("-18.1"),
		      "fader's dB value updates immediately");
		mute->click();
		check(mic.muted && mute->text() == "Unmute", "mute is applied and reflected immediately");
		mute->click();
		check(!mic.muted && mute->text() == "Mute", "unmute is applied and reflected");
		track(micCard, 2)->click();
		check(mic.tracks == 3, "a second track can be enabled");
		track(micCard, 1)->click();
		check(mic.tracks == 2, "a track can be removed when another remains");
		const int beforeCancel = fakeWrites();
		answerDialog(QMessageBox::No);
		track(micCard, 2)->click();
		check(mic.tracks == 2 && fakeWrites() == beforeCancel && track(micCard, 2)->isChecked(),
		      "cancelling last-track warning keeps the routing and button state");
		answerDialog(QMessageBox::Yes);
		track(micCard, 2)->click();
		check((mic.tracks & 63u) == 0 && micCard->findChild<QLabel *>("routeState")->text() == "NO TRACKS",
		      "confirmed last-track removal is visible");
		track(micCard, 1)->click();
		check(mic.tracks == 1, "a track can be restored");
		answerDialog(QMessageBox::No);
		monitor->click();
		check(!mic.monitoring, "monitoring confirmation can be cancelled");
		answerDialog(QMessageBox::Yes);
		monitor->click();
		check(mic.monitoring && monitor->text() == "Monitor: On", "monitoring enables only after confirmation");
		monitor->click();
		check(!mic.monitoring, "monitoring can be turned off without another confirmation");

		mic.locked = true;
		mic.gain = 2.0f;
		mic.muted = true;
		mic.tracks = 0x80000003u;
		dock.refreshNow();
		check(!slider->isEnabled() && mute->text() == "Unmute" && track(micCard, 2)->isChecked(),
		      "external OBS changes and fader lock synchronize into the UI");
		check(micCard->findChild<QLabel *>("levelValue")->text().startsWith("6.0") && mic.gain == 2.0f,
		      "external boost is displayed without silently clamping it");
		check(micCard->findChild<QLabel *>("sourceWarning")->text().contains("locked"),
		      "lock/boost warning is visible");
		fakeEmitPeak("mic", -28.0f, -12.0f);
		waitMs(160);
		check(micCard->findChild<QProgressBar *>("peakMeter")->value() == 80,
		      "audio-thread peak reaches the Qt meter through the timer");

		/* Narrow and wide docks must keep buttons legible; vertical scrolling
		 * keeps all controls reachable at a short dock height. */
		for (int width : {260, 340, 440}) {
			dock.resize(width, 500);
			app.processEvents();
			bool fits = true;
			for (QPushButton *button : dock.findChildren<QPushButton *>()) {
				if (button->isHidden())
					continue;
				const int textWidth = QFontMetrics(button->font()).horizontalAdvance(button->text());
				if (button->width() < textWidth + 10)
					fits = false;
			}
			check(fits, width == 260 ? "260px dock buttons do not clip"
						 : (width == 340 ? "340px dock buttons do not clip"
								 : "440px dock buttons do not clip"));
			QLabel *sourceName = micCard->findChild<QLabel *>("sourceName");
			check(sourceName->wordWrap() &&
				      sourceName->height() >= sourceName->heightForWidth(sourceName->width()),
			      "long source name wraps without clipping");
		}
		auto *scroll = dock.findChild<QScrollArea *>("mixScroll");
		check(scroll && scroll->widgetResizable() && scroll->verticalScrollBar()->maximum() > 0,
		      "short dock has a reachable, resizable vertical scroll area");
		if (scroll) {
			scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
			app.processEvents();
			check(help->isVisible() && help->mapTo(&dock, QPoint(0, 0)).y() < scroll->y(),
			      "Help stays visible when the source list is scrolled to the bottom");
		}

		/* A collection switch during a modal warning must not perform the
		 * pending write against a newly loaded source. */
		mic.locked = false;
		mic.monitoring = false;
		dock.refreshNow();
		const int beforeSwitch = fakeWrites();
		QPointer<QPushButton> guardedMonitor(monitor);
		bool senderRetained = false, metersDetached = false;
		answerDialog(QMessageBox::Yes, [&] {
			dock.suspendForCollectionChange();
			QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
			senderRetained = !guardedMonitor.isNull();
			metersDetached = fakeMeters() == 0;
		});
		monitor->click();
		check(senderRetained && metersDetached,
		      "collection cleanup keeps the modal sender alive but detaches meters");
		check(fakeWrites() == beforeSwitch && !mic.monitoring,
		      "scene switch cancels a pending monitoring action");
		dock.resumeAfterCollectionChange();
		app.processEvents();
		check(card(dock, "mic") != nullptr, "sources reappear after scene collection resumes");

		mic.name = "Renamed microphone";
		mic.muted = false;
		waitMs(1200);
		micCard = card(dock, "mic");
		check(micCard && micCard->findChild<QLabel *>("sourceName")->text() == "Renamed microphone",
		      "automatic refresh follows an external rename");
		desktop.removed = true;
		dock.refreshNow();
		app.processEvents();
		check(!card(dock, "desktop"), "removed OBS sources disappear from the dock");

		micCard = card(dock, "mic");
		monitor = micCard->findChild<QPushButton *>("monitorButton");
		FakeSource *replacement = nullptr;
		const int beforeReplacement = fakeWrites();
		answerDialog(QMessageBox::Yes, [&] {
			mic.removed = true;
			replacement = &fakeAdd("mic", "New source with reused ID", OBS_SOURCE_AUDIO);
			dock.refreshNow();
		});
		monitor->click();
		check(replacement && !replacement->monitoring && fakeWrites() == beforeReplacement,
		      "confirmation cannot modify a replacement source with the same UUID");
	}
	QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
	check(fakeMeters() == 0 && fakeAcquired() == fakeReleased(),
	      "closing the dock detaches meters and releases references");
	fakeReset();
	QPointer<QDialog> emptyGuide;
	{
		MixDock emptyDock;
		emptyDock.resize(320, 600);
		emptyDock.show();
		app.processEvents();
		auto *empty = emptyDock.findChild<QFrame *>("emptyCard");
		check(empty && empty->isVisible() && fakeWrites() == 0, "empty setup explains how to add a source");
		auto *emptyHelp = emptyDock.findChild<QPushButton *>("helpButton");
		if (emptyHelp)
			emptyHelp->click();
		app.processEvents();
		emptyGuide = emptyDock.findChild<QDialog *>("mixBloomGuide");
		auto *emptyContent = emptyGuide ? emptyGuide->findChild<QTextBrowser *>("guideContent") : nullptr;
		check(emptyHelp && emptyHelp->isEnabled() && emptyGuide && emptyGuide->isVisible() && emptyContent &&
			      emptyContent->toPlainText().contains("Quick start") && fakeWrites() == 0,
		      "the offline guide opens with no audio sources and makes no writes");
		emptyDock.suspendForCollectionChange();
		check(emptyHelp && emptyHelp->isEnabled() && emptyGuide && emptyGuide->isVisible() && fakeWrites() == 0,
		      "Help remains available while an OBS scene collection is changing");
		emptyDock.resumeAfterCollectionChange();
		auto &newSource = fakeAdd("new", "New microphone", OBS_SOURCE_AUDIO);
		waitMs(1200);
		check(card(emptyDock, "new") && !empty->isVisible(), "new OBS audio sources appear automatically");
		newSource.removed = true;
		waitMs(1200);
		check(!card(emptyDock, "new") && empty->isVisible(),
		      "empty state returns when the last source disappears");
	}
	QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
	check(!emptyGuide && fakeWrites() == 0, "destroying the dock also destroys its guide without audio writes");
	fakeReset();

	/* A clean representative fixture for the CI screenshot. */
	fakeAdd("mic", "Studio microphone", OBS_SOURCE_AUDIO).gain = 0.55f;
	fakeAdd("desktop", "Desktop audio", OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_SELF_MONITOR).tracks = 3;
	{
		MixDock dock;
		dock.resize(420, 1100);
		dock.show();
		fakeEmitPeak("mic", -18.0f, -14.0f);
		fakeEmitPeak("desktop", -26.0f, -23.0f);
		waitMs(140);
		const QByteArray screenshot = qgetenv("MIXBLOOM_UI_SCREENSHOT");
		if (!screenshot.isEmpty())
			check(dock.grab().save(QString::fromUtf8(screenshot)),
			      "representative dock screenshot captured");
		const QByteArray helpScreenshot = qgetenv("MIXBLOOM_HELP_SCREENSHOT");
		if (!helpScreenshot.isEmpty()) {
			auto *help = dock.findChild<QPushButton *>("helpButton");
			if (help)
				help->click();
			app.processEvents();
			auto *guide = dock.findChild<QDialog *>("mixBloomGuide");
			check(guide && guide->isVisible() && guide->grab().save(QString::fromUtf8(helpScreenshot)),
			      "representative offline guide screenshot captured");
		}
	}
	QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
	fakeReset();
	std::printf("\n%d Qt UI checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}

/*
MixBloom for OBS Studio
Copyright (C) 2026 Emmanuel Suah
Licensed under GPL-2.0-or-later; see LICENSE. No warranty.
*/
#include "mix-help.hpp"

#include <QFile>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QSizePolicy>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTextOption>
#include <QUrl>
#include <QVBoxLayout>

MixHelpDialog::MixHelpDialog(QWidget *parent) : QDialog(parent)
{
	setObjectName("mixBloomGuide");
	setWindowTitle("MixBloom user guide");
	setAccessibleName("MixBloom user guide");
	setModal(false);
	setMinimumSize(300, 260);
	setStyleSheet(R"(
		QDialog#mixBloomGuide { background: #0b111c; }
		QWidget { color: #e5edf5; font-family: 'Segoe UI'; font-size: 13px; }
		QFrame#guideHeader { background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #1b4748,stop:1 #162639);
		                     border-bottom: 1px solid #386063; }
		QLabel#guideEyebrow { color: #9debd2; font-size: 10px; font-weight: 700; letter-spacing: 1px; }
		QLabel#guideTitle { color: #f4fffb; font-size: 22px; font-weight: 700; }
		QLabel#guideSubtitle { color: #c5dbdb; font-size: 12px; }
		QTextBrowser#guideContent { background: #0f1a28; color: #e5edf5; border: 0; }
		QFrame#guideFooter { background: #111e2a; border-top: 1px solid #2b4351; }
		QLabel#guideFooterText { color: #a9c9c5; font-size: 11px; font-weight: 600; }
		QPushButton#guideClose { background: #a0ecd4; color: #102d2b; border: 1px solid #a0ecd4;
		                         border-radius: 7px; padding: 7px 16px; font-weight: 700; }
		QPushButton#guideClose:hover { background: #c0f7e5; }
		QPushButton#guideClose:focus { border: 2px solid #ffffff; }
		QScrollBar:vertical { background: #0b111c; width: 12px; }
		QScrollBar::handle:vertical { background: #496b74; min-height: 28px; border-radius: 5px; }
		QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
		QScrollBar:horizontal { background: #0b111c; height: 12px; }
		QScrollBar::handle:horizontal { background: #496b74; min-width: 28px; border-radius: 5px; }
		QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }
	)");

	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	auto *header = new QFrame(this);
	header->setObjectName("guideHeader");
	auto *headerLayout = new QVBoxLayout(header);
	headerLayout->setContentsMargins(22, 17, 22, 15);
	headerLayout->setSpacing(3);
	auto addHeaderLabel = [header, headerLayout](const QString &text, const char *name) {
		auto *label = new QLabel(text, header);
		label->setObjectName(name);
		label->setTextFormat(Qt::PlainText);
		label->setWordWrap(true);
		label->setMinimumWidth(0);
		QSizePolicy policy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		policy.setHeightForWidth(true);
		label->setSizePolicy(policy);
		headerLayout->addWidget(label);
	};
	addHeaderLabel("MIXBLOOM  /  HELP", "guideEyebrow");
	addHeaderLabel("Get to know your mix.", "guideTitle");
	addHeaderLabel("Practical help, available offline.", "guideSubtitle");
	layout->addWidget(header);

	auto *browser = new QTextBrowser(this);
	browser->setObjectName("guideContent");
	browser->setAccessibleName("MixBloom user guide contents");
	browser->setAccessibleDescription("Offline instructions. Select a section link or scroll to read.");
	browser->setMinimumHeight(75);
	browser->setLineWrapMode(QTextEdit::WidgetWidth);
	browser->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
	browser->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	browser->setOpenExternalLinks(false);
	browser->setOpenLinks(false);
	QFile guide(QStringLiteral(":/mixbloom/help/user-guide.html"));
	if (guide.open(QIODevice::ReadOnly)) {
		browser->setHtml(QString::fromUtf8(guide.readAll()));
	} else {
		browser->setHtml("<h1>Guide unavailable</h1><p>Please read the README or "
				 "data/help/user-guide.html from the MixBloom package.</p>");
	}
	browser->document()->setDocumentMargin(18);
	/* Only navigate within this embedded document. The guide does not open a
	 * browser, access the network, or follow filesystem/external URLs. */
	connect(browser, &QTextBrowser::anchorClicked, this, [browser](const QUrl &url) {
		if (url.isRelative() && url.path().isEmpty() && url.hasFragment())
			browser->scrollToAnchor(url.fragment());
	});
	layout->addWidget(browser, 1);

	auto *footer = new QFrame(this);
	footer->setObjectName("guideFooter");
	auto *footerLayout = new QHBoxLayout(footer);
	footerLayout->setContentsMargins(18, 10, 18, 10);
	footerLayout->setSpacing(10);
	auto *footerText = new QLabel("OFFLINE  ·  SAFE TO READ", footer);
	footerText->setObjectName("guideFooterText");
	footerText->setTextFormat(Qt::PlainText);
	footerText->setWordWrap(true);
	footerText->setMinimumWidth(0);
	QSizePolicy footerPolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
	footerPolicy.setHeightForWidth(true);
	footerText->setSizePolicy(footerPolicy);
	footerLayout->addWidget(footerText, 1);
	auto *close = new QPushButton("Close", footer);
	close->setObjectName("guideClose");
	close->setAccessibleName("Close the MixBloom user guide");
	close->setMinimumHeight(36);
	footerLayout->addWidget(close);
	connect(close, &QPushButton::clicked, this, &QDialog::close);
	layout->addWidget(footer);

	/* Keep the first opening within small laptop displays, while allowing the
	 * reader to resize the window and scroll at narrower sizes. */
	const QScreen *screen = parent ? parent->screen() : QGuiApplication::primaryScreen();
	const QRect available = screen ? screen->availableGeometry() : QRect(0, 0, 800, 800);
	resize(qBound(300, available.width() - 40, 720), qBound(260, available.height() - 80, 700));
}

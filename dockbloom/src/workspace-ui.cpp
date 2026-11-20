/*
DockBloom for OBS Studio
Copyright (C) 2026 Emmanuel Suah
GPL-2.0-or-later; see LICENSE. No warranty.
*/
#include "workspace-ui.hpp"
#include "workspace.hpp"

#include <QApplication>
#include <QDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTextBrowser>
#include <QTimer>
#include <QVBoxLayout>

namespace dockbloom {
namespace {
/* The stylesheet supplies the contrasting indicator background; draw an
 * explicit tick as well, so selection is not conveyed by color alone. */
class DockItemDelegate final : public QStyledItemDelegate {
public:
	explicit DockItemDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
	{
		QStyledItemDelegate::paint(painter, option, index);
		if (index.data(Qt::CheckStateRole).toInt() != Qt::Checked)
			return;
		QStyleOptionViewItem styled(option);
		initStyleOption(&styled, index);
		QStyle *style = styled.widget ? styled.widget->style() : QApplication::style();
		const QRectF box = style->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &styled, styled.widget);
		if (box.isEmpty())
			return;
		QPainterPath tick;
		tick.moveTo(box.left() + box.width() * 0.24, box.top() + box.height() * 0.51);
		tick.lineTo(box.left() + box.width() * 0.43, box.top() + box.height() * 0.71);
		tick.lineTo(box.left() + box.width() * 0.77, box.top() + box.height() * 0.31);
		painter->save();
		painter->setRenderHint(QPainter::Antialiasing);
		painter->setPen(QPen(QColor("#241b3b"), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
		painter->drawPath(tick);
		painter->restore();
	}
};

QLabel *label(const QString &text, const char *name, QWidget *parent, bool wrap = false)
{
	auto *result = new QLabel(text, parent);
	result->setObjectName(name);
	result->setTextFormat(Qt::PlainText);
	result->setWordWrap(wrap);
	if (wrap) {
		result->setMinimumWidth(0);
		QSizePolicy policy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		policy.setHeightForWidth(true);
		result->setSizePolicy(policy);
	}
	return result;
}

QPushButton *button(const QString &text, const char *name, QWidget *parent)
{
	auto *result = new QPushButton(text, parent);
	result->setObjectName(name);
	result->setMinimumHeight(36);
	return result;
}

bool ask(QWidget *parent, const QString &title, const QString &text)
{
	/* OBS may destroy a dock while the modal event loop is running. A heap
	 * dialog with a guard avoids a parent deleting a stack-allocated widget. */
	QPointer<QMessageBox> box =
		new QMessageBox(QMessageBox::Warning, title, text, QMessageBox::Yes | QMessageBox::No, parent);
	box->setTextFormat(Qt::PlainText);
	box->setDefaultButton(QMessageBox::No);
	const int result = box->exec();
	if (box)
		delete box.data();
	return result == QMessageBox::Yes;
}
} // namespace

WorkspaceUi::WorkspaceUi(Workspace *workspace, QWidget *parent) : QWidget(parent), workspace_(workspace)
{
	buildUi();
	refresh();
}

void WorkspaceUi::buildUi()
{
	setObjectName("dockBloomRoot");
	setStyleSheet(R"(
		QWidget#dockBloomRoot, QWidget#dockBloomBody { background: #0e1221; }
		QWidget { color: #e7e9f6; font-family: 'Segoe UI'; font-size: 12px; }
		QScrollArea { border: 0; background: transparent; }
		QFrame#hero { background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #34315a,stop:1 #192a43);
		              border: 1px solid #555681; border-radius: 13px; }
		QFrame#noticeCard { background: #282a35; border: 1px solid #645c58; border-radius: 9px; }
		QLabel#eyebrow { color: #c6b7ff; font-size: 10px; font-weight: 700; letter-spacing: 1px; }
		QLabel#brand { color: #fbf9ff; font-size: 23px; font-weight: 700; }
		QLabel#tagline, QLabel#caption, QLabel#footer { color: #bec8db; font-size: 12px; }
		QLabel#sectionTitle { color: #f8f7ff; font-size: 16px; font-weight: 700; }
		QLabel#notice { color: #ffe0ae; font-size: 12px; }
		QLabel#pageLabel { color: #cbbdff; font-weight: 700; }
		QPushButton { background: #252e44; color: #eef0ff; border: 1px solid #46536d;
		              border-radius: 7px; padding: 6px 9px; font-weight: 600; }
		QPushButton:hover { background: #343f5d; border-color: #b3a3f4; }
		QPushButton:focus { border: 2px solid #f5eeff; }
		QPushButton:disabled { color: #788397; background: #192131; border-color: #303a4a; }
		QPushButton#focusButton, QPushButton#gridButton, QPushButton#helpButton {
		    background: #cbbcff; color: #211b3b; border: 1px solid #cbbcff; font-weight: 700; }
		QPushButton#focusButton:hover, QPushButton#gridButton:hover, QPushButton#helpButton:hover { background: #e0d8ff; }
		QLineEdit, QListWidget, QTextBrowser { background: #111a2a; color: #edf0ff; border: 1px solid #35435b;
		                                      border-radius: 7px; padding: 6px; selection-background-color: #635790; }
		QListWidget::item { padding: 6px 3px; border-radius: 4px; }
		QListWidget::item:selected { background: #514a77; color: #ffffff; }
		QListWidget::indicator { width: 17px; height: 17px; }
		QListWidget::indicator:unchecked { background: #0e1727; border: 1px solid #7c8aa4; border-radius: 4px; }
		QListWidget::indicator:checked { background: #cbbcff; border: 1px solid #cbbcff; border-radius: 4px; }
		QDialog, QMessageBox { background: #151c2c; }
		QLabel#dialogTitle { color: #fbf9ff; font-size: 19px; font-weight: 700; }
		QScrollBar:vertical { background: #101727; width: 11px; }
		QScrollBar::handle:vertical { background: #59617d; min-height: 26px; border-radius: 5px; }
		QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
		QScrollBar:horizontal { background: #101727; height: 11px; }
		QScrollBar::handle:horizontal { background: #59617d; min-width: 26px; border-radius: 5px; }
		QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }
	)");
	auto *outer = new QVBoxLayout(this);
	outer->setContentsMargins(0, 0, 0, 0);
	auto *scroll = new QScrollArea(this);
	scroll->setObjectName("workspaceScroll");
	scroll->setWidgetResizable(true);
	scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	auto *body = new QWidget(scroll);
	body->setObjectName("dockBloomBody");
	body->setMinimumWidth(240);
	auto *content = new QVBoxLayout(body);
	content->setContentsMargins(12, 12, 12, 12);
	content->setSpacing(10);

	auto *hero = new QFrame(body);
	hero->setObjectName("hero");
	auto *heroLayout = new QVBoxLayout(hero);
	heroLayout->setContentsMargins(15, 14, 15, 14);
	heroLayout->setSpacing(5);
	auto *heroTop = new QHBoxLayout;
	heroTop->addWidget(label("YOUR DOCKS, TOGETHER", "eyebrow", hero, true), 1);
	auto *help = button("?  Help", "helpButton", hero);
	help->setAccessibleName("Open the DockBloom user guide");
	help->setToolTip("Open the offline DockBloom guide.");
	heroTop->addWidget(help, 0, Qt::AlignTop);
	heroLayout->addLayout(heroTop);
	heroLayout->addWidget(label("DockBloom", "brand", hero));
	heroLayout->addWidget(label("A calmer space for every part of your show.", "tagline", hero, true));
	content->addWidget(hero);

	auto *noticeFrame = new QFrame(body);
	noticeFrame->setObjectName("noticeCard");
	auto *noticeLayout = new QVBoxLayout(noticeFrame);
	noticeLayout->setContentsMargins(11, 9, 11, 9);
	notice_ = label(
		"Arranging moves OBS docks. Grid hides docks on other pages; restore your previous layout when needed.",
		"notice", noticeFrame, true);
	noticeLayout->addWidget(notice_);
	content->addWidget(noticeFrame);

	auto *section = new QHBoxLayout;
	auto *sectionText = new QVBoxLayout;
	sectionText->setSpacing(2);
	sectionText->addWidget(label("Your docks", "sectionTitle", body));
	count_ = label("", "caption", body, true);
	sectionText->addWidget(count_);
	section->addLayout(sectionText, 1);
	auto *refreshButton = button("Refresh", "refreshButton", body);
	refreshButton->setToolTip("Find OBS and plug-in docks that are currently available.");
	section->addWidget(refreshButton, 0, Qt::AlignVCenter);
	content->addLayout(section);

	search_ = new QLineEdit(body);
	search_->setObjectName("dockSearch");
	search_->setPlaceholderText("Find a dock…");
	search_->setAccessibleName("Search available docks");
	content->addWidget(search_);
	list_ = new QListWidget(body);
	list_->setObjectName("dockList");
	list_->setAccessibleName("Available docks; check to include in the workspace");
	list_->setItemDelegate(new DockItemDelegate(list_));
	list_->setWordWrap(true);
	list_->setTextElideMode(Qt::ElideNone);
	list_->setResizeMode(QListView::Adjust);
	list_->setMinimumHeight(180);
	list_->setMaximumHeight(280);
	list_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	content->addWidget(list_);
	content->addWidget(
		label("Check docks to include them. Unchecked docks stay where they are until you restore the layout.",
		      "caption", body, true));

	auto *webRow = new QHBoxLayout;
	auto *addWeb = button("+ Add web", "addWebButton", body);
	addWeb->setAccessibleName("Add a web dock by URL");
	removeWeb_ = button("Remove web", "removeWebButton", body);
	removeWeb_->setToolTip("Remove the selected DockBloom web dock, not an existing OBS dock.");
	webRow->addWidget(addWeb, 1);
	webRow->addWidget(removeWeb_);
	content->addLayout(webRow);

	content->addWidget(label("Arrange workspace", "sectionTitle", body));
	content->addWidget(label("Focus shows one dock. Grid shows up to four at a time; use the pages for the rest.",
				 "caption", body, true));
	auto *views = new QHBoxLayout;
	auto *focus = button("Focus view", "focusButton", body);
	auto *grid = button("Grid view", "gridButton", body);
	views->addWidget(focus, 1);
	views->addWidget(grid, 1);
	content->addLayout(views);
	auto *pages = new QHBoxLayout;
	previous_ = button("Previous", "previousPage", body);
	page_ = label("", "pageLabel", body);
	page_->setAlignment(Qt::AlignCenter);
	next_ = button("Next", "nextPage", body);
	pages->addWidget(previous_);
	pages->addWidget(page_, 1);
	pages->addWidget(next_);
	content->addLayout(pages);
	auto *show = button("Show selected dock", "showSelected", body);
	show->setToolTip("Show the selected included dock, changing grid page if needed.");
	content->addWidget(show);
	restore_ = button("Restore previous OBS layout", "restoreButton", body);
	content->addWidget(restore_);
	content->addWidget(label("Only trusted web pages should be added. Addresses are saved locally as plain text.",
				 "footer", body, true));
	content->addStretch();
	scroll->setWidget(body);
	outer->addWidget(scroll);

	connect(help, &QPushButton::clicked, this, [this] { showGuide(); });
	connect(refreshButton, &QPushButton::clicked, this, [this] { refresh(); });
	connect(search_, &QLineEdit::textChanged, this, [this](const QString &text) {
		for (int i = 0; i < list_->count(); ++i)
			list_->item(i)->setHidden(!list_->item(i)->text().contains(text, Qt::CaseInsensitive));
	});
	connect(list_, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) { onItemChanged(item); });
	connect(list_, &QListWidget::currentItemChanged, this, [this] {
		QListWidgetItem *item = list_->currentItem();
		removeWeb_->setEnabled(item && item->data(Qt::UserRole + 1).toBool());
	});
	connect(addWeb, &QPushButton::clicked, this, [this] { addWebDialog(); });
	connect(removeWeb_, &QPushButton::clicked, this, [this] { removeSelectedWeb(); });
	connect(focus, &QPushButton::clicked, this, [this] { applyView("focus", 0, selectedId(), true); });
	connect(grid, &QPushButton::clicked, this, [this] { applyView("grid", 0, selectedId(), true); });
	const auto currentPage = [this] {
		if (!workspace_)
			return 0;
		const int count = static_cast<int>(workspace_->availableMembers().size());
		const int last = qMax(0, (count - 1) / kGridPageSize);
		return qBound(0, workspace_->config().page, last);
	};
	connect(previous_, &QPushButton::clicked, this, [this, currentPage] {
		if (workspace_)
			applyView("grid", currentPage() - 1, selectedId(), false);
	});
	connect(next_, &QPushButton::clicked, this, [this, currentPage] {
		if (workspace_)
			applyView("grid", currentPage() + 1, selectedId(), false);
	});
	connect(show, &QPushButton::clicked, this, [this] { showSelected(); });
	connect(restore_, &QPushButton::clicked, this, [this] { restoreLayout(); });
}

QString WorkspaceUi::selectedId() const
{
	const auto *item = list_->currentItem();
	return item ? item->data(Qt::UserRole).toString() : QString();
}

void WorkspaceUi::setNotice(const QString &text, bool warning)
{
	notice_->setText(text);
	notice_->setStyleSheet(warning ? "color: #ffd2b2;" : "color: #ffe0ae;");
}

void WorkspaceUi::showError(const QString &message)
{
	setNotice(message, true);
}

void WorkspaceUi::refresh()
{
	if (!workspace_)
		return;
	const QString selected = selectedId();
	const auto docks = workspace_->availableDocks();
	const Config config = workspace_->config();
	QSet<QString> present;
	const QSignalBlocker blocker(list_);
	list_->clear();
	for (const DockInfo &info : docks) {
		present.insert(info.id);
		auto *item = new QListWidgetItem(info.title + (info.web ? "  ·  Web" : "  ·  OBS"), list_);
		item->setData(Qt::UserRole, info.id);
		item->setData(Qt::UserRole + 1, info.web);
		item->setToolTip(info.title + "\nDock ID: " + info.id);
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
		item->setCheckState(config.members.contains(info.id) ? Qt::Checked : Qt::Unchecked);
		item->setHidden(!item->text().contains(search_->text(), Qt::CaseInsensitive));
		if (info.id == selected)
			list_->setCurrentItem(item);
	}
	QStringList unavailable = config.members;
	for (const WebDock &web : config.webDocks)
		if (!unavailable.contains(web.id))
			unavailable.append(web.id);
	for (const QString &id : unavailable) {
		if (present.contains(id))
			continue;
		bool web = false;
		QString name = id;
		for (const WebDock &entry : config.webDocks) {
			if (entry.id == id) {
				web = true;
				name = entry.title;
				break;
			}
		}
		auto *item = new QListWidgetItem("Unavailable: " + name + (web ? "  ·  Web" : ""), list_);
		item->setData(Qt::UserRole, id);
		item->setData(Qt::UserRole + 1, web);
		item->setToolTip(
			web ? "This saved web dock could not be registered. Select it and choose Remove web if needed."
			    : "This dock is not currently available in OBS. Uncheck it to remove it from the workspace.");
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
		item->setCheckState(config.members.contains(id) ? Qt::Checked : Qt::Unchecked);
		item->setHidden(!item->text().contains(search_->text(), Qt::CaseInsensitive));
		if (id == selected)
			list_->setCurrentItem(item);
	}
	const int availableCount = workspace_->availableMembers().size();
	count_->setText(QString("%1 included  ·  %2 available").arg(config.members.size()).arg(availableCount));
	const int pages = qMax(1, (availableCount + kGridPageSize - 1) / kGridPageSize);
	const int current = qBound(0, config.page, pages - 1);
	page_->setText(QString("%1 / %2").arg(current + 1).arg(pages));
	const bool gridActive = config.mode == "grid" && workspace_->hasBackup();
	previous_->setEnabled(gridActive && current > 0);
	next_->setEnabled(gridActive && current + 1 < pages);
	restore_->setEnabled(workspace_->hasBackup());
	QListWidgetItem *item = list_->currentItem();
	removeWeb_->setEnabled(item && item->data(Qt::UserRole + 1).toBool());
}

void WorkspaceUi::onItemChanged(QListWidgetItem *item)
{
	if (!workspace_ || !item)
		return;
	const bool selected = item->checkState() == Qt::Checked;
	QString error;
	if (!workspace_->setMember(item->data(Qt::UserRole).toString(), selected, &error)) {
		const QSignalBlocker blocker(list_);
		item->setCheckState(selected ? Qt::Unchecked : Qt::Checked);
		showError(error);
		return;
	}
	setNotice("Selection saved. Choose Focus or Grid to arrange these docks.");
	/* Do not destroy the item while QListWidget is still emitting its change. */
	QTimer::singleShot(0, this, [this] { refresh(); });
}

void WorkspaceUi::applyView(const QString &mode, int page, const QString &focusedId, bool askIfNeeded)
{
	if (!workspace_)
		return;
	if (askIfNeeded && !workspace_->hasBackup()) {
		QPointer<WorkspaceUi> guard(this);
		const bool yes = ask(this, "Arrange OBS docks?",
				     "This moves the selected OBS docks into one workspace. Focus shows one; "
				     "Grid shows up to four and hides other selected docks. An OBS layout backup "
				     "will be saved first. You can restore it later. Continue?");
		if (!guard || !yes || !workspace_)
			return;
	}
	QString error;
	QPointer<WorkspaceUi> guard(this);
	const bool applied = workspace_->apply(mode, page, focusedId, &error);
	if (!guard)
		return;
	if (!applied) {
		showError(error.isEmpty() ? "The layout could not be applied." : error);
		return;
	}
	setNotice(mode == "grid" ? "Grid applied. Other selected docks are on the next pages."
				 : "Focus applied. Select a dock and choose Show selected dock.");
	refresh();
}

void WorkspaceUi::showSelected()
{
	if (!workspace_)
		return;
	const QString id = selectedId();
	const QStringList members = workspace_->availableMembers();
	const int index = members.indexOf(id);
	if (index < 0) {
		showError("Select an included, available dock in the list first.");
		return;
	}
	const QString mode = workspace_->config().mode;
	applyView(mode, mode == "grid" ? index / kGridPageSize : 0, id, true);
}

void WorkspaceUi::restoreLayout()
{
	if (!workspace_ || !workspace_->hasBackup())
		return;
	QPointer<WorkspaceUi> guard(this);
	const bool yes = ask(this, "Restore previous OBS layout?",
			     "This restores the entire OBS dock layout captured before DockBloom first arranged it. "
			     "Later layout changes may be overwritten; docks added or removed since then may need "
			     "manual adjustment. Continue?");
	if (!guard || !yes || !workspace_)
		return;
	QString error;
	const bool restored = workspace_->restore(&error);
	if (!guard)
		return;
	if (!restored) {
		showError(error);
		return;
	}
	setNotice(error.isEmpty() ? "Previous OBS dock layout restored. Your workspace list is still saved." : error,
		  !error.isEmpty());
	refresh();
}

void WorkspaceUi::addWebDialog()
{
	if (!workspace_)
		return;
	if (!workspace_->browserAvailable()) {
		showError("The OBS browser component is unavailable. Enable OBS Browser and restart OBS.");
		return;
	}
	if (addDialog_) {
		addDialog_->show();
		addDialog_->raise();
		addDialog_->activateWindow();
		return;
	}
	auto *dialog = new QDialog(this);
	addDialog_ = dialog;
	dialog->setObjectName("addWebDialog");
	dialog->setWindowTitle("Add a web dock");
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	dialog->setMinimumWidth(300);
	dialog->resize(460, 300);
	auto *layout = new QVBoxLayout(dialog);
	layout->setContentsMargins(20, 20, 20, 20);
	layout->setSpacing(9);
	layout->addWidget(label("Add a web dock", "dialogTitle", dialog, true));
	layout->addWidget(label("Name", "caption", dialog));
	auto *name = new QLineEdit(dialog);
	name->setObjectName("webName");
	name->setAccessibleName("Web dock name");
	name->setMaxLength(100);
	name->setPlaceholderText("For example, stream chat");
	layout->addWidget(name);
	layout->addWidget(label("Web address", "caption", dialog));
	auto *url = new QLineEdit(dialog);
	url->setObjectName("webUrl");
	url->setAccessibleName("Web dock URL");
	url->setMaxLength(4096);
	url->setPlaceholderText("https://example.com");
	layout->addWidget(url);
	layout->addWidget(label("Only use trusted sites. Pages can connect to the internet and use browser cookies. "
				"Addresses, including query tokens, are saved locally in plain text.",
				"caption", dialog, true));
	auto *errorLabel = label("", "webError", dialog, true);
	layout->addWidget(errorLabel);
	auto *actions = new QHBoxLayout;
	actions->addStretch();
	auto *cancel = button("Cancel", "webCancel", dialog);
	auto *add = button("Add dock", "webConfirm", dialog);
	actions->addWidget(cancel);
	actions->addWidget(add);
	layout->addLayout(actions);
	connect(cancel, &QPushButton::clicked, dialog, &QDialog::close);
	connect(add, &QPushButton::clicked, dialog, [this, dialog, name, url, errorLabel] {
		if (!workspace_)
			return;
		QPointer<WorkspaceUi> guard(this);
		QPointer<QDialog> dialogGuard(dialog);
		QString error, id;
		const bool added = workspace_->addWebDock(name->text(), url->text(), &id, &error);
		if (!guard || !dialogGuard)
			return;
		if (!added) {
			errorLabel->setText(error);
			return;
		}
		setNotice("Web dock added to the list. Choose Focus or Grid to arrange it.");
		refresh();
		for (int i = 0; i < list_->count(); ++i)
			if (list_->item(i)->data(Qt::UserRole).toString() == id)
				list_->setCurrentRow(i);
		dialog->close();
	});
	dialog->show();
}

void WorkspaceUi::removeSelectedWeb()
{
	if (!workspace_ || !list_->currentItem() || !list_->currentItem()->data(Qt::UserRole + 1).toBool())
		return;
	const QString id = selectedId();
	QPointer<WorkspaceUi> guard(this);
	const bool yes = ask(this, "Remove web dock?",
			     "This closes and removes this DockBloom web dock and its saved address. Browser cookies "
			     "may remain in OBS's browser data. Continue?");
	if (!guard || !yes || !workspace_)
		return;
	QString error;
	const bool removed = workspace_->removeWebDock(id, &error);
	if (!guard)
		return;
	if (!removed) {
		showError(error);
		return;
	}
	setNotice("Web dock removed. Existing OBS docks were not deleted.");
	refresh();
}

void WorkspaceUi::showGuide()
{
	if (!guide_) {
		auto *dialog = new QDialog(this);
		guide_ = dialog;
		dialog->setObjectName("guideDialog");
		dialog->setWindowTitle("DockBloom user guide");
		dialog->setMinimumSize(300, 260);
		dialog->resize(600, 600);
		auto *layout = new QVBoxLayout(dialog);
		layout->setContentsMargins(18, 18, 18, 18);
		layout->addWidget(label("DockBloom user guide", "dialogTitle", dialog, true));
		auto *text = new QTextBrowser(dialog);
		text->setObjectName("guideText");
		text->setAccessibleName("DockBloom offline user guide");
		text->setOpenLinks(false);
		text->setOpenExternalLinks(false);
		text->setHtml(QStringLiteral(
			"<h2>One workspace, many docks</h2>"
			"<p>DockBloom arranges existing OBS, plug-in and custom browser docks, and can add web "
			"docks by URL. The list supports up to 256 included docks, including up to 100 web docks.</p>"
			"<h3>Get started</h3><ol><li>Open <b>Docks → DockBloom</b>. Use Refresh if a dock is missing.</li>"
			"<li>Check the docks to include. Search or scroll the list. The selection is saved locally.</li>"
			"<li>Choose <b>Focus view</b> for one dock, or <b>Grid view</b> for up to four per page.</li>"
			"<li>Select an included dock and choose <b>Show selected dock</b>; use Previous and Next "
			"in Grid view.</li></ol>"
			"<h3>Before rearranging</h3><p>DockBloom moves and shows or hides the selected native OBS "
			"docks. It does not change scenes, sources, audio or stream settings. Make layout changes "
			"before going live. Unchecked docks stay where they are; they are not automatically restored.</p>"
			"<h3>Restore your layout</h3><p>The first arrangement saves the whole OBS dock layout. "
			"<b>Restore previous OBS layout</b> restores that snapshot and may overwrite later layout changes. "
			"New or removed docks may need manual adjustment. Restore before uninstalling DockBloom; "
			"uninstalling does not restore it automatically.</p>"
			"<h3>Web docks and privacy</h3><p>Use <b>Add web</b> with a trusted http or https URL. "
			"Pages can access the network, use cookies, and open popups. Web addresses including query "
			"tokens are saved locally as plain text in OBS plugin settings; do not put secrets in them. "
			"Removing a web dock does not clear OBS browser cookies.</p>"
			"<h3>Missing docks</h3><p>Click Refresh, ensure the other plug-in is loaded, or open the dock "
			"from OBS's Docks menu. Renaming or removing a third-party dock can change its identity. "
			"Unlock OBS docks before arranging; docks limited to other areas cannot be moved here. "
			"The OBS Browser component is required for web docks.</p>"));
		layout->addWidget(text, 1);
		auto *close = button("Close", "guideClose", dialog);
		layout->addWidget(close, 0, Qt::AlignRight);
		connect(close, &QPushButton::clicked, dialog, &QDialog::close);
	}
	guide_->show();
	guide_->raise();
	guide_->activateWindow();
}

QWidget *createWorkspaceAnchor()
{
	auto *widget = new QWidget;
	widget->setObjectName("dockBloomAnchor");
	widget->setStyleSheet("QWidget#dockBloomAnchor { background: #171d30; color: #eeeaff; } "
			      "QLabel { color: #d5caff; font-family: 'Segoe UI'; font-size: 13px; }");
	auto *layout = new QVBoxLayout(widget);
	layout->setContentsMargins(18, 18, 18, 18);
	layout->addStretch();
	auto *title = label("DOCKBLOOM WORKSPACE", "anchorTitle", widget, true);
	title->setAlignment(Qt::AlignCenter);
	layout->addWidget(title);
	auto *caption = label("Choose docks in the DockBloom control panel, then apply Focus or Grid view.",
			      "anchorCaption", widget, true);
	caption->setAlignment(Qt::AlignCenter);
	layout->addWidget(caption);
	layout->addStretch();
	return widget;
}
} // namespace dockbloom

/*
DockBloom's real Qt workspace against a fake OBS host. No real stream or web page.
Copyright (C) 2026 Emmanuel Suah. GPL-2.0-or-later; no warranty.
*/
#include "workspace.hpp"
#include "workspace-ui.hpp"

#include <QAbstractButton>
#include <QApplication>
#include <QDialog>
#include <QDockWidget>
#include <QEvent>
#include <QFile>
#include <QFontMetrics>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTimer>

#include <cstdio>

using namespace dockbloom;
static int checks = 0, failures = 0;
static void check(bool ok, const char *message)
{
	++checks;
	if (!ok)
		++failures;
	std::printf("%s  %s\n", ok ? "PASS" : "FAIL", message);
	std::fflush(stdout);
}

class FakeHost final : public HostBridge {
public:
	explicit FakeHost(QMainWindow *main) : main_(main) {}
	bool addDock(const QString &id, const QString &title, QWidget *content) override
	{
		if (!main_ || main_->findChild<QDockWidget *>(id, Qt::FindDirectChildrenOnly))
			return false;
		auto *dock = new QDockWidget(title, main_);
		dock->setObjectName(id);
		dock->setWidget(content);
		main_->addDockWidget(Qt::RightDockWidgetArea, dock);
		dock->hide();
		dock->setFloating(true);
		return true;
	}
	void removeDock(const QString &id) override
	{
		if (main_)
			delete main_->findChild<QDockWidget *>(id, Qt::FindDirectChildrenOnly);
	}
	bool initializeBrowser() override
	{
		available_ = true;
		return true;
	}
	bool browserAvailable() const override { return available_; }
	QWidget *createBrowser(QWidget *parent, const QString &) override
	{
		++created;
		auto *browser = new QLabel("A fake local browser", parent);
		browser->setObjectName("fakeBrowser");
		return browser;
	}
	void closeBrowser(QWidget *) override { ++closed; }
	void shutdownBrowser() override { available_ = false; }
	int created = 0, closed = 0;

private:
	QPointer<QMainWindow> main_;
	bool available_ = false;
};

static void answer(QMessageBox::StandardButton button)
{
	QTimer::singleShot(0, [button] {
		for (QWidget *top : QApplication::topLevelWidgets()) {
			auto *box = qobject_cast<QMessageBox *>(top);
			if (box && box->isVisible()) {
				if (auto *choice = box->button(button))
					choice->click();
				return;
			}
		}
	});
}

static QDockWidget *findDock(QMainWindow &main, const QString &id)
{
	return main.findChild<QDockWidget *>(id, Qt::FindDirectChildrenOnly);
}

int main(int argc, char **argv)
{
	QApplication app(argc, argv);
	QTemporaryDir temp;
	check(temp.isValid(), "temporary test directory is available");
	if (!temp.isValid())
		return 1;
	QString normalized, error;
	check(normalizeWebUrl("https://example.com/chat?q=hello", &normalized, &error) &&
		      normalized.startsWith("https://example.com/"),
	      "HTTPS web URLs validate and normalize");
	check(normalizeWebUrl("http://127.0.0.1:6000/", &normalized, &error),
	      "local HTTP web dashboards are supported");
	check(!normalizeWebUrl("file:///etc/passwd", nullptr, &error) &&
		      !normalizeWebUrl("javascript:alert(1)", nullptr, &error) &&
		      !normalizeWebUrl("https://user:password@example.com/", nullptr, &error),
	      "file, script and embedded-credential URLs are rejected");

	const QString configPath = temp.filePath("workspace.json");
	Config saved;
	for (int i = 0; i < 24; ++i)
		saved.members.append(QString("existing-%1").arg(i));
	saved.webDocks.append({"dockbloom-web-0123456789abcdef0123456789abcdef", "Chat", "https://example.com/"});
	saved.members.append(saved.webDocks.first().id);
	saved.mode = "grid";
	saved.page = 3;
	check(saveConfig(configPath, saved, &error), "workspace with over ten docks saves atomically");
	Config loaded;
	check(loadConfig(configPath, &loaded, &error) && loaded.members == saved.members &&
		      loaded.webDocks.size() == 1 && loaded.mode == "grid" && loaded.page == 3,
	      "more than ten members, web docks and view survive a reload");
	QFile original(configPath);
	check(original.open(QIODevice::ReadOnly), "saved settings file is readable");
	const QByteArray beforeInvalid = original.readAll();
	original.close();
	Config invalid = saved;
	invalid.members.append(invalid.members.first());
	check(!saveConfig(configPath, invalid, &error), "duplicate dock IDs are rejected");
	check(original.open(QIODevice::ReadOnly), "saved settings file can be reopened");
	check(original.readAll() == beforeInvalid, "invalid settings do not overwrite the previous file");
	original.close();
	QFile corrupt(temp.filePath("corrupt.json"));
	check(corrupt.open(QIODevice::WriteOnly), "corrupt settings fixture can be created");
	check(corrupt.write("{not valid json") > 0, "corrupt settings fixture can be written");
	corrupt.close();
	Config untouched = saved;
	check(!loadConfig(corrupt.fileName(), &untouched, &error) && untouched.members == saved.members,
	      "corrupt settings are rejected without resetting the caller's data");

	QMainWindow main;
	main.setObjectName("fakeOBSMainWindow");
	main.resize(1300, 900);
	main.setCentralWidget(new QLabel("OBS preview", &main));
	FakeHost host(&main);
	const QString runtimePath = temp.filePath("runtime/workspace.json");
	Workspace workspace(&main, &host, runtimePath);
	check(workspace.load(&error), "new workspace loads without a file");
	QList<QPointer<QDockWidget>> native;
	QList<QPointer<QWidget>> contents;
	for (int i = 0; i < 18; ++i) {
		auto *dock = new QDockWidget(QString("Existing dock %1").arg(i, 2, 10, QChar('0')), &main);
		dock->setObjectName(QString("native-%1").arg(i, 2, 10, QChar('0')));
		auto *content = new QLabel(QString("Original content %1").arg(i), dock);
		dock->setWidget(content);
		main.addDockWidget(Qt::LeftDockWidgetArea, dock);
		native.append(dock);
		contents.append(content);
	}
	auto *ui = new WorkspaceUi(&workspace);
	check(host.addDock(kControlId, "DockBloom", ui) &&
		      host.addDock(kWorkspaceId, "DockBloom Workspace", createWorkspaceAnchor()),
	      "control and workspace docks register in the host");
	main.show();
	findDock(main, kControlId)->setFloating(false);
	findDock(main, kControlId)->show();
	findDock(main, kWorkspaceId)->setFloating(false);
	findDock(main, kWorkspaceId)->show();
	workspace.frontendReady();
	ui->refresh();
	app.processEvents();
	check(workspace.availableDocks().size() == 18 && ui->findChild<QListWidget *>("dockList")->count() == 18,
	      "18 existing OBS-like docks are discoverable without a ten-dock limit");
	const QByteArray beforeSelection = main.saveState();
	for (int i = 0; i < 14; ++i)
		check(workspace.setMember(QString("native-%1").arg(i, 2, 10, QChar('0')), true, &error),
		      "existing dock can be added to the workspace list");
	check(workspace.config().members.size() == 14 && main.saveState() == beforeSelection && !workspace.hasBackup(),
	      "selecting 14 docks does not rearrange OBS before applying a view");
	ui->refresh();
	auto *list = ui->findChild<QListWidget *>("dockList");
	for (int i = 0; list && i < list->count(); ++i)
		if (list->item(i)->data(Qt::UserRole).toString() == "native-05")
			list->setCurrentRow(i);
	auto *focusButton = ui->findChild<QPushButton *>("focusButton");
	auto *restoreButton = ui->findChild<QPushButton *>("restoreButton");
	answer(QMessageBox::No);
	if (focusButton)
		focusButton->click();
	app.processEvents();
	check(!workspace.hasBackup() && main.saveState() == beforeSelection,
	      "canceling the first-arrangement confirmation changes no layout");
	answer(QMessageBox::Yes);
	if (focusButton)
		focusButton->click();
	app.processEvents();
	check(workspace.hasBackup() && !native[5]->isHidden(),
	      "the Focus button arranges the selected dock after confirmation");
	answer(QMessageBox::Yes);
	if (restoreButton)
		restoreButton->click();
	app.processEvents();
	check(!workspace.hasBackup() && native[5] && main.dockWidgetArea(native[5]) == Qt::LeftDockWidgetArea,
	      "the Restore button returns the layout after confirmation");
	const auto oldFeatures = native[0]->features();
	native[0]->setFeatures(QDockWidget::NoDockWidgetFeatures);
	check(!workspace.apply("focus", 0, "native-00", &error) && !workspace.hasBackup(),
	      "locked docks cannot be moved and no backup/layout change is made");
	native[0]->setFeatures(oldFeatures);
	check(workspace.apply("focus", 0, "native-05", &error) && workspace.hasBackup(),
	      "focus view arranges 14 docks after saving the original layout");
	app.processEvents();
	QDockWidget *anchor = findDock(main, kWorkspaceId);
	check(anchor && main.tabifiedDockWidgets(anchor).contains(native[5]) && !native[5]->isHidden(),
	      "the chosen dock is shown in the focused workspace");
	bool intact = true;
	for (int i = 0; i < 18; ++i)
		intact = intact && native[i] && native[i]->widget() == contents[i];
	check(intact, "arrangement keeps every OBS dock and its original widget alive");
	check(workspace.apply("grid", 1, QString(), &error), "grid view can show a later page");
	app.processEvents();
	int visiblePage = 0, hiddenOthers = 0;
	for (int i = 0; i < 14; ++i) {
		if (i >= 4 && i < 8)
			visiblePage += !native[i]->isHidden();
		else
			hiddenOthers += native[i]->isHidden();
	}
	check(visiblePage == 4 && hiddenOthers == 10, "grid shows four docks and hides other pages");
	bool allActuallyVisible = true;
	bool distinctCells = true;
	for (int i = 4; i < 8; ++i) {
		allActuallyVisible = allActuallyVisible && native[i]->isVisible() && contents[i]->isVisible() &&
				     native[i]->width() > 5 && native[i]->height() > 5;
		for (int j = i + 1; j < 8; ++j)
			distinctCells = distinctCells && !native[i]->geometry().intersects(native[j]->geometry());
	}
	check(allActuallyVisible && distinctCells,
	      "all four grid widgets are simultaneously visible in distinct cells");
	const QByteArray gridScreenshot = qgetenv("DOCKBLOOM_GRID_SCREENSHOT");
	if (!gridScreenshot.isEmpty())
		check(main.grab().save(QString::fromUtf8(gridScreenshot)), "grid screenshot captured");
	check(workspace.apply("grid", 3, QString(), &error), "last grid page is reachable");
	app.processEvents();
	check(!native[12]->isHidden() && !native[13]->isHidden() && native[0]->isHidden(),
	      "last page shows the remaining two of 14 docks");
	check(workspace.restore(&error) && !workspace.hasBackup(),
	      "previous OBS layout restores and clears its backup");
	app.processEvents();
	bool restored = true;
	for (const auto &dock : native)
		restored = restored && dock && main.dockWidgetArea(dock) == Qt::LeftDockWidgetArea && !dock->isHidden();
	check(restored, "restore returns all original docks to their original area and visibility");

	/* A malformed existing backup is never silently overwritten. */
	QFile badBackup(workspace.backupPath());
	check(badBackup.open(QIODevice::WriteOnly), "invalid backup fixture can be created");
	badBackup.close();
	const QByteArray beforeBadBackup = main.saveState();
	check(!workspace.apply("focus", 0, "native-00", &error) && main.saveState() == beforeBadBackup,
	      "invalid backup prevents rearrangement rather than losing the original layout");
	QFile::remove(workspace.backupPath());

	QString webId;
	check(workspace.addWebDock("Web chat", "https://example.com/chat", &webId, &error) &&
		      workspace.config().members.contains(webId) && findDock(main, webId) && host.created == 1,
	      "adding a web URL creates a registered browser dock and saves it in the workspace");
	check(!workspace.addWebDock("Bad", "file:///private", nullptr, &error),
	      "the add-web workflow refuses non-HTTP URLs");
	for (int i = 0; i < 11; ++i) {
		QString id;
		check(workspace.addWebDock(QString("Web %1").arg(i), "http://127.0.0.1:6000/", &id, &error),
		      "additional web dock can be added");
	}
	check(workspace.config().webDocks.size() == 12 && workspace.config().members.size() == 26,
	      "more than ten web docks coexist with existing OBS docks");
	QPointer<QDockWidget> webDock = findDock(main, webId);
	check(workspace.removeWebDock(webId, &error) && !webDock && host.closed == 1 && native[0],
	      "removing a web dock closes only its browser and leaves native docks intact");

	ui->refresh();
	auto *addWeb = ui->findChild<QPushButton *>("addWebButton");
	if (addWeb)
		addWeb->click();
	app.processEvents();
	auto *dialog = ui->findChild<QDialog *>("addWebDialog");
	auto *webName = dialog ? dialog->findChild<QLineEdit *>("webName") : nullptr;
	auto *webUrl = dialog ? dialog->findChild<QLineEdit *>("webUrl") : nullptr;
	auto *webConfirm = dialog ? dialog->findChild<QPushButton *>("webConfirm") : nullptr;
	check(dialog && dialog->isVisible() && webName && webUrl && webConfirm,
	      "Add web opens a named dialog with accessible input controls");
	if (webName && webUrl && webConfirm) {
		webName->setText("News panel");
		webUrl->setText("file:///secret");
		webConfirm->click();
		auto *webError = dialog->findChild<QLabel *>("webError");
		check(dialog->isVisible() && webError && !webError->text().isEmpty() &&
			      workspace.config().webDocks.size() == 11,
		      "invalid URLs show an inline error and keep the add dialog open");
		webUrl->setText("https://example.com/news");
		webConfirm->click();
		app.processEvents();
		QPointer<QDockWidget> addedByUi = findDock(main, workspace.config().webDocks.last().id);
		check(addedByUi && workspace.config().webDocks.size() == 12,
		      "adding a valid URL through the dialog creates a web dock");
		auto *removeButton = ui->findChild<QPushButton *>("removeWebButton");
		answer(QMessageBox::No);
		if (removeButton)
			removeButton->click();
		check(addedByUi && workspace.config().webDocks.size() == 12,
		      "canceling web removal keeps its dock and settings");
		answer(QMessageBox::Yes);
		if (removeButton)
			removeButton->click();
		app.processEvents();
		check(!addedByUi && workspace.config().webDocks.size() == 11 && host.closed == 2,
		      "confirmed web removal closes only the selected web dock");
	}
	{
		QMainWindow restartedMain;
		FakeHost restartedHost(&restartedMain);
		Workspace restarted(&restartedMain, &restartedHost, runtimePath);
		check(restarted.load(&error), "the saved workspace reloads in a new host");
		restarted.registerSavedWebDocks();
		check(restarted.availableDocks().size() == 11 && restartedHost.created == 0,
		      "saved web placeholders register before frontend/browser initialization");
		restarted.frontendReady();
		check(restartedHost.created == 11 && restarted.availableMembers().size() == 11,
		      "saved browser widgets are created after frontend readiness");
		restarted.shutdown();
		check(restartedHost.closed == 11, "restart fixture closes each browser exactly once");
	}

	ui->refresh();
	for (const int width : {260, 340, 440}) {
		ui->resize(width, 500);
		app.processEvents();
		bool fits = true;
		for (QLabel *text : ui->findChildren<QLabel *>()) {
			if (text->isVisible() && text->wordWrap() && !text->text().isEmpty() &&
			    text->heightForWidth(text->width()) > text->height() + 2) {
				std::fprintf(stderr, "Wrapped label %s needs %dpx but is %dpx high (test width %d).\n",
					     text->objectName().toUtf8().constData(),
					     text->heightForWidth(text->width()), text->height(), width);
				fits = false;
			}
		}
		for (QPushButton *button : ui->findChildren<QPushButton *>()) {
			if (!button->isVisible())
				continue;
			if (button->width() < QFontMetrics(button->font()).horizontalAdvance(button->text()) + 8) {
				std::fprintf(stderr, "Button %s is %dpx wide; text needs %dpx (test width %d).\n",
					     button->objectName().toUtf8().constData(), button->width(),
					     QFontMetrics(button->font()).horizontalAdvance(button->text()) + 8, width);
				fits = false;
			}
		}
		check(fits, width == 260 ? "260px controls and wrapped labels do not clip"
					 : (width == 340 ? "340px controls and wrapped labels do not clip"
							 : "440px controls and wrapped labels do not clip"));
	}
	auto *scroll = ui->findChild<QScrollArea *>("workspaceScroll");
	check(scroll && scroll->verticalScrollBar()->maximum() > 0, "short control dock scrolls to every section");
	auto *help = ui->findChild<QPushButton *>("helpButton");
	const QByteArray beforeHelp = main.saveState();
	if (help)
		help->click();
	app.processEvents();
	auto *guide = ui->findChild<QDialog *>("guideDialog");
	auto *guideText = guide ? guide->findChild<QTextBrowser *>("guideText") : nullptr;
	check(guide && guide->isVisible() && guideText && guideText->toPlainText().contains("Restore") &&
		      main.saveState() == beforeHelp,
	      "offline Help opens without changing OBS dock layout");
	if (guide)
		guide->close();
	const QByteArray screenshot = qgetenv("DOCKBLOOM_UI_SCREENSHOT");
	if (!screenshot.isEmpty()) {
		ui->resize(420, 1100);
		app.processEvents();
		check(ui->grab().save(QString::fromUtf8(screenshot)), "control panel screenshot captured");
	}
	/* An unavailable or duplicate object name must never select the wrong dock. */
	auto *duplicateA = new QDockWidget("Duplicate A", &main);
	auto *duplicateB = new QDockWidget("Duplicate B", &main);
	duplicateA->setObjectName("duplicate-dock");
	duplicateB->setObjectName("duplicate-dock");
	main.addDockWidget(Qt::RightDockWidgetArea, duplicateA);
	main.addDockWidget(Qt::RightDockWidgetArea, duplicateB);
	bool duplicateOffered = false;
	for (const DockInfo &info : workspace.availableDocks())
		duplicateOffered = duplicateOffered || info.id == "duplicate-dock";
	check(!duplicateOffered && !workspace.setMember("duplicate-dock", true, &error),
	      "duplicate dock object names are not offered or accepted");
	delete duplicateB;
	check(workspace.setMember("duplicate-dock", true, &error),
	      "a unique dock can be selected after a collision ends");
	delete duplicateA;
	check(!workspace.availableMembers().contains("duplicate-dock") &&
		      workspace.config().members.contains("duplicate-dock"),
	      "a destroyed third-party dock becomes unavailable without losing the saved selection");
	check(workspace.apply("focus", 0, "native-00", &error),
	      "an unavailable member does not crash layout arrangement");
	check(workspace.restore(&error), "layout can be restored after a selected dock disappears");
	{
		QPointer<WorkspaceUi> disposable = new WorkspaceUi(&workspace);
		disposable->show();
		auto *button = disposable->findChild<QPushButton *>("focusButton");
		QTimer::singleShot(0, [disposable] { delete disposable.data(); });
		if (button)
			button->click();
		app.processEvents();
		check(!disposable && !workspace.hasBackup() && native[0],
		      "closing the control dock during confirmation is safe and changes no layout");
	}

	{
		QMainWindow reentrantMain;
		reentrantMain.setCentralWidget(new QLabel("Preview"));
		FakeHost reentrantHost(&reentrantMain);
		QPointer<Workspace> reentrant =
			new Workspace(&reentrantMain, &reentrantHost, temp.filePath("reentrant/workspace.json"));
		reentrant->load(&error);
		auto *dock = new QDockWidget("A plugin dock", &reentrantMain);
		dock->setObjectName("reentrant-dock");
		dock->setWidget(new QLabel("Content", dock));
		reentrantMain.addDockWidget(Qt::LeftDockWidgetArea, dock);
		reentrantHost.addDock(kWorkspaceId, "Workspace", createWorkspaceAnchor());
		reentrantMain.show();
		app.processEvents();
		reentrant->setMember("reentrant-dock", true, &error);
		QObject::connect(dock, &QDockWidget::visibilityChanged, &reentrantMain, [reentrant](bool visible) {
			if (!visible && reentrant)
				delete reentrant.data();
		});
		const bool applied = reentrant->apply("focus", 0, "reentrant-dock", &error);
		check(!reentrant && !applied && reentrantMain.updatesEnabled(),
		      "reentrant workspace destruction stops arrangement and restores window updates");
	}

	{
		QMainWindow collisionMain;
		FakeHost collisionHost(&collisionMain);
		const QString collisionPath = temp.filePath("collision/workspace.json");
		const QString id = "dockbloom-web-abcdef0123456789abcdef0123456789";
		check(collisionHost.addDock(id, "Foreign plugin dock", new QLabel("Do not delete")),
		      "foreign collision fixture is registered");
		QPointer<QDockWidget> foreign = findDock(collisionMain, id);
		Config collisionConfig;
		collisionConfig.members.append(id);
		collisionConfig.webDocks.append({id, "Saved web", "https://example.com/"});
		check(saveConfig(collisionPath, collisionConfig, &error), "saved web collision fixture is valid");
		Workspace collisionWorkspace(&collisionMain, &collisionHost, collisionPath);
		check(collisionWorkspace.load(&error), "saved web collision fixture loads");
		collisionWorkspace.registerSavedWebDocks();
		check(collisionWorkspace.availableDocks().isEmpty(),
		      "a failed web registration does not expose a foreign dock as ours");
		WorkspaceUi collisionUi(&collisionWorkspace);
		collisionUi.show();
		auto *collisionList = collisionUi.findChild<QListWidget *>("dockList");
		check(collisionList && collisionList->count() == 1 &&
			      collisionList->item(0)->text().contains("Saved web"),
		      "an unavailable saved web dock remains visible in the control list");
		if (collisionList && collisionList->count())
			collisionList->item(0)->setCheckState(Qt::Unchecked);
		app.processEvents();
		check(collisionList && collisionList->count() == 1 &&
			      collisionList->item(0)->checkState() == Qt::Unchecked,
		      "an unchecked unavailable web dock can still be selected for removal");
		if (collisionList && collisionList->count())
			collisionList->setCurrentRow(0);
		auto *collisionRemove = collisionUi.findChild<QPushButton *>("removeWebButton");
		answer(QMessageBox::Yes);
		if (collisionRemove)
			collisionRemove->click();
		check(collisionWorkspace.config().webDocks.isEmpty() && foreign,
		      "removing an unavailable saved web dock never deletes a foreign plugin's dock");
		collisionWorkspace.shutdown();
		check(foreign, "workspace shutdown leaves the foreign colliding dock intact");
	}

	workspace.shutdown();
	check(host.closed == 13, "shutdown closes each remaining fake browser exactly once");
	check(native[0] && native[0]->widget() == contents[0], "shutdown does not delete OBS-owned docks");
	std::printf("\n%d DockBloom checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}

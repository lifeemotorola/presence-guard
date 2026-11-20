/*
 * Test-only OBS module. It drives the actual DockBloom UI in a disposable OBS
 * 33.0.1 installation with a loopback HTTP server and synthetic plugin docks.
 * Never distributed in the user package. No streaming or recording.
 * Copyright (C) 2026 Emmanuel Suah. GPL-2.0-or-later; no warranty.
 */
#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QAbstractButton>
#include <QApplication>
#include <QDialog>
#include <QDockWidget>
#include <QEventLoop>
#include <QFile>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include <functional>
#include <memory>

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("Emmanuel Suah")

namespace {
QPointer<QMainWindow> mainWindow;
QPointer<QDockWidget> customDock;
std::unique_ptr<QTcpServer> server;
int failures = 0;
int webRequests = 0;
int existingRequests = 0;
QString resultPath;
QString statePath;
int phase = 0;
quint16 port = 0;

void record(bool condition, const QString &description)
{
	if (!condition)
		++failures;
	const QString line = QString(condition ? "PASS " : "FAIL ") + description;
	blog(condition ? LOG_INFO : LOG_ERROR, "[dockbloom-test] %s", line.toUtf8().constData());
	QFile file(resultPath);
	if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
		file.write((line + '\n').toUtf8());
}

void waitFor(const std::function<bool()> &predicate, int milliseconds)
{
	if (predicate())
		return;
	QEventLoop loop;
	QTimer poll;
	poll.setInterval(100);
	QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
		if (predicate())
			loop.quit();
	});
	QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
	poll.start();
	loop.exec();
}

void answerYes()
{
	QTimer::singleShot(0, [] {
		for (QWidget *widget : QApplication::topLevelWidgets()) {
			auto *box = qobject_cast<QMessageBox *>(widget);
			if (box && box->isVisible()) {
				if (auto *yes = box->button(QMessageBox::Yes))
					yes->click();
				return;
			}
		}
	});
}

QListWidgetItem *itemFor(QListWidget *list, const QString &id)
{
	for (int i = 0; list && i < list->count(); ++i)
		if (list->item(i)->data(Qt::UserRole).toString() == id)
			return list->item(i);
	return nullptr;
}

QDockWidget *findDock(const QString &id)
{
	return mainWindow ? mainWindow->findChild<QDockWidget *>(id, Qt::FindDirectChildrenOnly) : nullptr;
}

QDockWidget *existingBrowserDock()
{
	if (!mainWindow)
		return nullptr;
	for (QDockWidget *dock : mainWindow->findChildren<QDockWidget *>(QString(), Qt::FindDirectChildrenOnly))
		if (dock->windowTitle() == "Existing browser test")
			return dock;
	return nullptr;
}

QDockWidget *dockBloomWeb()
{
	if (!mainWindow)
		return nullptr;
	for (QDockWidget *dock : mainWindow->findChildren<QDockWidget *>(QString(), Qt::FindDirectChildrenOnly))
		if (dock->objectName().startsWith("dockbloom-web-"))
			return dock;
	return nullptr;
}

bool startServer()
{
	server = std::make_unique<QTcpServer>();
	QObject::connect(server.get(), &QTcpServer::newConnection, server.get(), [] {
		while (server && server->hasPendingConnections()) {
			QTcpSocket *socket = server->nextPendingConnection();
			QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
			QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket] {
				QByteArray request = socket->property("request").toByteArray() + socket->readAll();
				socket->setProperty("request", request);
				if (!request.contains("\r\n\r\n"))
					return;
				const QByteArray firstLine = request.left(request.indexOf("\r\n"));
				if (firstLine.startsWith("GET /web "))
					++webRequests;
				if (firstLine.startsWith("GET /existing "))
					++existingRequests;
				const QByteArray body =
					"<!doctype html><title>DockBloom loopback test</title>"
					"<h1>DockBloom browser test</h1><p>Served only on localhost.</p>";
				const QByteArray response =
					"HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
					"Cache-Control: no-store\r\nConnection: close\r\nContent-Length: " +
					QByteArray::number(body.size()) + "\r\n\r\n" + body;
				socket->write(response);
				socket->disconnectFromHost();
			});
		}
	});
	return server->listen(QHostAddress::LocalHost, port);
}

void finish()
{
	QFile file(resultPath);
	if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
		file.write(QString("DONE phase=%1 failures=%2\n").arg(phase).arg(failures).toUtf8());
	blog(failures ? LOG_ERROR : LOG_INFO, "[dockbloom-test] DONE phase=%d failures=%d", phase, failures);
	/* The caller checks both the DONE marker and OBS's normal exit code. */
	if (mainWindow)
		QTimer::singleShot(250, mainWindow.data(), [] {
			if (mainWindow)
				mainWindow->close();
		});
}

void runTests()
{
	if (!mainWindow) {
		record(false, "OBS main window exists");
		finish();
		return;
	}
	auto *ui = mainWindow->findChild<QWidget *>("dockBloomRoot");
	auto *list = ui ? ui->findChild<QListWidget *>("dockList") : nullptr;
	auto *refresh = ui ? ui->findChild<QPushButton *>("refreshButton") : nullptr;
	auto *focus = ui ? ui->findChild<QPushButton *>("focusButton") : nullptr;
	auto *grid = ui ? ui->findChild<QPushButton *>("gridButton") : nullptr;
	auto *show = ui ? ui->findChild<QPushButton *>("showSelected") : nullptr;
	auto *restore = ui ? ui->findChild<QPushButton *>("restoreButton") : nullptr;
	auto *addWeb = ui ? ui->findChild<QPushButton *>("addWebButton") : nullptr;
	auto *removeWeb = ui ? ui->findChild<QPushButton *>("removeWebButton") : nullptr;
	QPointer<QDockWidget> anchor = findDock("dockbloom-workspace");
	QPointer<QDockWidget> control = findDock("dockbloom-control");
	QPointer<QDockWidget> scenes = findDock("scenesDock");
	QPointer<QDockWidget> existing = existingBrowserDock();
	record(ui && list && refresh && focus && grid && show && restore && addWeb && removeWeb && anchor && control &&
		       scenes && existing,
	       "real OBS exposes DockBloom controls, an OBS dock and an existing custom browser dock");
	if (!ui || !list || !refresh || !focus || !grid || !show || !restore || !addWeb || !removeWeb || !anchor ||
	    !control || !scenes || !existing) {
		finish();
		return;
	}
	control->show();
	refresh->click();
	QApplication::processEvents();
	QPointer<QWidget> originalSceneContent = scenes->widget();
	QPointer<QWidget> originalBrowserContent = existing->widget();

	if (phase == 1) {
		QJsonObject snapshot{{"sceneArea", int(mainWindow->dockWidgetArea(scenes))},
				     {"sceneHidden", scenes->isHidden()},
				     {"sceneFloating", scenes->isFloating()}};
		QFile state(statePath);
		const bool stateWritten = state.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
					  state.write(QJsonDocument(snapshot).toJson()) > 0;
		state.close();
		record(stateWritten, "recorded original OBS dock position and visibility");
		QStringList ids{"scenesDock", existing->objectName(), "dockbloom-test-custom"};
		for (int i = 0; i < 12; ++i)
			ids.append(QString("dockbloom-test-%1").arg(i, 2, 10, QChar('0')));
		bool foundAll = true;
		for (const QString &id : ids) {
			auto *item = itemFor(list, id);
			if (item)
				item->setCheckState(Qt::Checked);
			else
				foundAll = false;
		}
		QApplication::processEvents();
		record(foundAll && ids.size() == 15, "selected 15 built-in, custom-browser and plugin docks");
		if (!foundAll) {
			finish();
			return;
		}
		list->setCurrentItem(itemFor(list, "scenesDock"));
		answerYes();
		focus->click();
		QApplication::processEvents();
		record(mainWindow->tabifiedDockWidgets(anchor).contains(scenes) && !scenes->isHidden(),
		       "focus view moves the real Scenes dock into the workspace");
		grid->click();
		QApplication::processEvents();
		int shown = 0, hidden = 0;
		for (int i = 0; i < ids.size(); ++i) {
			QDockWidget *dock = findDock(ids[i]);
			if (dock && i < 4 && !dock->isHidden())
				++shown;
			if (dock && i >= 4 && dock->isHidden())
				++hidden;
		}
		record(shown == 4 && hidden == 11, "grid displays four of the 15 docks and pages the rest");
		record(scenes && scenes->widget() == originalSceneContent && existing &&
			       existing->widget() == originalBrowserContent && customDock,
		       "moving native and browser docks preserves their original owned widgets");
		waitFor([] { return existingRequests > 0; }, 25000);
		record(existingRequests > 0,
		       "the existing OBS custom browser dock loads a real loopback page in the grid");

		addWeb->click();
		auto *dialog = ui->findChild<QDialog *>("addWebDialog");
		auto *name = dialog ? dialog->findChild<QLineEdit *>("webName") : nullptr;
		auto *url = dialog ? dialog->findChild<QLineEdit *>("webUrl") : nullptr;
		auto *confirm = dialog ? dialog->findChild<QPushButton *>("webConfirm") : nullptr;
		record(dialog && dialog->isVisible() && name && url && confirm,
		       "add-web dialog opens with the OBS browser available");
		if (!name || !url || !confirm) {
			finish();
			return;
		}
		name->setText("DockBloom web test");
		url->setText(QString("http://127.0.0.1:%1/web").arg(port));
		confirm->click();
		QApplication::processEvents();
		QPointer<QDockWidget> web = dockBloomWeb();
		record(web && itemFor(list, web->objectName()),
		       "web URL creates a registered DockBloom dock and list item");
		if (!web) {
			finish();
			return;
		}
		list->setCurrentItem(itemFor(list, web->objectName()));
		focus->click();
		QApplication::processEvents();
		waitFor([] { return webRequests > 0; }, 25000);
		record(webRequests > 0 && !web->isHidden(),
		       "DockBloom's real CEF browser displays and loads the loopback page");
		const int oldRequests = webRequests;
		web->close();
		QApplication::processEvents();
		web->show();
		QApplication::processEvents();
		waitFor([oldRequests] { return webRequests > oldRequests; }, 25000);
		record(web && webRequests > oldRequests, "a closed DockBloom browser dock reopens and loads again");
	} else {
		QPointer<QDockWidget> web = dockBloomWeb();
		record(web && !web->isHidden() && mainWindow->tabifiedDockWidgets(anchor).contains(web),
		       "restart restores the previously focused web dock in the OBS workspace");
		record(web && itemFor(list, web->objectName()) &&
			       itemFor(list, web->objectName())->checkState() == Qt::Checked &&
			       itemFor(list, "scenesDock") && itemFor(list, "scenesDock")->checkState() == Qt::Checked,
		       "workspace membership survives a normal OBS restart");
		waitFor([] { return webRequests > 0; }, 25000);
		record(webRequests > 0, "the restored CEF dock reloads its saved URL after restart");
		grid->click();
		QApplication::processEvents();
		auto *next = ui->findChild<QPushButton *>("nextPage");
		if (next) {
			next->click();
			next->click();
			next->click();
		}
		QApplication::processEvents();
		auto *page = ui->findChild<QLabel *>("pageLabel");
		record(page && page->text() == "4 / 4" && web && !web->isHidden(),
		       "all four pages remain reachable after restart");
		QFile state(statePath);
		const QJsonObject snapshot = state.open(QIODevice::ReadOnly)
						     ? QJsonDocument::fromJson(state.readAll()).object()
						     : QJsonObject{};
		answerYes();
		restore->click();
		QApplication::processEvents();
		record(!snapshot.isEmpty() && scenes &&
			       int(mainWindow->dockWidgetArea(scenes)) == snapshot.value("sceneArea").toInt() &&
			       scenes->isHidden() == snapshot.value("sceneHidden").toBool() &&
			       scenes->isFloating() == snapshot.value("sceneFloating").toBool(),
		       "restore returns the real OBS Scenes dock to its captured position and visibility");
		if (web) {
			refresh->click();
			list->setCurrentItem(itemFor(list, web->objectName()));
			answerYes();
			removeWeb->click();
			QApplication::processEvents();
		}
		record(!web && scenes && scenes->widget() == originalSceneContent && existing &&
			       existing->widget() == originalBrowserContent && customDock,
		       "removing the DockBloom web dock does not delete existing OBS, browser or plugin docks");
	}
	finish();
}

void frontendEvent(enum obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING && mainWindow) {
		/* DockBloom defers CEF setup until all OBS modules are available. */
		QTimer::singleShot(2000, mainWindow.data(), [] { runTests(); });
	} else if (event == OBS_FRONTEND_EVENT_EXIT && customDock) {
		obs_frontend_remove_dock("dockbloom-test-custom");
		delete customDock.data();
		customDock = nullptr;
	}
}
} // namespace

bool obs_module_load(void)
{
	resultPath = QString::fromLocal8Bit(qgetenv("DOCKBLOOM_TEST_RESULT"));
	statePath = QString::fromLocal8Bit(qgetenv("DOCKBLOOM_TEST_STATE"));
	phase = qEnvironmentVariableIntValue("DOCKBLOOM_TEST_PHASE");
	port = quint16(qEnvironmentVariableIntValue("DOCKBLOOM_TEST_PORT"));
	if (resultPath.isEmpty() || statePath.isEmpty() || !port || (phase != 1 && phase != 2))
		return false;
	mainWindow = qobject_cast<QMainWindow *>(static_cast<QWidget *>(obs_frontend_get_main_window()));
	if (!mainWindow || !startServer()) {
		record(false, "disposable OBS main window and loopback server initialized");
		return false;
	}
	for (int i = 0; i < 12; ++i) {
		const QByteArray id = QString("dockbloom-test-%1").arg(i, 2, 10, QChar('0')).toUtf8();
		const QByteArray title = QString("Test plugin dock %1").arg(i).toUtf8();
		auto *content = new QLabel(QString::fromUtf8(title));
		if (!obs_frontend_add_dock_by_id(id.constData(), title.constData(), content)) {
			delete content;
			record(false, "test plugin dock registration");
			return false;
		}
	}
	customDock = new QDockWidget("Test custom plugin dock", mainWindow);
	customDock->setWidget(new QLabel("Owned by the test plugin", customDock));
	if (!obs_frontend_add_custom_qdock("dockbloom-test-custom", customDock.data())) {
		delete customDock.data();
		customDock = nullptr;
		record(false, "test custom plugin dock registration");
		return false;
	}
	obs_frontend_add_event_callback(frontendEvent, nullptr);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(frontendEvent, nullptr);
	server.reset();
}

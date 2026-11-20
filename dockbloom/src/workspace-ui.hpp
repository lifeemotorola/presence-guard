/*
DockBloom for OBS Studio
Copyright (C) 2026 Emmanuel Suah
GPL-2.0-or-later; see LICENSE. No warranty.
*/
#pragma once

#include <QPointer>
#include <QWidget>

class QDialog;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;

namespace dockbloom {
class Workspace;

class WorkspaceUi final : public QWidget {
public:
	explicit WorkspaceUi(Workspace *workspace, QWidget *parent = nullptr);
	void refresh();
	void showError(const QString &message);

private:
	void buildUi();
	void onItemChanged(QListWidgetItem *item);
	void applyView(const QString &mode, int page, const QString &focusedId, bool askIfNeeded);
	void showSelected();
	void restoreLayout();
	void addWebDialog();
	void removeSelectedWeb();
	void showGuide();
	QString selectedId() const;
	void setNotice(const QString &text, bool warning = false);

	QPointer<Workspace> workspace_;
	QPointer<QDialog> addDialog_;
	QPointer<QDialog> guide_;
	QListWidget *list_ = nullptr;
	QLineEdit *search_ = nullptr;
	QLabel *count_ = nullptr;
	QLabel *notice_ = nullptr;
	QLabel *page_ = nullptr;
	QPushButton *previous_ = nullptr;
	QPushButton *next_ = nullptr;
	QPushButton *restore_ = nullptr;
	QPushButton *removeWeb_ = nullptr;
};

QWidget *createWorkspaceAnchor();
} // namespace dockbloom

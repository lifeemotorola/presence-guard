/*
OBS Beautifier
Copyright (C) 2026 Presence Guard contributors

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "beautifier-dock.hpp"
#include "beautifier.hpp"

#include <obs-module.h>

#include <QCheckBox>
#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QFont>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

/* Long enough to swallow a slider drag, short enough to feel instant. */
static const int APPLY_DELAY_MS = 140;

QString BeautifierDock::trText(const char *key) const
{
	return QString::fromUtf8(obs_module_text(key));
}

BeautifierDock::BeautifierDock(Beautifier *beautifier, QWidget *parent) : QWidget(parent), beautifier_(beautifier)
{
	buildUi();

	applyTimer_ = new QTimer(this);
	applyTimer_->setSingleShot(true);
	applyTimer_->setInterval(APPLY_DELAY_MS);
	QObject::connect(applyTimer_, &QTimer::timeout, [this]() { pushOptions(); });

	syncFromBeautifier();
}

/* ------------------------------------------------------------------------- */
/* construction                                                              */
/* ------------------------------------------------------------------------- */

void BeautifierDock::buildUi()
{
	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setContentsMargins(6, 6, 6, 6);
	layout->setSpacing(6);

	enabledBox_ = new QCheckBox(trText("Beautifier.Enabled"), this);
	enabledBox_->setToolTip(trText("Beautifier.Enabled.Tooltip"));
	layout->addWidget(enabledBox_);

	QGroupBox *statusGroup = new QGroupBox(trText("Beautifier.Status"), this);
	QVBoxLayout *statusLayout = new QVBoxLayout(statusGroup);
	statusLabel_ = new QLabel(statusGroup);
	QFont bold = statusLabel_->font();
	bold.setBold(true);
	statusLabel_->setFont(bold);
	detailLabel_ = new QLabel(statusGroup);
	detailLabel_->setWordWrap(true);
	statusLayout->addWidget(statusLabel_);
	statusLayout->addWidget(detailLabel_);
	layout->addWidget(statusGroup);

	QGroupBox *lookGroup = new QGroupBox(trText("Beautifier.Look"), this);
	QGridLayout *grid = new QGridLayout(lookGroup);
	grid->setColumnStretch(1, 1);

	int row = 0;

	presetBox_ = new QComboBox(lookGroup);
	presetBox_->setToolTip(trText("Beautifier.Preset.Tooltip"));
	grid->addWidget(new QLabel(trText("Beautifier.Preset"), lookGroup), row, 0);
	grid->addWidget(presetBox_, row, 1);
	row++;

	autoAccentBox_ = new QCheckBox(trText("Beautifier.Accent.Auto"), lookGroup);
	autoAccentBox_->setToolTip(trText("Beautifier.Accent.Auto.Tooltip"));
	accentButton_ = new QPushButton(lookGroup);
	accentButton_->setToolTip(trText("Beautifier.Accent.Pick.Tooltip"));
	accentButton_->setMinimumWidth(56);
	QHBoxLayout *accentRow = new QHBoxLayout();
	accentRow->addWidget(autoAccentBox_);
	accentRow->addWidget(accentButton_);
	grid->addWidget(new QLabel(trText("Beautifier.Accent"), lookGroup), row, 0);
	grid->addLayout(accentRow, row, 1);
	row++;

	radiusSlider_ = new QSlider(Qt::Horizontal, lookGroup);
	radiusSlider_->setRange(0, 20);
	radiusSlider_->setSingleStep(1);
	radiusSlider_->setPageStep(2);
	radiusValue_ = new QLabel(lookGroup);
	radiusValue_->setMinimumWidth(40);
	QHBoxLayout *radiusRow = new QHBoxLayout();
	radiusRow->addWidget(radiusSlider_);
	radiusRow->addWidget(radiusValue_);
	grid->addWidget(new QLabel(trText("Beautifier.Radius"), lookGroup), row, 0);
	grid->addLayout(radiusRow, row, 1);
	row++;

	densityBox_ = new QComboBox(lookGroup);
	densityBox_->addItem(trText("Beautifier.Density.Compact"));
	densityBox_->addItem(trText("Beautifier.Density.Normal"));
	densityBox_->addItem(trText("Beautifier.Density.Comfortable"));
	grid->addWidget(new QLabel(trText("Beautifier.Density"), lookGroup), row, 0);
	grid->addWidget(densityBox_, row, 1);
	row++;

	fontSlider_ = new QSlider(Qt::Horizontal, lookGroup);
	fontSlider_->setRange(90, 130);
	fontSlider_->setSingleStep(5);
	fontSlider_->setPageStep(5);
	fontValue_ = new QLabel(lookGroup);
	fontValue_->setMinimumWidth(40);
	QHBoxLayout *fontRow = new QHBoxLayout();
	fontRow->addWidget(fontSlider_);
	fontRow->addWidget(fontValue_);
	grid->addWidget(new QLabel(trText("Beautifier.FontScale"), lookGroup), row, 0);
	grid->addLayout(fontRow, row, 1);
	row++;

	layout->addWidget(lookGroup);

	QGroupBox *detailGroup = new QGroupBox(trText("Beautifier.Details"), this);
	QVBoxLayout *detailLayout = new QVBoxLayout(detailGroup);
	roundedBox_ = new QCheckBox(trText("Beautifier.RoundedButtons"), detailGroup);
	tabsBox_ = new QCheckBox(trText("Beautifier.AccentTabs"), detailGroup);
	scrollbarsBox_ = new QCheckBox(trText("Beautifier.SlimScrollbars"), detailGroup);
	docksBox_ = new QCheckBox(trText("Beautifier.CompactDocks"), detailGroup);
	detailLayout->addWidget(roundedBox_);
	detailLayout->addWidget(tabsBox_);
	detailLayout->addWidget(scrollbarsBox_);
	detailLayout->addWidget(docksBox_);
	layout->addWidget(detailGroup);

	QHBoxLayout *buttons = new QHBoxLayout();
	applyButton_ = new QPushButton(trText("Beautifier.Apply"), this);
	revertButton_ = new QPushButton(trText("Beautifier.Revert"), this);
	revertButton_->setToolTip(trText("Beautifier.Revert.Tooltip"));
	buttons->addWidget(applyButton_);
	buttons->addWidget(revertButton_);
	layout->addLayout(buttons);

	QLabel *hint = new QLabel(trText("Beautifier.Hint"), this);
	hint->setWordWrap(true);
	hint->setEnabled(false);
	layout->addWidget(hint);

	layout->addStretch(1);

	/* --- wiring --- */

	QObject::connect(enabledBox_, &QCheckBox::toggled, [this](bool) { scheduleApply(); });
	QObject::connect(autoAccentBox_, &QCheckBox::toggled, [this](bool) { scheduleApply(); });
	QObject::connect(roundedBox_, &QCheckBox::toggled, [this](bool) { scheduleApply(); });
	QObject::connect(tabsBox_, &QCheckBox::toggled, [this](bool) { scheduleApply(); });
	QObject::connect(scrollbarsBox_, &QCheckBox::toggled, [this](bool) { scheduleApply(); });
	QObject::connect(docksBox_, &QCheckBox::toggled, [this](bool) { scheduleApply(); });

	QObject::connect(radiusSlider_, &QSlider::valueChanged, [this](int) {
		updateValueLabels();
		scheduleApply();
	});
	QObject::connect(fontSlider_, &QSlider::valueChanged, [this](int) {
		updateValueLabels();
		scheduleApply();
	});

	QObject::connect(densityBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
			 [this](int) { scheduleApply(); });

	QObject::connect(accentButton_, &QPushButton::clicked, [this]() {
		if (!beautifier_)
			return;

		const QColor current(beautifier_->options.accentRgb);
		const QColor chosen = QColorDialog::getColor(current, this, trText("Beautifier.Accent.Pick"),
							     QColorDialog::DontUseNativeDialog);

		if (!chosen.isValid())
			return;

		/* Picking a colour means the theme accent is no longer in charge. */
		loading_ = true;
		autoAccentBox_->setChecked(false);
		loading_ = false;

		beautify::Options options = beautifier_->options;
		options.autoAccent = false;
		options.accentRgb = chosen.rgb() & 0xFFFFFF;
		beautifier_->applyOptions(options);
		syncFromBeautifier();
	});

	QObject::connect(presetBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) {
		if (loading_ || !beautifier_ || index <= 0)
			return;

		std::size_t count = 0;
		const beautify::Preset *list = beautify::presets(&count);
		if ((std::size_t)(index - 1) >= count)
			return;

		beautify::Options options = beautifier_->options;
		options.enabled = true;

		if (!beautify::applyPreset(list[index - 1].id, &options))
			return;

		beautifier_->applyOptions(options);
		syncFromBeautifier();
	});

	QObject::connect(applyButton_, &QPushButton::clicked, [this]() {
		pushOptions();

		if (!beautifier_)
			return;

		/* "Apply" also switches it on: pressing it should do something. */
		if (!beautifier_->options.enabled) {
			beautify::Options options = beautifier_->options;
			options.enabled = true;
			beautifier_->applyOptions(options);
		}

		syncFromBeautifier();
	});

	QObject::connect(revertButton_, &QPushButton::clicked, [this]() {
		if (!beautifier_)
			return;

		beautify::Options options = beautifier_->options;
		options.enabled = false;
		beautifier_->applyOptions(options);
		beautifier_->revert();
		syncFromBeautifier();
	});
}

/* ------------------------------------------------------------------------- */
/* state                                                                     */
/* ------------------------------------------------------------------------- */

void BeautifierDock::updateValueLabels()
{
	radiusValue_->setText(trText("Beautifier.RadiusValue").arg(radiusSlider_->value()));
	fontValue_->setText(trText("Beautifier.FontValue").arg(fontSlider_->value()));
}

void BeautifierDock::updateAccentSwatch()
{
	if (!accentButton_ || !beautifier_)
		return;

	const QColor colour(beautifier_->options.accentRgb);
	accentButton_->setText(colour.name().toUpper());
	const QString ink = beautify::isDark(colour.rgb()) ? QStringLiteral("#FFFFFF") : QStringLiteral("#101010");
	accentButton_->setStyleSheet(QStringLiteral("background-color: %1; color: %2;").arg(colour.name(), ink));
}

void BeautifierDock::syncFromBeautifier()
{
	if (!beautifier_ || loading_)
		return;

	loading_ = true;

	/* Presets: "Custom" first, then the built-in looks. */
	presetBox_->clear();
	presetBox_->addItem(trText("Beautifier.Preset.Custom"));

	std::size_t count = 0;
	const beautify::Preset *list = beautify::presets(&count);
	for (std::size_t i = 0; i < count; i++) {
		const std::string key = std::string("Beautifier.Preset.") + list[i].id;
		presetBox_->addItem(QString::fromUtf8(obs_module_text(key.c_str())));
	}

	const beautify::Options &options = beautifier_->options;

	enabledBox_->setChecked(options.enabled);
	autoAccentBox_->setChecked(options.autoAccent);
	radiusSlider_->setValue(options.radius);
	fontSlider_->setValue(options.fontScale);
	densityBox_->setCurrentIndex((int)options.density);
	roundedBox_->setChecked(options.roundedButtons);
	tabsBox_->setChecked(options.accentTabs);
	scrollbarsBox_->setChecked(options.slimScrollbars);
	docksBox_->setChecked(options.compactDocks);

	/* Which preset, if any, describes the current settings? */
	int presetIndex = 0;
	if (!options.autoAccent) {
		for (std::size_t i = 0; i < count; i++) {
			if (list[i].accentRgb == options.accentRgb && list[i].radius == options.radius &&
			    list[i].density == options.density && list[i].accentTabs == options.accentTabs) {
				presetIndex = (int)i + 1;
				break;
			}
		}
	}
	presetBox_->setCurrentIndex(presetIndex);

	updateValueLabels();
	updateAccentSwatch();

	statusLabel_->setText(beautifier_->statusText());
	detailLabel_->setText(beautifier_->detailText());

	loading_ = false;
}

void BeautifierDock::scheduleApply()
{
	if (loading_ || !beautifier_ || !applyTimer_)
		return;

	applyTimer_->start();
}

void BeautifierDock::pushOptions()
{
	if (!beautifier_ || loading_)
		return;

	beautify::Options options = beautifier_->options;

	options.enabled = enabledBox_->isChecked();
	options.autoAccent = autoAccentBox_->isChecked();
	options.radius = radiusSlider_->value();
	options.fontScale = fontSlider_->value();
	options.density = (beautify::Density)densityBox_->currentIndex();
	options.roundedButtons = roundedBox_->isChecked();
	options.accentTabs = tabsBox_->isChecked();
	options.slimScrollbars = scrollbarsBox_->isChecked();
	options.compactDocks = docksBox_->isChecked();

	/* Keep the chosen colour unless the theme is in charge again. */
	if (!options.autoAccent && options.accentRgb == 0)
		options.accentRgb = beautifier_->options.accentRgb;

	beautifier_->applyOptions(options);

	statusLabel_->setText(beautifier_->statusText());
	detailLabel_->setText(beautifier_->detailText());
	updateAccentSwatch();
}

void BeautifierDock::detach()
{
	beautifier_ = nullptr;
}

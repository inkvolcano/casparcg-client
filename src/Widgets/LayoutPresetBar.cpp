#include "LayoutPresetBar.h"

#include "LayoutPreset.h"

#include "DatabaseManager.h"
#include "EventManager.h"
#include "Models/ConfigurationModel.h"
#include "Models/LayoutPresetModel.h"

#include <QtCore/QMap>

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMessageBox>

LayoutPresetBar::LayoutPresetBar(const QString& scope, QWidget* parent)
    : QGroupBox("Saved Layouts", parent), scope(scope)
{
    QHBoxLayout* row = new QHBoxLayout(this);
    row->setContentsMargins(10, 18, 10, 10);
    row->setSpacing(8);

    this->presetCombo = new QComboBox(this);
    this->presetCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    this->presetCombo->setToolTip("Layouts you have saved for this mode.");
    row->addWidget(this->presetCombo, 1);

    auto addButton = [this, row](const QString& text, const QString& tip) {
        QPushButton* button = new QPushButton(text, this);
        button->setFocusPolicy(Qt::NoFocus);
        button->setToolTip(tip);
        row->addWidget(button);
        return button;
    };

    this->applyButton = addButton("Apply", "Put the selected layout back.");
    this->saveButton = addButton("Save As...", "Save the current arrangement under a name.");
    this->renameButton = addButton("Rename...", "Rename the selected layout.");
    this->deleteButton = addButton("Delete", "Delete the selected layout.");

    QObject::connect(this->applyButton, &QPushButton::clicked, this, &LayoutPresetBar::applyClicked);
    QObject::connect(this->saveButton, &QPushButton::clicked, this, &LayoutPresetBar::saveClicked);
    QObject::connect(this->renameButton, &QPushButton::clicked, this, &LayoutPresetBar::renameClicked);
    QObject::connect(this->deleteButton, &QPushButton::clicked, this, &LayoutPresetBar::deleteClicked);

    QObject::connect(this->presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     this, [this](int) { updateButtonState(); });

    refresh();
}

void LayoutPresetBar::refresh()
{
    const int previousId = selectedPresetId();

    this->presetCombo->blockSignals(true);
    this->presetCombo->clear();

    const QList<LayoutPresetModel> presets = DatabaseManager::getInstance().getLayoutPresets(this->scope);

    if (presets.isEmpty())
        this->presetCombo->addItem("(none saved yet)", -1);

    foreach (const LayoutPresetModel& preset, presets)
        this->presetCombo->addItem(preset.getName(), preset.getId());

    // Keep the operator on the row they were on, so saving over a preset does
    // not silently move the selection somewhere else.
    const int index = this->presetCombo->findData(previousId);
    if (index >= 0)
        this->presetCombo->setCurrentIndex(index);

    this->presetCombo->blockSignals(false);

    updateButtonState();
}

int LayoutPresetBar::selectedPresetId() const
{
    if (this->presetCombo == nullptr || this->presetCombo->currentIndex() < 0)
        return -1;

    return this->presetCombo->currentData().toInt();
}

QString LayoutPresetBar::selectedPresetName() const
{
    return selectedPresetId() > 0 ? this->presetCombo->currentText() : QString();
}

void LayoutPresetBar::updateButtonState()
{
    const bool hasSelection = selectedPresetId() > 0;

    this->applyButton->setEnabled(hasSelection);
    this->renameButton->setEnabled(hasSelection);
    this->deleteButton->setEnabled(hasSelection);
}

void LayoutPresetBar::saveClicked()
{
    bool accepted = false;

    // Offering the selected name back makes "save over this one" the easy path,
    // which is what an operator adjusting a saved layout actually wants.
    const QString typed = QInputDialog::getText(this, "Save Layout",
        "Name this layout:", QLineEdit::Normal, selectedPresetName(), &accepted);

    if (!accepted)
        return;

    const QString name = LayoutPreset::sanitiseName(typed);
    if (name.isEmpty())
    {
        QMessageBox::information(this, "Save Layout", "That name cannot be used. Try a different one.");
        return;
    }

    // Overwriting is a real loss, so it is asked about rather than assumed.
    const QList<LayoutPresetModel> existing = DatabaseManager::getInstance().getLayoutPresets(this->scope);
    foreach (const LayoutPresetModel& preset, existing)
    {
        if (!LayoutPreset::sameName(preset.getName(), name))
            continue;

        const int answer = QMessageBox::question(this, "Save Layout",
            QString("\"%1\" already exists. Replace it with the current arrangement?").arg(preset.getName()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

        if (answer != QMessageBox::Yes)
            return;

        break;
    }

    // Read the current arrangement out of the settings this scope owns. A key
    // that has never been set is skipped rather than saved as empty, so applying
    // the preset later leaves it at its default instead of blanking it.
    QMap<QString, QString> values;
    const QStringList keys = LayoutPreset::keysFor(this->scope);
    foreach (const QString& key, keys)
    {
        const QString value = DatabaseManager::getInstance().getConfigurationByName(key).getValue();
        if (!value.isEmpty())
            values.insert(key, value);
    }

    DatabaseManager::getInstance().saveLayoutPreset(name, this->scope, LayoutPreset::serialise(values));

    refresh();

    const int index = this->presetCombo->findText(name);
    if (index >= 0)
        this->presetCombo->setCurrentIndex(index);

    EventManager::getInstance().fireStatusbarEvent(
        StatusbarEvent(QString("Saved layout \"%1\" (%2 settings)").arg(name).arg(values.size())));
}

void LayoutPresetBar::applyClicked()
{
    const int id = selectedPresetId();
    if (id <= 0)
        return;

    LayoutPresetModel selected(0, QString(), QString(), QString());
    bool found = false;

    const QList<LayoutPresetModel> presets = DatabaseManager::getInstance().getLayoutPresets(this->scope);
    foreach (const LayoutPresetModel& preset, presets)
    {
        if (preset.getId() != id)
            continue;

        selected = preset;
        found = true;
        break;
    }

    if (!found)
    {
        refresh();
        return;
    }

    const QMap<QString, QString> values = LayoutPreset::deserialise(selected.getData(), this->scope);

    if (values.isEmpty())
    {
        QMessageBox::information(this, "Apply Layout",
            QString("\"%1\" holds nothing this version understands, so nothing was changed.").arg(selected.getName()));
        return;
    }

    // Every key the scope owns is written, not only the ones the preset carries.
    // A key the preset has nothing for is cleared, because it was at its default
    // when the layout was saved and leaving whatever is there now would give an
    // arrangement that matches neither the preset nor what was on screen.
    const QStringList keys = LayoutPreset::keysFor(this->scope);
    foreach (const QString& key, keys)
    {
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, key, values.value(key, QString())));
    }

    EventManager::getInstance().fireStatusbarEvent(
        StatusbarEvent(QString("Applied layout \"%1\"").arg(selected.getName())));

    emit presetApplied();
}

void LayoutPresetBar::renameClicked()
{
    const int id = selectedPresetId();
    if (id <= 0)
        return;

    const QString current = selectedPresetName();

    bool accepted = false;
    const QString typed = QInputDialog::getText(this, "Rename Layout",
        "New name:", QLineEdit::Normal, current, &accepted);

    if (!accepted)
        return;

    const QString name = LayoutPreset::sanitiseName(typed);
    if (name.isEmpty() || LayoutPreset::sameName(name, current))
        return;

    const QList<LayoutPresetModel> presets = DatabaseManager::getInstance().getLayoutPresets(this->scope);
    foreach (const LayoutPresetModel& preset, presets)
    {
        if (preset.getId() != id && LayoutPreset::sameName(preset.getName(), name))
        {
            QMessageBox::information(this, "Rename Layout",
                QString("There is already a layout called \"%1\".").arg(preset.getName()));
            return;
        }
    }

    DatabaseManager::getInstance().renameLayoutPreset(id, name);

    refresh();
}

void LayoutPresetBar::deleteClicked()
{
    const int id = selectedPresetId();
    if (id <= 0)
        return;

    const QString name = selectedPresetName();

    const int answer = QMessageBox::question(this, "Delete Layout",
        QString("Delete the layout \"%1\"? The arrangement on screen is not changed.").arg(name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (answer != QMessageBox::Yes)
        return;

    DatabaseManager::getInstance().deleteLayoutPreset(id);

    refresh();
}

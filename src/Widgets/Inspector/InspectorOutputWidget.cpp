#include "InspectorOutputWidget.h"
#include "../Rundown/RundownWidgetHelper.h"

#include "Global.h"

#include "DatabaseManager.h"
#include "DeviceManager.h"
#include "EventManager.h"
#include "Commands/BlendModeCommand.h"
#include "Commands/GridCommand.h"
#include "Commands/BrightnessCommand.h"
#include "Commands/ClearOutputCommand.h"
#include "Commands/CommitCommand.h"
#include "Commands/ResetCommand.h"
#include "Commands/ContrastCommand.h"
#include "Commands/CustomCommand.h"
#include "Commands/ClipCommand.h"
#include "Commands/DeckLinkInputCommand.h"
#include "Commands/FillCommand.h"
#include "Commands/GpiOutputCommand.h"
#include "Commands/GroupCommand.h"
#include "Commands/FileRecorderCommand.h"
#include "Commands/KeyerCommand.h"
#include "Commands/LevelsCommand.h"
#include "Commands/OpacityCommand.h"
#include "Commands/PrintCommand.h"
#include "Commands/SaturationCommand.h"
#include "Commands/SeparatorCommand.h"
#include "Commands/GatewayCommand.h"
#include "Commands/SolidColorCommand.h"
#include "Commands/FadeToBlackCommand.h"
#include "Commands/VolumeCommand.h"
#include "Commands/ChromaCommand.h"
#include "Commands/PlayoutCommand.h"
#include "Commands/RotationCommand.h"
#include "Commands/OscOutputCommand.h"
#include "Commands/PerspectiveCommand.h"
#include "Commands/AnchorCommand.h"
#include "Commands/CropCommand.h"
#include "Commands/HtmlCommand.h"
#include "Commands/HttpGetCommand.h"
#include "Commands/HttpPostCommand.h"
#include "Commands/RouteChannelCommand.h"
#include "Commands/RouteVideolayerCommand.h"
#include "Events/Inspector/ChannelChangedEvent.h"
#include "Events/Inspector/LabelChangedEvent.h"
#include "Events/Inspector/TargetChangedEvent.h"
#include "Events/Inspector/VideolayerChangedEvent.h"
#include "Models/ConfigurationModel.h"

#include <QtCore/QtMath>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLineEdit>

InspectorOutputWidget::InspectorOutputWidget(QWidget *parent)
    : QWidget(parent),
      command(NULL), model(NULL), delayType(""), libraryFilter(""), forceMilliseconds(false)
{
    setupUi(this);

    this->comboBoxDevice->setEnabled(false);

    this->delayType = DatabaseManager::getInstance().getConfigurationByName("DelayType").getValue();

    this->comboBoxTarget->lineEdit()->setStyleSheet("background-color: transparent; border-width: 0px;");

    this->buttonDelayUnit->setStyleSheet("min-width: 20px; max-width: 30px; padding: 2px 4px;");
    this->buttonDurationUnit->setStyleSheet("min-width: 20px; max-width: 30px; padding: 2px 4px;");

    QObject::connect(this->buttonDelayUnit, &QPushButton::clicked, this, &InspectorOutputWidget::toggleDelayUnit);
    QObject::connect(this->buttonDurationUnit, &QPushButton::clicked, this, &InspectorOutputWidget::toggleDurationUnit);

    QObject::connect(&DeviceManager::getInstance(), SIGNAL(deviceRemoved()), this, SLOT(deviceRemoved()));
    QObject::connect(&DeviceManager::getInstance(), SIGNAL(deviceAdded(CasparDevice &)), this, SLOT(deviceAdded(CasparDevice &)));

    QObject::connect(&EventManager::getInstance(), SIGNAL(rundownItemSelected(const RundownItemSelectedEvent &)), this, SLOT(rundownItemSelected(const RundownItemSelectedEvent &)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(libraryItemSelected(const LibraryItemSelectedEvent &)), this, SLOT(libraryItemSelected(const LibraryItemSelectedEvent &)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(emptyRundown(const EmptyRundownEvent &)), this, SLOT(emptyRundown(const EmptyRundownEvent &)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(deviceChanged(const DeviceChangedEvent &)), this, SLOT(deviceChanged(const DeviceChangedEvent &)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(mediaChanged(const MediaChangedEvent &)), this, SLOT(mediaChanged(const MediaChangedEvent &)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(templateChanged(const TemplateChangedEvent &)), this, SLOT(templateChanged(const TemplateChangedEvent &)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(libraryFilterChanged(const LibraryFilterChangedEvent &)), this, SLOT(libraryFilterChanged(const LibraryFilterChangedEvent &)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(bankAssignmentChanged(const BankAssignmentChangedEvent &)), this, SLOT(bankAssignmentChanged(const BankAssignmentChangedEvent &)));
}

void InspectorOutputWidget::updateUnitButtons()
{
    if (this->forceMilliseconds)
    {
        this->buttonDelayUnit->setText("ms");
        this->buttonDelayUnit->setEnabled(false);
        this->buttonDurationUnit->setText("ms");
        this->buttonDurationUnit->setEnabled(false);
    }
    else
    {
        QString delayUnit = DatabaseManager::getInstance().getConfigurationByName("DelayType").getValue();
        QString durationUnit = DatabaseManager::getInstance().getConfigurationByName("DurationUnit").getValue();

        this->buttonDelayUnit->setText(delayUnit == Output::DEFAULT_DELAY_IN_FRAMES ? "fr" : "ms");
        this->buttonDelayUnit->setEnabled(this->spinBoxDelay->isEnabled());
        this->buttonDurationUnit->setText(durationUnit == Output::DEFAULT_DELAY_IN_FRAMES ? "fr" : "ms");
        this->buttonDurationUnit->setEnabled(this->spinBoxDuration->isEnabled());
    }
}

double InspectorOutputWidget::getCurrentFps()
{
    if (this->model == nullptr)
        return 50.0;

    QString deviceName = this->comboBoxDevice->currentText();
    int channel = this->spinBoxChannel->value();

    if (deviceName.isEmpty() || channel <= 0)
        return 50.0;

    const QStringList channelFormats = DatabaseManager::getInstance().getDeviceByName(deviceName).getChannelFormats().split(",");
    if (channel > channelFormats.count())
        return 50.0;

    double fps = DatabaseManager::getInstance().getFormat(channelFormats[channel - 1]).getFramesPerSecond().toDouble();
    return (fps > 0) ? fps : 50.0;
}

void InspectorOutputWidget::toggleDelayUnit()
{
    if (this->command == nullptr || this->forceMilliseconds)
        return;

    QString currentUnit = DatabaseManager::getInstance().getConfigurationByName("DelayType").getValue();
    QString newUnit = (currentUnit == Output::DEFAULT_DELAY_IN_MILLISECONDS)
                        ? Output::DEFAULT_DELAY_IN_FRAMES
                        : Output::DEFAULT_DELAY_IN_MILLISECONDS;

    double fps = getCurrentFps();
    int currentValue = this->spinBoxDelay->value();
    int newValue = currentValue;

    if (currentValue > 0)
    {
        if (newUnit == Output::DEFAULT_DELAY_IN_FRAMES)
            newValue = qRound(currentValue * fps / 1000.0);
        else
            newValue = qRound(currentValue * 1000.0 / fps);
    }

    // Save to DB first so format functions pick up the new unit.
    DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, "DelayType", newUnit));
    RundownWidgetHelper::invalidateConfigCache();

    this->buttonDelayUnit->setText(newUnit == Output::DEFAULT_DELAY_IN_FRAMES ? "fr" : "ms");

    // Update spinbox — triggers delayChanged which updates the rundown item.
    blockAllSignals(true);
    this->spinBoxDelay->setValue(newValue);
    blockAllSignals(false);

    for (AbstractCommand* cmd : this->allCommands)
        cmd->setDelay(newValue);

    // Refresh all rundown item labels.
    EventManager::getInstance().fireUnitSettingsChangedEvent();
}

void InspectorOutputWidget::toggleDurationUnit()
{
    if (this->command == nullptr || this->forceMilliseconds)
        return;

    QString currentUnit = DatabaseManager::getInstance().getConfigurationByName("DurationUnit").getValue();
    QString newUnit = (currentUnit == Output::DEFAULT_DELAY_IN_MILLISECONDS)
                        ? Output::DEFAULT_DELAY_IN_FRAMES
                        : Output::DEFAULT_DELAY_IN_MILLISECONDS;

    double fps = getCurrentFps();
    int currentValue = this->spinBoxDuration->value();
    int newValue = currentValue;

    if (currentValue > 0)
    {
        if (newUnit == Output::DEFAULT_DELAY_IN_FRAMES)
            newValue = qRound(currentValue * fps / 1000.0);
        else
            newValue = qRound(currentValue * 1000.0 / fps);
    }

    DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, "DurationUnit", newUnit));

    this->buttonDurationUnit->setText(newUnit == Output::DEFAULT_DELAY_IN_FRAMES ? "fr" : "ms");

    blockAllSignals(true);
    this->spinBoxDuration->setValue(newValue);
    blockAllSignals(false);

    for (AbstractCommand* cmd : this->allCommands)
        cmd->setDuration(newValue);

    EventManager::getInstance().fireUnitSettingsChangedEvent();
}

void InspectorOutputWidget::libraryFilterChanged(const LibraryFilterChangedEvent &event)
{
    this->libraryFilter = event.getFilter();

    checkEmptyTarget();
}

void InspectorOutputWidget::rundownItemSelected(const RundownItemSelectedEvent &event)
{
    this->command = nullptr;
    this->model = event.getLibraryModel();
    this->allCommands = event.getAllCommands();
    this->forceMilliseconds = false;

    const QSharedPointer<DeviceModel> deviceModel = DeviceManager::getInstance().getDeviceModelByName(this->model->getDeviceName());

    blockAllSignals(true);

    this->comboBoxDevice->setVisible(true);

    this->comboBoxDevice->setEnabled(true);
    this->comboBoxTarget->setEnabled(true);
    this->spinBoxChannel->setEnabled(true);
    this->spinBoxVideolayer->setEnabled(true);
    this->spinBoxDelay->setEnabled(true);
    this->spinBoxDuration->setEnabled(true);
    this->checkBoxAllowGpi->setEnabled(true);
    this->checkBoxAllowRemoteTriggering->setEnabled(true);
    this->labelRemoteTriggerIdField->setEnabled(true);
    this->lineEditRemoteTriggerId->setEnabled(true);

    this->buttonDelayUnit->setVisible(true);
    this->buttonDurationUnit->setVisible(true);

    if (event.getCommand() != NULL && event.getLibraryModel() != NULL)
    {
        this->command = event.getCommand();

        int index = this->comboBoxDevice->findText(this->model->getDeviceName());
        int channelMax = 8; // Reasonable default — always allow channel selection.
        if (index != -1 && deviceModel != NULL)
        {
            const QStringList &channelFormats = DatabaseManager::getInstance().getDeviceByName(deviceModel->getName()).getChannelFormats().split(",");
            int reportedChannels = channelFormats.count();
            if (reportedChannels > 1 || (!channelFormats.isEmpty() && !channelFormats.first().isEmpty()))
                channelMax = reportedChannels;
        }
        // Ensure the maximum accommodates the item's current channel value.
        int itemChannel = this->command->getBaseChannel();
        if (itemChannel > channelMax)
            channelMax = itemChannel;
        this->spinBoxChannel->setMaximum(channelMax);

        this->comboBoxDevice->setCurrentIndex(index);
        this->spinBoxChannel->setValue(itemChannel);
        this->spinBoxVideolayer->setValue(this->command->getVideolayer());
        this->spinBoxDelay->setValue(this->command->getDelay());
        this->spinBoxDuration->setValue(this->command->getDuration());
        this->checkBoxAllowGpi->setChecked(this->command->getAllowGpi());
        this->checkBoxAllowRemoteTriggering->setChecked(this->command->getAllowRemoteTriggering());
        this->lineEditRemoteTriggerId->setText(this->command->getRemoteTriggerId());

        int bank = this->command->getTriggerBank();
        if (bank > 0)
        {
            QString hotkey = DatabaseManager::getInstance().getConfigurationByName(QString("HotkeyBank%1").arg(bank)).getValue();
            this->labelTriggerBank->setText(QString("Bank %1 (%2)").arg(bank).arg(hotkey.isEmpty() ? "-" : hotkey));
        }
        else
        {
            this->labelTriggerBank->setText("");
        }

        if (!this->checkBoxAllowRemoteTriggering->isChecked())
        {
            this->labelRemoteTriggerIdField->setEnabled(false);
            this->lineEditRemoteTriggerId->setEnabled(false);
        }

        fillTargetCombo(this->model->getType());

        if (dynamic_cast<FileRecorderCommand *>(event.getCommand()))
        {
            this->comboBoxTarget->setEnabled(false);
            this->spinBoxVideolayer->setEnabled(false);

            this->comboBoxTarget->setCurrentIndex(-1);
            this->spinBoxVideolayer->setValue(Output::DEFAULT_VIDEOLAYER);
        }
        else if (dynamic_cast<CommitCommand *>(event.getCommand()) ||
                 dynamic_cast<PrintCommand *>(event.getCommand()))
        {
            this->comboBoxTarget->setEnabled(false);
            this->spinBoxVideolayer->setEnabled(false);
            this->spinBoxDuration->setEnabled(false);

            this->comboBoxTarget->setCurrentIndex(-1);
            this->spinBoxVideolayer->setValue(Output::DEFAULT_VIDEOLAYER);
            this->spinBoxDuration->setValue(Output::DEFAULT_DURATION);
        }
        else if (dynamic_cast<GridCommand *>(event.getCommand()))
        {
            this->comboBoxTarget->setEnabled(false);
            this->spinBoxVideolayer->setEnabled(false);

            this->spinBoxVideolayer->setValue(Output::DEFAULT_VIDEOLAYER);
        }
        else if (dynamic_cast<GroupCommand *>(event.getCommand()))
        {
            this->comboBoxDevice->setEnabled(false);
            this->comboBoxTarget->setEnabled(false);
            this->spinBoxChannel->setEnabled(false);
            this->spinBoxVideolayer->setEnabled(false);
            this->spinBoxDelay->setEnabled(false);
            this->spinBoxDuration->setEnabled(false);

            this->forceMilliseconds = true;

            this->comboBoxDevice->setCurrentIndex(-1);
            this->comboBoxTarget->setCurrentIndex(-1);
            this->spinBoxChannel->setValue(Output::DEFAULT_CHANNEL);
            this->spinBoxVideolayer->setValue(Output::DEFAULT_VIDEOLAYER);
            this->spinBoxDelay->setValue(Output::DEFAULT_DELAY);
        }
        else if (dynamic_cast<GpiOutputCommand *>(event.getCommand()) ||
                 dynamic_cast<OscOutputCommand *>(event.getCommand()) ||
                 dynamic_cast<HttpGetCommand *>(event.getCommand()) ||
                 dynamic_cast<HttpPostCommand *>(event.getCommand()) ||
                 dynamic_cast<PlayoutCommand *>(event.getCommand()))
        {
            this->comboBoxDevice->setEnabled(false);
            this->comboBoxTarget->setEnabled(false);
            this->spinBoxChannel->setEnabled(false);
            this->spinBoxVideolayer->setEnabled(false);
            this->spinBoxDuration->setEnabled(false);

            this->forceMilliseconds = true;

            this->comboBoxDevice->setCurrentIndex(-1);
            this->comboBoxTarget->setCurrentIndex(-1);
            this->spinBoxChannel->setValue(Output::DEFAULT_CHANNEL);
            this->spinBoxVideolayer->setValue(Output::DEFAULT_VIDEOLAYER);
            this->spinBoxDuration->setValue(Output::DEFAULT_DURATION);
        }
        else if (dynamic_cast<SeparatorCommand *>(event.getCommand()) ||
                 dynamic_cast<GatewayCommand *>(event.getCommand()))
        {
            this->comboBoxDevice->setEnabled(false);
            this->comboBoxTarget->setEnabled(false);
            this->spinBoxChannel->setEnabled(false);
            this->spinBoxVideolayer->setEnabled(false);
            this->spinBoxDelay->setEnabled(false);
            this->spinBoxDuration->setEnabled(false);
            this->checkBoxAllowGpi->setEnabled(false);
            this->checkBoxAllowRemoteTriggering->setEnabled(false);
            this->labelRemoteTriggerIdField->setEnabled(false);
            this->lineEditRemoteTriggerId->setEnabled(false);

            this->comboBoxDevice->setCurrentIndex(-1);
            this->comboBoxTarget->setCurrentIndex(-1);
            this->spinBoxChannel->setValue(Output::DEFAULT_CHANNEL);
            this->spinBoxVideolayer->setValue(Output::DEFAULT_VIDEOLAYER);
            this->spinBoxDelay->setValue(Output::DEFAULT_DELAY);
            this->spinBoxDuration->setValue(Output::DEFAULT_DURATION);
            this->checkBoxAllowGpi->setChecked(Output::DEFAULT_ALLOW_GPI);
            this->checkBoxAllowRemoteTriggering->setChecked(Output::DEFAULT_ALLOW_REMOTE_TRIGGERING);
            this->lineEditRemoteTriggerId->setText(Output::DEFAULT_REMOTE_TRIGGER_ID);
        }
        else if (dynamic_cast<CustomCommand *>(event.getCommand()))
        {
            this->comboBoxTarget->setEnabled(false);
            this->spinBoxChannel->setEnabled(false);
            this->spinBoxVideolayer->setEnabled(false);

            this->comboBoxTarget->setCurrentIndex(-1);
            this->spinBoxChannel->setValue(Output::DEFAULT_CHANNEL);
            this->spinBoxVideolayer->setValue(Output::DEFAULT_VIDEOLAYER);
        }
        else if (dynamic_cast<ClearOutputCommand *>(event.getCommand()) ||
                 dynamic_cast<ResetCommand *>(event.getCommand()))
        {
            this->comboBoxTarget->setEnabled(false);
            this->spinBoxDuration->setEnabled(false);

            this->comboBoxTarget->setCurrentIndex(-1);
            this->spinBoxDuration->setValue(Output::DEFAULT_DURATION);
        }
        else if (dynamic_cast<DeckLinkInputCommand *>(event.getCommand()) ||
                 dynamic_cast<BlendModeCommand *>(event.getCommand()) ||
                 dynamic_cast<BrightnessCommand *>(event.getCommand()) ||
                 dynamic_cast<ContrastCommand *>(event.getCommand()) ||
                 dynamic_cast<ClipCommand *>(event.getCommand()) ||
                 dynamic_cast<CropCommand *>(event.getCommand()) ||
                 dynamic_cast<FillCommand *>(event.getCommand()) ||
                 dynamic_cast<PerspectiveCommand *>(event.getCommand()) ||
                 dynamic_cast<RotationCommand *>(event.getCommand()) ||
                 dynamic_cast<AnchorCommand *>(event.getCommand()) ||
                 dynamic_cast<KeyerCommand *>(event.getCommand()) ||
                 dynamic_cast<LevelsCommand *>(event.getCommand()) ||
                 dynamic_cast<OpacityCommand *>(event.getCommand()) ||
                 dynamic_cast<SaturationCommand *>(event.getCommand()) ||
                 dynamic_cast<VolumeCommand *>(event.getCommand()) ||
                 dynamic_cast<SolidColorCommand *>(event.getCommand()) ||
                 dynamic_cast<FadeToBlackCommand *>(event.getCommand()) ||
                 dynamic_cast<HtmlCommand *>(event.getCommand()) ||
                 dynamic_cast<RouteChannelCommand *>(event.getCommand()) ||
                 dynamic_cast<RouteVideolayerCommand *>(event.getCommand()) ||
                 dynamic_cast<ChromaCommand *>(event.getCommand()))
        {
            this->comboBoxTarget->setEnabled(false);

            this->comboBoxTarget->setCurrentIndex(-1);
        }
    }

    updateUnitButtons();

    if (deviceModel != NULL && deviceModel->getLockedChannel() > 0 && deviceModel->getLockedChannel() <= this->spinBoxChannel->maximum())
    {
        this->spinBoxChannel->setEnabled(false);
        this->spinBoxChannel->setValue(deviceModel->getLockedChannel());

        // Manually trigger the changed channel slot because we actively blocking signals.
        channelChanged(deviceModel->getLockedChannel());
    }

    checkEmptyDevice();
    checkEmptyTarget();

    blockAllSignals(false);
}

void InspectorOutputWidget::libraryItemSelected(const LibraryItemSelectedEvent &event)
{
    this->model = event.getLibraryModel();

    blockAllSignals(true);

    this->comboBoxTarget->clear();

    this->comboBoxDevice->setVisible(true);

    this->comboBoxDevice->setEnabled(false);
    this->comboBoxTarget->setEnabled(false);
    this->spinBoxChannel->setEnabled(false);
    this->spinBoxVideolayer->setEnabled(false);
    this->spinBoxDelay->setEnabled(false);
    this->spinBoxDuration->setEnabled(false);
    this->checkBoxAllowGpi->setEnabled(false);
    this->checkBoxAllowRemoteTriggering->setEnabled(false);
    this->labelRemoteTriggerIdField->setEnabled(false);
    this->lineEditRemoteTriggerId->setEnabled(false);

    this->buttonDelayUnit->setText("");
    this->buttonDelayUnit->setVisible(false);
    this->buttonDurationUnit->setText("");
    this->buttonDurationUnit->setVisible(false);

    this->comboBoxDevice->setCurrentIndex(this->comboBoxDevice->findText(this->model->getDeviceName()));
    this->checkBoxAllowGpi->setChecked(Output::DEFAULT_ALLOW_GPI);
    this->checkBoxAllowRemoteTriggering->setChecked(Output::DEFAULT_ALLOW_REMOTE_TRIGGERING);
    this->lineEditRemoteTriggerId->setText(Output::DEFAULT_REMOTE_TRIGGER_ID);
    this->labelTriggerBank->setText("");

    fillTargetCombo(this->model->getType());

    checkEmptyDevice();
    checkEmptyTarget();

    this->comboBoxTarget->setStyleSheet("");

    blockAllSignals(false);
}

void InspectorOutputWidget::emptyRundown(const EmptyRundownEvent &event)
{
    Q_UNUSED(event);

    blockAllSignals(true);

    this->model = NULL;

    this->comboBoxTarget->clear();

    this->comboBoxDevice->setVisible(true);

    this->comboBoxDevice->setEnabled(false);
    this->comboBoxTarget->setEnabled(false);
    this->spinBoxChannel->setEnabled(false);
    this->spinBoxVideolayer->setEnabled(false);
    this->spinBoxDelay->setEnabled(false);
    this->spinBoxDuration->setEnabled(false);
    this->checkBoxAllowGpi->setEnabled(false);
    this->checkBoxAllowRemoteTriggering->setEnabled(false);
    this->labelRemoteTriggerIdField->setEnabled(false);
    this->lineEditRemoteTriggerId->setEnabled(false);

    this->buttonDelayUnit->setText("");
    this->buttonDelayUnit->setVisible(false);
    this->buttonDurationUnit->setText("");
    this->buttonDurationUnit->setVisible(false);

    this->comboBoxDevice->setCurrentIndex(-1);
    this->spinBoxChannel->setValue(Output::DEFAULT_CHANNEL);
    this->spinBoxVideolayer->setValue(Output::DEFAULT_VIDEOLAYER);
    this->spinBoxDelay->setValue(Output::DEFAULT_DELAY);
    this->spinBoxDuration->setValue(Output::DEFAULT_DURATION);
    this->checkBoxAllowGpi->setChecked(Output::DEFAULT_ALLOW_GPI);
    this->checkBoxAllowRemoteTriggering->setChecked(Output::DEFAULT_ALLOW_REMOTE_TRIGGERING);
    this->lineEditRemoteTriggerId->setText(Output::DEFAULT_REMOTE_TRIGGER_ID);
    this->labelTriggerBank->setText("");

    checkEmptyDevice();
    checkEmptyTarget();

    this->comboBoxTarget->setStyleSheet("");

    blockAllSignals(false);
}

void InspectorOutputWidget::deviceChanged(const DeviceChangedEvent &event)
{
    if (this->model == NULL)
        return;

    blockAllSignals(true);

    if (!event.getDeviceName().isEmpty())
        fillTargetCombo(this->model->getType(), event.getDeviceName());

    checkEmptyDevice();
    checkEmptyTarget();

    blockAllSignals(false);
}

void InspectorOutputWidget::mediaChanged(const MediaChangedEvent &event)
{
    Q_UNUSED(event);

    if (this->model == NULL)
        return;

    blockAllSignals(true);

    fillTargetCombo(this->model->getType());

    blockAllSignals(false);
}

void InspectorOutputWidget::templateChanged(const TemplateChangedEvent &event)
{
    Q_UNUSED(event);

    if (this->model == NULL)
        return;

    blockAllSignals(true);

    fillTargetCombo(this->model->getType());

    blockAllSignals(false);
}

void InspectorOutputWidget::blockAllSignals(bool block)
{
    this->comboBoxDevice->blockSignals(block);
    this->comboBoxTarget->blockSignals(block);
    this->spinBoxChannel->blockSignals(block);
    this->spinBoxVideolayer->blockSignals(block);
    this->spinBoxDelay->blockSignals(block);
    this->spinBoxDuration->blockSignals(block);
    this->checkBoxAllowGpi->blockSignals(block);
    this->checkBoxAllowRemoteTriggering->blockSignals(block);
    this->lineEditRemoteTriggerId->blockSignals(block);
}

void InspectorOutputWidget::fillTargetCombo(const QString &type, QString deviceName)
{
    this->comboBoxTarget->clear();

    if (this->model == NULL)
        return;

    if (deviceName.isEmpty())
        deviceName = this->model->getDeviceName();

    QSharedPointer<DeviceModel> deviceModel;

    if (!deviceName.isEmpty())
        deviceModel = DeviceManager::getInstance().getDeviceModelByName(deviceName);

    if (deviceModel)
    {
        QList<LibraryModel> models;
        if (this->libraryFilter.isEmpty())
            models = DatabaseManager::getInstance().getLibraryByDeviceId(deviceModel->getId());
        else
            models = DatabaseManager::getInstance().getLibraryByDeviceIdAndFilter(deviceModel->getId(), this->libraryFilter);

        if (models.count() > 0)
        {
            foreach (LibraryModel model, models)
            {
                if (type == Rundown::MOVIE && model.getType() == Rundown::MOVIE)
                    this->comboBoxTarget->addItem(model.getName());
                else if (type == Rundown::AUDIO && model.getType() == Rundown::AUDIO)
                    this->comboBoxTarget->addItem(model.getName());
                else if (type == Rundown::TEMPLATE && model.getType() == Rundown::TEMPLATE)
                    this->comboBoxTarget->addItem(model.getName());
                else if ((type == Rundown::STILL || type == Rundown::IMAGESCROLLER) && model.getType() == Rundown::STILL)
                    this->comboBoxTarget->addItem(model.getName());
            }
        }
    }

    this->comboBoxTarget->setCurrentIndex(this->comboBoxTarget->findText(this->model->getName()));
}

void InspectorOutputWidget::checkEmptyDevice()
{
    if (this->comboBoxDevice->isEnabled() && this->comboBoxDevice->currentText() == "")
        this->comboBoxDevice->setStyleSheet("border-color: firebrick;");
    else
        this->comboBoxDevice->setStyleSheet("");
}

void InspectorOutputWidget::checkEmptyTarget()
{
    if (this->libraryFilter.isEmpty())
    {
        if (this->comboBoxTarget->isEnabled() && this->comboBoxTarget->currentText() == "")
            this->comboBoxTarget->setStyleSheet("border-color: firebrick;");
        else
            this->comboBoxTarget->setStyleSheet("");
    }
    else
    {
        if (this->comboBoxTarget->isEnabled() && this->comboBoxTarget->currentText() == "")
            this->comboBoxTarget->setStyleSheet("border-color: firebrick;");
        else if (this->comboBoxTarget->isEnabled())
            this->comboBoxTarget->setStyleSheet("border-color: darkorange;");
    }
}

void InspectorOutputWidget::deviceRemoved()
{
    blockAllSignals(true);

    this->comboBoxDevice->clear();
    foreach (const DeviceModel &model, DeviceManager::getInstance().getDeviceModels())
        this->comboBoxDevice->addItem(model.getName());

    blockAllSignals(false);
}

void InspectorOutputWidget::deviceAdded(CasparDevice &device)
{
    blockAllSignals(true);

    const QSharedPointer<DeviceModel> model = DeviceManager::getInstance().getDeviceModelByAddress(device.getAddress());
    if (model == NULL || model->getShadow() == "Yes")
        return; // Don't add shadow systems.

    int index = this->comboBoxDevice->currentIndex();

    this->comboBoxDevice->addItem(model->getName());

    if (index == -1)
        this->comboBoxDevice->setCurrentIndex(index);

    blockAllSignals(false);
}

void InspectorOutputWidget::deviceNameChanged(QString deviceName)
{
    if (deviceName.isEmpty())
        return;

    const QSharedPointer<DeviceModel> model = DeviceManager::getInstance().getDeviceModelByName(deviceName);
    const QStringList &channelFormats = DatabaseManager::getInstance().getDeviceByName(model->getName()).getChannelFormats().split(",");
    this->spinBoxChannel->setMaximum(channelFormats.count());

    if (model->getLockedChannel() > 0 && model->getLockedChannel() <= this->spinBoxChannel->maximum())
    {
        this->spinBoxChannel->setEnabled(false);
        this->spinBoxChannel->setValue(model->getLockedChannel());
    }
    else
    {
        this->spinBoxChannel->setEnabled(true);
        this->spinBoxChannel->setValue(Output::DEFAULT_CHANNEL);
    }

    checkEmptyDevice();
    checkEmptyTarget();

    EventManager::getInstance().fireDeviceChangedEvent(DeviceChangedEvent(this->comboBoxDevice->currentText()));
}

void InspectorOutputWidget::targetChanged(QString name)
{
    Q_UNUSED(name);

    checkEmptyTarget();

    EventManager::getInstance().fireTargetChangedEvent(TargetChangedEvent(this->comboBoxTarget->currentText()));
    if (this->model->getLabel() == this->comboBoxTarget->getPreviousText())
        EventManager::getInstance().fireLabelChangedEvent(LabelChangedEvent(this->comboBoxTarget->currentText()));
}

void InspectorOutputWidget::channelChanged(int channel)
{
    if (this->command == NULL)
        return;

    // Always set on the primary command, even if allCommands is empty or stale.
    this->command->setChannel(channel);

    for (AbstractCommand* cmd : this->allCommands)
    {
        if (cmd != this->command)
            cmd->setChannel(channel);
    }

    EventManager::getInstance().fireChannelChangedEvent(ChannelChangedEvent(channel));
}

void InspectorOutputWidget::videolayerChanged(int videolayer)
{
    if (this->command != NULL)
        this->command->setVideolayer(videolayer);

    for (AbstractCommand* cmd : this->allCommands)
    {
        if (cmd != this->command)
            cmd->setVideolayer(videolayer);
    }

    EventManager::getInstance().fireVideolayerChangedEvent(VideolayerChangedEvent(videolayer));
}

void InspectorOutputWidget::delayChanged(int delay)
{
    if (this->command != NULL)
        this->command->setDelay(delay);

    for (AbstractCommand* cmd : this->allCommands)
    {
        if (cmd != this->command)
            cmd->setDelay(delay);
    }
}

void InspectorOutputWidget::durationChanged(int duration)
{
    if (this->command != NULL)
        this->command->setDuration(duration);

    for (AbstractCommand* cmd : this->allCommands)
    {
        if (cmd != this->command)
            cmd->setDuration(duration);
    }
}

void InspectorOutputWidget::allowGpiChanged(int state)
{
    bool checked = (state == Qt::Checked) ? true : false;
    for (AbstractCommand* cmd : this->allCommands)
        cmd->setAllowGpi(checked);
}

void InspectorOutputWidget::allowRemoteTriggeringChanged(int state)
{
    bool checked = (state == Qt::Checked) ? true : false;
    for (AbstractCommand* cmd : this->allCommands)
        cmd->setAllowRemoteTriggering(checked);

    this->labelRemoteTriggerIdField->setEnabled(this->command->getAllowRemoteTriggering());
    this->lineEditRemoteTriggerId->setEnabled(this->command->getAllowRemoteTriggering());

    // Re-trigger remoteTriggerIdChanged so the rundown widget label updates.
    for (AbstractCommand* cmd : this->allCommands)
        cmd->setRemoteTriggerId(cmd->getRemoteTriggerId());
}

void InspectorOutputWidget::remoteTriggerIdChanged(QString id)
{
    this->command->setRemoteTriggerId(id);
}

void InspectorOutputWidget::bankAssignmentChanged(const BankAssignmentChangedEvent& event)
{
    if (this->command == nullptr)
        return;

    int bank = this->command->getTriggerBank();
    if (bank > 0)
    {
        QString hotkey = DatabaseManager::getInstance().getConfigurationByName(QString("HotkeyBank%1").arg(bank)).getValue();
        this->labelTriggerBank->setText(QString("Bank %1 (%2)").arg(bank).arg(hotkey.isEmpty() ? "-" : hotkey));
    }
    else
    {
        this->labelTriggerBank->setText("");
    }
}

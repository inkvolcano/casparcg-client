#include "PresenterOutputDialog.h"

#include "DatabaseManager.h"
#include "DeviceManager.h"
#include "LiveRunSheet.h"
#include "PresenterOutputRules.h"
#include "SheetCacheServer.h"

#include "CasparDevice.h"
#include "Models/DeviceModel.h"

#include <QtCore/QTime>

#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QVBoxLayout>

namespace
{
    QString setting(const QString& name)
    {
        return DatabaseManager::getInstance().getConfigurationByName(name).getValue();
    }

    void remember(const QString& name, const QString& value)
    {
        DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, name, value));
    }

    // Where the page was last put on; empty server when it is not on anywhere.
    struct Place
    {
        QString server;
        int channel = 0;
        int layer = 0;
    };

    Place lastPlace()
    {
        Place p;
        if (setting("PresenterOutputOn") == "true")
        {
            p.server = setting("PresenterOutputServer");
            p.channel = setting("PresenterOutputChannel").toInt();
            p.layer = setting("PresenterOutputLayer").toInt();
        }
        return p;
    }
}

// The run sheet's token, when one is set: the server's link must carry it.
static QString runSheetToken()
{
    return DatabaseManager::getInstance().getConfigurationByName("LiveRunSheetToken").getValue().trimmed();
}

void PresenterOutputDialog::open(QWidget* parent)
{
    PresenterOutputDialog* dialog = new PresenterOutputDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

PresenterOutputDialog::PresenterOutputDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Presenter View On A Channel");
    resize(560, 0);

    QVBoxLayout* layout = new QVBoxLayout(this);

    QLabel* about = new QLabel("The presenter view - what is on air with its countdown, and what comes next with its note - "
                               "played by a server on one of its channels, for a confidence monitor. It fills the whole "
                               "picture: choose a channel that goes to a monitor, not to air.", this);
    about->setWordWrap(true);
    layout->addWidget(about);

    QHBoxLayout* serving = new QHBoxLayout();
    this->servingLabel = new QLabel(this);
    this->servingLabel->setWordWrap(true);
    serving->addWidget(this->servingLabel, 1);
    this->buttonServe = new QPushButton("Serve The Live Run Sheet", this);
    this->buttonServe->setToolTip("Turns on Settings > General > Serve a live run sheet on the network.\n"
                                  "Anyone on the network can then open the run sheet; it changes nothing.");
    serving->addWidget(this->buttonServe);
    layout->addLayout(serving);

    QFormLayout* form = new QFormLayout();
    this->comboServer = new QComboBox(this);
    foreach (const DeviceModel& model, DeviceManager::getInstance().getDeviceModels())
        this->comboServer->addItem(model.getName());
    form->addRow("Server", this->comboServer);
    this->spinChannel = new QSpinBox(this);
    this->spinChannel->setRange(1, 99);
    form->addRow("Channel", this->spinChannel);
    this->spinLayer = new QSpinBox(this);
    this->spinLayer->setRange(0, 9999);
    this->spinLayer->setValue(999);
    form->addRow("Layer", this->spinLayer);
    layout->addLayout(form);

    // The form starts where it was last used.
    const QString server = setting("PresenterOutputServer");
    if (!server.isEmpty() && this->comboServer->findText(server) >= 0)
        this->comboServer->setCurrentIndex(this->comboServer->findText(server));
    serverChosen();
    if (setting("PresenterOutputChannel").toInt() > 0)
        this->spinChannel->setValue(setting("PresenterOutputChannel").toInt());
    if (!setting("PresenterOutputLayer").isEmpty())
        this->spinLayer->setValue(setting("PresenterOutputLayer").toInt());

    this->statusLabel = new QLabel(this);
    this->statusLabel->setWordWrap(true);
    this->statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(this->statusLabel);

    QDialogButtonBox* buttons = new QDialogButtonBox(this);
    this->buttonOn = buttons->addButton("Put On", QDialogButtonBox::ActionRole);
    this->buttonOff = buttons->addButton("Take Off", QDialogButtonBox::ActionRole);
    buttons->addButton(QDialogButtonBox::Close);
    layout->addWidget(buttons);

    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    QObject::connect(this->buttonServe, &QPushButton::clicked, this, [this]() {
        LiveRunSheet::setEnabled(true);
        SheetCacheServer::getInstance().start();
        refresh();
    });
    QObject::connect(this->comboServer, &QComboBox::currentTextChanged, this, [this]() { serverChosen(); refresh(); });
    QObject::connect(this->spinChannel, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { refresh(); });
    QObject::connect(this->buttonOn, &QPushButton::clicked, this, [this]() { putOn(); });
    QObject::connect(this->buttonOff, &QPushButton::clicked, this, [this]() { takeOff(); });

    refresh();
}

void PresenterOutputDialog::serverChosen()
{
    const QSharedPointer<DeviceModel> model = DeviceManager::getInstance().getDeviceModelByName(this->comboServer->currentText());
    const int channels = (model != nullptr) ? model->getChannels() : 0;
    this->spinChannel->setRange(1, channels > 0 ? channels : 99);
}

void PresenterOutputDialog::refresh()
{
    const bool serving = LiveRunSheet::isEnabled() && SheetCacheServer::getInstance().isRunning();
    if (serving)
        this->servingLabel->setText(QString("The live run sheet is served on port %1.").arg(SheetCacheServer::configuredPort()));
    else if (LiveRunSheet::isEnabled())
        this->servingLabel->setText(QString("The live run sheet is on, but the client's web server is not running - "
                                            "port %1 may be in use by another program.").arg(SheetCacheServer::configuredPort()));
    else
        this->servingLabel->setText("The server fetches the page from this client, so the live run sheet has to be served first.");
    this->buttonServe->setVisible(!LiveRunSheet::isEnabled());

    const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(this->comboServer->currentText());
    const bool connected = device != nullptr && device->isConnected();

    const Place place = lastPlace();
    this->buttonOff->setEnabled(!place.server.isEmpty());
    this->buttonOff->setText(place.server.isEmpty() ? QString("Take Off")
                                                    : QString("Take Off %1-%2").arg(place.channel).arg(place.layer));
    this->buttonOn->setEnabled(serving && connected && this->comboServer->count() > 0);

    QString status;
    if (this->comboServer->count() == 0)
        status = "No server is set up. Add one in Settings > Devices.";
    else if (!connected)
        status = QString("%1 is not connected.").arg(this->comboServer->currentText());
    else if (serving)
    {
        const QString url = PresenterOutputRules::pageUrl(device->localAddress(), SheetCacheServer::configuredPort(), runSheetToken());
        status = QString("The server will play %1").arg(url.isEmpty() ? QString("- no address to give it") : url);
    }
    if (!place.server.isEmpty())
        status += QString("%1On %2, channel %3, layer %4.").arg(status.isEmpty() ? "" : "\n\n").arg(place.server).arg(place.channel).arg(place.layer);
    this->statusLabel->setText(status);
}

void PresenterOutputDialog::putOn()
{
    const QString server = this->comboServer->currentText();
    const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(server);
    const QSharedPointer<DeviceModel> model = DeviceManager::getInstance().getDeviceModelByName(server);
    if (device == nullptr || !device->isConnected())
        return;

    const int channel = this->spinChannel->value();
    const int layer = this->spinLayer->value();
    if (!PresenterOutputRules::validPlace(channel, layer, model != nullptr ? model->getChannels() : 0))
        return;

    const QString url = PresenterOutputRules::pageUrl(device->localAddress(), SheetCacheServer::configuredPort(), runSheetToken());
    if (url.isEmpty())
    {
        this->statusLabel->setText("There is no address of this machine the server could reach.");
        return;
    }

    // Moved, not doubled: the page comes off the place it was on first.
    const Place before = lastPlace();
    if (!before.server.isEmpty() && (before.server != server || before.channel != channel || before.layer != layer))
    {
        const QSharedPointer<CasparDevice> old = DeviceManager::getInstance().getDeviceByName(before.server);
        if (old != nullptr && old->isConnected())
            old->sendCommand(PresenterOutputRules::clearCommand(before.channel, before.layer));
    }

    device->sendCommand(PresenterOutputRules::playCommand(channel, layer, url));

    remember("PresenterOutputServer", server);
    remember("PresenterOutputChannel", QString::number(channel));
    remember("PresenterOutputLayer", QString::number(layer));
    remember("PresenterOutputOn", "true");
    qInfo("Presenter view put on %s channel %d layer %d: %s", qPrintable(server), channel, layer, qPrintable(url));

    refresh();
    this->statusLabel->setText(this->statusLabel->text() +
                               QString("\n\nIf the monitor stays black or dimmed, the server cannot reach this machine on port %1 - "
                                       "allow the client through Windows Firewall.").arg(SheetCacheServer::configuredPort()));
}

void PresenterOutputDialog::takeOff()
{
    const Place place = lastPlace();
    if (place.server.isEmpty())
        return;

    const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(place.server);
    if (device == nullptr || !device->isConnected())
    {
        this->statusLabel->setText(QString("%1 is not connected; the page is still on channel %2, layer %3.")
                                   .arg(place.server).arg(place.channel).arg(place.layer));
        return;
    }

    device->sendCommand(PresenterOutputRules::clearCommand(place.channel, place.layer));
    remember("PresenterOutputOn", "false");
    qInfo("Presenter view taken off %s channel %d layer %d", qPrintable(place.server), place.channel, place.layer);
    refresh();
}

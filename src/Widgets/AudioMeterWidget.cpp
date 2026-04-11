#include "AudioMeterWidget.h"

#include "Global.h"

#include "DatabaseManager.h"
#include "DeviceManager.h"
#include "EventManager.h"

#include <cmath>

#include <QtGui/QPainter>

// Block meter constants.
static const int BLOCK_COUNT = 10;
static const int BLOCK_X = 3;
static const int BLOCK_WIDTH = 26;
static const int BLOCK_HEIGHT = 7;
static const int BLOCK_GAP = 1;
static const int METER_TOP = 3;  // y offset for topmost block

// dB range: -61 to 0.
static const double DB_MIN = -61.0;
static const double DB_MAX = 0.0;
static const double DB_RANGE = DB_MAX - DB_MIN;

// Active (lit) colors per block — bottom (index 0) to top (index 9).
static const QColor BLOCK_COLORS_ON[BLOCK_COUNT] = {
    QColor(0x00, 0xaa, 0x00),  // 0: green
    QColor(0x00, 0xbb, 0x00),  // 1: green
    QColor(0x00, 0xcc, 0x00),  // 2: green
    QColor(0x22, 0xcc, 0x00),  // 3: green
    QColor(0x55, 0xcc, 0x00),  // 4: green-yellow
    QColor(0x99, 0xcc, 0x00),  // 5: yellow-green
    QColor(0xcc, 0xcc, 0x00),  // 6: yellow
    QColor(0xdd, 0x99, 0x00),  // 7: orange
    QColor(0xee, 0x44, 0x00),  // 8: orange-red
    QColor(0xee, 0x00, 0x00),  // 9: red
};

// Dim (off) colors — same hue at very low brightness.
static const QColor BLOCK_COLORS_OFF[BLOCK_COUNT] = {
    QColor(0x08, 0x22, 0x08),
    QColor(0x08, 0x24, 0x08),
    QColor(0x08, 0x28, 0x08),
    QColor(0x0a, 0x28, 0x08),
    QColor(0x12, 0x28, 0x08),
    QColor(0x1e, 0x28, 0x08),
    QColor(0x28, 0x28, 0x08),
    QColor(0x2a, 0x1e, 0x08),
    QColor(0x2e, 0x10, 0x08),
    QColor(0x2e, 0x08, 0x08),
};

AudioMeterWidget::AudioMeterWidget(QWidget* parent)
    : QWidget(parent),
      channel(-1), currentLevel(DB_MIN), model(NULL), command(NULL), audioSubscription(NULL)
{
    setupUi(this);

    QObject::connect(&EventManager::getInstance(), SIGNAL(deviceChanged(const DeviceChangedEvent&)), this, SLOT(deviceChanged(const DeviceChangedEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(channelChanged(const ChannelChangedEvent&)), this, SLOT(channelChanged(const ChannelChangedEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(emptyRundown(const EmptyRundownEvent&)), this, SLOT(emptyRundown(const EmptyRundownEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(rundownItemSelected(const RundownItemSelectedEvent&)), this, SLOT(rundownItemSelected(const RundownItemSelectedEvent&)));
}

void AudioMeterWidget::configureAudioMeter(int channel)
{
    this->channel = channel;
    this->labelAudioMeterChannel->setText(QString("Ch %1").arg(channel));
}

void AudioMeterWidget::configureForDevice(int audioChannel, const QString& deviceName, int serverChannel)
{
    this->channel = audioChannel;
    this->directMode = true;
    this->labelAudioMeterChannel->setText(QString("Ch %1").arg(audioChannel));

    delete this->audioSubscription;
    this->audioSubscription = nullptr;

    const auto device = DeviceManager::getInstance().getDeviceByName(deviceName);
    if (device == nullptr)
        return;

    QString audioFilter = Osc::AUDIOCHANNEL_FILTER;
    audioFilter.replace("#IPADDRESS#", device->resolveIpAddress())
               .replace("#CHANNEL#", QString::number(serverChannel));
    this->audioSubscription = new OscSubscription(audioFilter, this);
    QObject::connect(this->audioSubscription, SIGNAL(subscriptionReceived(const QString&, const QList<QVariant>&)),
                     this, SLOT(audioSubscriptionReceived(const QString&, const QList<QVariant>&)));
}

void AudioMeterWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    // Calculate how many blocks should be lit.
    // Each block covers DB_RANGE / BLOCK_COUNT dB.
    double dbPerBlock = DB_RANGE / BLOCK_COUNT;

    for (int i = 0; i < BLOCK_COUNT; i++)
    {
        // Block 0 is at the bottom, block 9 at the top.
        double threshold = DB_MIN + i * dbPerBlock;
        bool lit = (this->currentLevel >= threshold) && (this->currentLevel > DB_MIN);

        // Y position: top block (index 9) is at METER_TOP, bottom block (index 0) is lowest.
        int blockIndex = BLOCK_COUNT - 1 - i;
        int y = METER_TOP + blockIndex * (BLOCK_HEIGHT + BLOCK_GAP);

        painter.fillRect(BLOCK_X, y, BLOCK_WIDTH, BLOCK_HEIGHT,
                         lit ? BLOCK_COLORS_ON[i] : BLOCK_COLORS_OFF[i]);
    }
}

void AudioMeterWidget::deviceChanged(const DeviceChangedEvent& event)
{
    if (this->directMode)
        return;

    if (this->model == NULL)
        return;

    if (!event.getDeviceName().isEmpty() && event.getDeviceName() != this->model->getDeviceName())
    {
        delete this->audioSubscription;
        this->audioSubscription = NULL;

        if (DeviceManager::getInstance().getDeviceByName(event.getDeviceName()) == NULL)
            return;

        this->currentLevel = DB_MIN;
        update();

        QString audioFilter = Osc::AUDIOCHANNEL_FILTER;
        audioFilter.replace("#IPADDRESS#", QString("%1").arg(DeviceManager::getInstance().getDeviceByName(event.getDeviceName())->resolveIpAddress()))
                   .replace("#CHANNEL#", QString("%1").arg(this->command->getChannel()));
        this->audioSubscription = new OscSubscription(audioFilter, this);
        QObject::connect(this->audioSubscription, SIGNAL(subscriptionReceived(const QString&, const QList<QVariant>&)),
                         this, SLOT(audioSubscriptionReceived(const QString&, const QList<QVariant>&)));
    }
}

void AudioMeterWidget::channelChanged(const ChannelChangedEvent& event)
{
    if (this->directMode)
        return;

    Q_UNUSED(event);

    configureOscSubscriptions();
}

void AudioMeterWidget::emptyRundown(const EmptyRundownEvent& event)
{
    if (this->directMode)
        return;

    Q_UNUSED(event);

    delete this->audioSubscription;
    this->audioSubscription = NULL;

    this->model = NULL;

    this->currentLevel = DB_MIN;
    update();
}

void AudioMeterWidget::rundownItemSelected(const RundownItemSelectedEvent& event)
{
    if (this->directMode)
        return;

    this->model = event.getLibraryModel();
    this->command = dynamic_cast<AbstractCommand*>(event.getCommand());

    configureOscSubscriptions();
}

void AudioMeterWidget::configureOscSubscriptions()
{
    delete this->audioSubscription;
    this->audioSubscription = nullptr;

    if (this->model == NULL)
        return;

    if (DeviceManager::getInstance().getDeviceByName(this->model->getDeviceName()) == NULL)
        return;

    QString audioFilter = Osc::AUDIOCHANNEL_FILTER;
    audioFilter.replace("#IPADDRESS#", QString("%1").arg(DeviceManager::getInstance().getDeviceByName(this->model->getDeviceName())->resolveIpAddress()))
               .replace("#CHANNEL#", QString("%1").arg(this->command->getChannel()));
    this->audioSubscription = new OscSubscription(audioFilter, this);
    QObject::connect(this->audioSubscription, SIGNAL(subscriptionReceived(const QString&, const QList<QVariant>&)),
                     this, SLOT(audioSubscriptionReceived(const QString&, const QList<QVariant>&)));
}

void AudioMeterWidget::audioSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    double value = convertToLevel(arguments.at(this->channel - 1).toInt());
    if (value < DB_MIN)
        value = DB_MIN;

    this->currentLevel = value;
    update();
}

double AudioMeterWidget::convertToLevel(int value)
{
    auto MIN_PFS = 0.5 / static_cast<double>(std::numeric_limits<int32_t>::max());
    auto pFS = value / static_cast<double>(std::numeric_limits<int32_t>::max());

    return 20.0 * std::log10((std::max(MIN_PFS, pFS)));
}

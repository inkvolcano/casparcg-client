#include "ThumbnailWorker.h"
#include "DatabaseManager.h"
#include "DeviceManager.h"
#include "EventManager.h"
#include "Events/MediaChangedEvent.h"
#include "Events/StatusbarEvent.h"

#include "Global.h"

#include "CasparDevice.h"

#include <QtCore/QTimer>

#include <QtWidgets/QApplication>

ThumbnailWorker::ThumbnailWorker(const QList<ThumbnailModel>& thumbnailModels, QObject* parent)
    : QObject(parent),
      thumbnailModels(thumbnailModels)
{
    this->thumbnailTimer.setSingleShot(true);
    QObject::connect(&this->thumbnailTimer, SIGNAL(timeout()), this, SLOT(process()));
}

void ThumbnailWorker::start()
{
    // The first request waits as long as it always did, so a refresh that has just
    // asked the server for its lists is not answered with a burst straight away.
    this->thumbnailTimer.start(WAIT_FOR_ANSWER_MS);
}

void ThumbnailWorker::process()
{
    if (this->thumbnailModels.count() == 0)
    {
        this->thumbnailTimer.stop();
        EventManager::getInstance().fireMediaChangedEvent(MediaChangedEvent());

        return;
    }

    this->currentName = this->thumbnailModels.at(0).getName();
    this->currentAddress = this->thumbnailModels.at(0).getAddress();
    this->currentTimestamp= this->thumbnailModels.at(0).getTimestamp();
    this->currentSize= this->thumbnailModels.at(0).getSize();

    const QSharedPointer<DeviceModel> model = DeviceManager::getInstance().getDeviceModelByAddress(this->currentAddress);
    if (model == NULL || model->getShadow() == "Yes")
    {
        // Dropped, not retried. Returning with the entry still at the head meant
        // the same entry was looked at again every two seconds for as long as the
        // client ran - the timer never stopped for a removed or shadow server.
        this->thumbnailModels.removeAt(0);
        this->thumbnailTimer.start(GAP_AFTER_ANSWER_MS);
        return;
    }

    const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(model->getName());
    if (!device->isConnected())
    {
        this->thumbnailTimer.stop();
        EventManager::getInstance().fireMediaChangedEvent(MediaChangedEvent());

        return;
    }

    EventManager::getInstance().fireStatusbarEvent(StatusbarEvent(QString("Retrieving thumbnail %1...").arg(this->currentName)));
    qDebug("Retrieving thumbnail %s", qPrintable(this->currentName));

    // Once. This ran on every tick, and a reply slower than two seconds found a
    // second connection added before the first was removed, so it was stored
    // twice.
    QObject::connect(device.data(), SIGNAL(thumbnailRetrieveChanged(const QString&, CasparDevice&)), this, SLOT(thumbnailRetrieveChanged(const QString&, CasparDevice&)), Qt::UniqueConnection);
    device->retrieveThumbnail(this->currentName);

    this->thumbnailModels.removeAt(0);

    // Given up on if no answer comes; an answer brings the next request forward.
    this->thumbnailTimer.start(WAIT_FOR_ANSWER_MS);
}

void ThumbnailWorker::thumbnailRetrieveChanged(const QString& data, CasparDevice& device)
{
    QObject::disconnect(&device, SIGNAL(thumbnailRetrieveChanged(const QString&, CasparDevice&)), this, SLOT(thumbnailRetrieveChanged(const QString&, CasparDevice&)));
    DatabaseManager::getInstance().updateThumbnail(ThumbnailModel(0, data, this->currentTimestamp, this->currentSize, this->currentName, this->currentAddress));

    // Answered: the next one shortly, rather than when the two seconds are up.
    if (this->thumbnailTimer.isActive())
        this->thumbnailTimer.start(GAP_AFTER_ANSWER_MS);
}

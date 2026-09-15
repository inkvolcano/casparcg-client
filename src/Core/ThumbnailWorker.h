#pragma once

#include "Shared.h"

#include "Models/CasparData.h"
#include "CasparDevice.h"
#include "Models/CasparMedia.h"
#include "Models/CasparTemplate.h"
#include "Models/CasparThumbnail.h"

#include "Models/ThumbnailModel.h"

#include <QtCore/QObject>
#include <QtCore/QTimer>

class CORE_EXPORT ThumbnailWorker : public QObject
{
    Q_OBJECT

    public:
        explicit ThumbnailWorker(const QList<ThumbnailModel>& thumbnailModels, QObject* parent = 0);

        void start();

    private:
        // Paced by the server's answers rather than by a fixed clock. One thumbnail
        // every two seconds, answered or not, meant a thousand new clips took more
        // than half an hour to get their pictures in the Library. The next request
        // now goes out a quarter of a second after an answer; a request that is not
        // answered is given up on after two seconds, as long as a request ever got
        // before, so no case is slower than it was.
        static const int GAP_AFTER_ANSWER_MS = 250;
        static const int WAIT_FOR_ANSWER_MS = 2000;

        QTimer thumbnailTimer;

        QString currentName;
        QString currentTimestamp;
        QString currentSize;
        QString currentAddress;

        QList<ThumbnailModel> thumbnailModels;

        Q_SLOT void process();
        Q_SLOT void thumbnailRetrieveChanged(const QString& data, CasparDevice& device);
};

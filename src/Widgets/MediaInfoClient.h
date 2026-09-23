#pragma once

#include "Shared.h"

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>

#include <functional>

class QNetworkAccessManager;

// Asks a server's media scanner what a file is, for the Inspector's Media line
// (src/Common/MediaInfoSummary.h writes the line). The scanner runs beside every
// CasparCG server on port 8000 and keeps ffprobe's findings for each file, so
// the client opens no file itself - the user's choice - and a server on another
// machine works the same as a local one.
//
// Answers are kept for ten minutes, a scanner that did not answer for thirty
// seconds, and all of them are dropped when the Library is refreshed.
class WIDGETS_EXPORT MediaInfoClient : public QObject
{
    Q_OBJECT

    public:
        static const int SCANNER_PORT = 8000;

        static MediaInfoClient& getInstance();

        // done(text, known) runs on the GUI thread, and not at all if context has
        // gone. known is false when the text says why nothing is known.
        void request(const QString& deviceName, const QString& mediaName, bool still,
                     QObject* context, const std::function<void(const QString&, bool)>& done);

    private:
        explicit MediaInfoClient(QObject* parent = nullptr);

        struct Entry
        {
            QString text;
            bool known = false;
            qint64 at = 0;
        };

        QNetworkAccessManager* network = nullptr;
        QHash<QString, Entry> cache;

        Q_SLOT void forget();
};

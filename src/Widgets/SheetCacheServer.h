#pragma once

#include "Shared.h"

#include <QtCore/QByteArray>
#include <QtCore/QDateTime>
#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QString>

class QTcpServer;
class QTcpSocket;

// The sheet cache, hosted by the client itself.
//
// The templates already know how to talk to a cache: they ask for a spreadsheet
// and a sheet number and either get rows back or a 404 that sends them to Google.
// This answers exactly that, on exactly those parameters, so a template pointed at
// this port needs no change at all.
//
//   GET  <anything>?spreadsheetId=X&sheetNumber=N   cached rows, or 404
//   POST <anything>?spreadsheetId=X&sheetNumber=N   store rows (a template warming us)
//   GET  /strain                                    reads in the last minute, totalled
//   GET  /strain?tick=1&spreadsheetId=X             a template reporting one read
//   GET  /bypass  |  /bypass?on=1  |  /bypass?on=0  read or flip bypass
//
// Bypass is the switch that matters during a show: reads answer 404 so graphics go
// live to the sheet, while writes still land, so the cache stays warm for the moment
// it is turned back on.
class WIDGETS_EXPORT SheetCacheServer : public QObject
{
    Q_OBJECT

    public:
        static SheetCacheServer& getInstance();

        // Starts only when enabled in the settings; safe to call again.
        void start();
        void stop();
        bool isRunning() const;
        int listeningPort() const;

        static bool isEnabled();
        static int configuredPort();
        static QString cacheDirectory();

        // What the cache is holding, and getting rid of it. Only files named the way
        // this service names them are touched, because the folder can be shared with
        // the PHP service and with whatever else the operator keeps in there.
        static int cachedSheetCount(qint64* totalBytes = nullptr);
        static int clearCache(QString* error = nullptr);

        static bool isBypassing();
        static void setBypassing(bool bypass);

    private:
        explicit SheetCacheServer();

        Q_SLOT void acceptConnection();
        Q_SLOT void readFromSocket();
        Q_SLOT void forgetSocket();

        void handle(QTcpSocket* socket, const QString& method, const QString& target,
                    const QByteArray& body, const QString& pushToken);
        void respond(QTcpSocket* socket, int status, const QByteArray& body,
                     const QDateTime& cachedAt = QDateTime());

        static QString cacheFilePath(const QString& spreadsheetId, const QString& sheetNumber);

        QTcpServer* server = nullptr;
        QMap<QTcpSocket*, QByteArray> buffers;
};

#pragma once

#include "Shared.h"

#include <QtCore/QDate>
#include <QtCore/QFile>
#include <QtCore/QObject>
#include <QtCore/QString>

class CasparDevice;

// Writes the as-run log: one CSV file a day in ~/.CasparCG/Client/AsRun, a line
// for every command that put something on air or took it off, on every server,
// at the moment it was sent. Which commands count, and how a line reads, is in
// src/Common/AsRunLog.h. Files older than 30 days are removed when a new day's
// file is opened. File > Export As-Run Log... copies a day out.
class WIDGETS_EXPORT AsRunLogWriter : public QObject
{
    Q_OBJECT

    public:
        explicit AsRunLogWriter(QObject* parent = nullptr);

        static QString folder();

    private:
        QFile file;
        QDate fileDate;

        void write(const QString& line);
        static QString serverNameOf(const QObject* device);

        Q_SLOT void deviceAdded(CasparDevice& device);
        Q_SLOT void commandSent(const QString& message);
};

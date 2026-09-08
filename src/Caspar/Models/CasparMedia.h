#pragma once

#include "../Shared.h"

#include <QtCore/QString>

class CASPAR_EXPORT CasparMedia
{
    public:
        // The size and the timestamp arrive on every CLS line and were discarded
        // for years, which is why the Library could only ever sort by name.
        // Defaulted so the old three-argument form still compiles.
        explicit CasparMedia(const QString& name, const QString& type, const QString& timecode,
                             qint64 size = -1, const QString& timestamp = QString());

        const QString& getName() const;
        const QString& getType() const;
        const QString& getTimecode() const;
        qint64 getSize() const;
        const QString& getTimestamp() const;

    private:
        QString name;
        QString type;
        QString timecode;
        qint64 size;
        QString timestamp;
};

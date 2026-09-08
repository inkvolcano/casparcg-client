#pragma once

#include "../Shared.h"

#include <QtCore/QObject>
#include <QtCore/QString>

class CORE_EXPORT LibraryModel
{
    public:
        explicit LibraryModel() { }
        // Size and timestamp come from the server's own listing and were
        // discarded for years. Defaulted, because a dozen call sites build one of
        // these from places that have no such thing - a local folder walk, an
        // OGraf manifest - and an unknown size has to stay unknown rather than
        // become zero.
        explicit LibraryModel(int id, const QString& label, const QString& name, const QString& deviceName, const QString& type, int thumbnailId, const QString& timecode,
                              qint64 size = -1, const QString& timestamp = QString());

        int getId() const;
        const QString& getLabel() const;
        const QString& getName() const;
        const QString& getDeviceName() const;
        const QString& getType() const;
        int getThumbnailId() const;
        const QString& getTimecode() const;
        qint64 getSize() const;
        const QString& getTimestamp() const;

        void setLabel(const QString& label);
        void setName(const QString& name);
        void setDeviceName(const QString& deviceName);
        void setTimecode(const QString& timecode);

    private:
        int id;
        QString label;
        QString name;
        QString deviceName;
        QString type;
        int thumbnailId;
        QString timecode;
        qint64 size;
        QString timestamp;
};

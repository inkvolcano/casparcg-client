#include "CasparMedia.h"

CasparMedia::CasparMedia(const QString& name, const QString& type, const QString& timecode,
                         qint64 size, const QString& timestamp)
    : name(name), type(type), timecode(timecode), size(size), timestamp(timestamp)
{
}

const QString& CasparMedia::getName() const
{
    return this->name;
}

const QString& CasparMedia::getType() const
{
    return this->type;
}

const QString& CasparMedia::getTimecode() const
{
    return this->timecode;
}

qint64 CasparMedia::getSize() const
{
    return this->size;
}

const QString& CasparMedia::getTimestamp() const
{
    return this->timestamp;
}

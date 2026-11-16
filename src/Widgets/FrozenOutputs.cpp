#include "FrozenOutputs.h"

FrozenOutputs& FrozenOutputs::getInstance()
{
    static FrozenOutputs* instance = new FrozenOutputs();
    return *instance;
}

void FrozenOutputs::report(QObject* source, const QString& deviceName, int channel, bool isFrozenNow)
{
    if (source == nullptr)
        return;

    const bool known = this->frozen.contains(source);
    if (isFrozenNow)
    {
        const QPair<QString, int> where = qMakePair(deviceName, channel);
        if (known && this->frozen.value(source) == where)
            return;
        if (!known)
        {
            // A row deleted while frozen must not stay frozen here.
            QObject::connect(source, &QObject::destroyed, this, [this, source]() {
                if (this->frozen.remove(source) > 0)
                    emit changed();
            });
        }
        this->frozen.insert(source, where);
        emit changed();
        return;
    }

    if (known)
    {
        this->frozen.remove(source);
        QObject::disconnect(source, &QObject::destroyed, this, nullptr);
        emit changed();
    }
}

bool FrozenOutputs::isFrozen(const QString& deviceName, int channel) const
{
    for (const QPair<QString, int>& where : this->frozen)
    {
        if (where.first == deviceName && where.second == channel)
            return true;
    }
    return false;
}

QList<QPair<QString, int>> FrozenOutputs::frozenChannels() const
{
    QList<QPair<QString, int>> channels;
    for (const QPair<QString, int>& where : this->frozen)
    {
        if (!channels.contains(where))
            channels << where;
    }
    return channels;
}

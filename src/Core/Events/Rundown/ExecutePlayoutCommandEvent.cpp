#include "ExecutePlayoutCommandEvent.h"

ExecutePlayoutCommandEvent::ExecutePlayoutCommandEvent(QEvent::Type type, int key, Qt::KeyboardModifiers modifiers)
    : type(type), key(key), modifiers(modifiers),
      playoutType(Playout::PlayoutType::Stop), hasPlayoutType(false)
{
}

ExecutePlayoutCommandEvent::ExecutePlayoutCommandEvent(Playout::PlayoutType playoutType)
    : type(QEvent::KeyPress), key(0), modifiers(Qt::NoModifier),
      playoutType(playoutType), hasPlayoutType(true)
{
}

QEvent::Type ExecutePlayoutCommandEvent::getType() const
{
    return this->type;
}

int ExecutePlayoutCommandEvent::getKey() const
{
    return this->key;
}

Qt::KeyboardModifiers ExecutePlayoutCommandEvent::getModifiers() const
{
    return this->modifiers;
}

Playout::PlayoutType ExecutePlayoutCommandEvent::getPlayoutType() const
{
    return this->playoutType;
}

bool ExecutePlayoutCommandEvent::getHasPlayoutType() const
{
    return this->hasPlayoutType;
}

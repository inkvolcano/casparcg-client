#pragma once

#include "../../Shared.h"

#include "Global.h"
#include "Playout.h"

#include <QtGui/QKeyEvent>

class CORE_EXPORT ExecutePlayoutCommandEvent
{
    public:
        explicit ExecutePlayoutCommandEvent(QEvent::Type type, int key, Qt::KeyboardModifiers modifiers);
        explicit ExecutePlayoutCommandEvent(Playout::PlayoutType playoutType);

        QEvent::Type getType() const;
        int getKey() const;
        Qt::KeyboardModifiers getModifiers() const;
        Playout::PlayoutType getPlayoutType() const;
        bool getHasPlayoutType() const;

    private:
        QEvent::Type type;
        int key;
        Qt::KeyboardModifiers modifiers;
        Playout::PlayoutType playoutType;
        bool hasPlayoutType;
};

#pragma once

#include "Shared.h"

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QPair>
#include <QtCore/QString>

// Which channels have a frozen clip on them right now (build 302), for the
// Server Status panel to flash. Each playing clip row says when it freezes and
// when it moves again (Common/FrozenRules.h decides); a row that goes away
// takes its report with it.
class WIDGETS_EXPORT FrozenOutputs : public QObject
{
    Q_OBJECT

    public:
        static FrozenOutputs& getInstance();

        void report(QObject* source, const QString& deviceName, int channel, bool frozen);
        bool isFrozen(const QString& deviceName, int channel) const;
        QList<QPair<QString, int>> frozenChannels() const;

        Q_SIGNAL void changed();

    private:
        explicit FrozenOutputs() {}

        QHash<QObject*, QPair<QString, int>> frozen;
};

#pragma once

#include "Shared.h"

#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QString>

class AbstractCommand;

class CORE_EXPORT CloneGroupRegistry : public QObject
{
    Q_OBJECT

    public:
        explicit CloneGroupRegistry();

        static CloneGroupRegistry& getInstance();

        void registerCommand(const QString& groupId, AbstractCommand* command);
        void unregisterCommand(AbstractCommand* command);
        void syncFromSource(const QString& groupId, AbstractCommand* source);

        bool isSyncing() const;

    private:
        QMap<QString, QSet<AbstractCommand*>> groups;
        bool syncing = false;
};

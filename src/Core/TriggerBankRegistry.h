#pragma once

#include "Shared.h"

#include "Global.h"

#include <QtCore/QMap>
#include <QtCore/QObject>

#include <QtWidgets/QTreeWidgetItem>

class CORE_EXPORT TriggerBankRegistry : public QObject
{
    Q_OBJECT

    public:
        explicit TriggerBankRegistry();

        static TriggerBankRegistry& getInstance();

        void assign(int bankId, QTreeWidgetItem* item);
        void unassign(int bankId);
        void unassignByItem(QTreeWidgetItem* item);
        QTreeWidgetItem* getItem(int bankId) const;
        int getBankForItem(QTreeWidgetItem* item) const;
        void clear();

        void fireBankTriggered(int bankId);

    Q_SIGNALS:
        void bankTriggered(int bankId);

    private:
        QMap<int, QTreeWidgetItem*> assignments;
};

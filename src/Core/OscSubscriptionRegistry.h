#pragma once

#include "Shared.h"

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtCore/QVector>

class OscSubscription;

class OscSubscriptionRegistry
{
    public:
        static OscSubscriptionRegistry& getInstance();

        void subscribe(const QString& path, OscSubscription* subscription);
        void unsubscribe(OscSubscription* subscription);

        void dispatch(const QString& eventPath, const QList<QVariant>& arguments);

        OscSubscriptionRegistry();

    private:

        QHash<QString, QVector<OscSubscription*>> subscriptions;
};

#pragma once

#include "Shared.h"

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariant>

class CORE_EXPORT OscSubscription : public QObject
{
    Q_OBJECT

    public:
        explicit OscSubscription(const QString& path, QObject* parent = 0);
        ~OscSubscription();

        const QString& getPath() const;

        void notifySubscriber(const QList<QVariant>& arguments);

        Q_SIGNAL void subscriptionReceived(const QString&, const QList<QVariant>&);

    private:
        QString path;
};

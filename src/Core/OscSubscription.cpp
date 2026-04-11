#include "OscSubscription.h"
#include "OscSubscriptionRegistry.h"

OscSubscription::OscSubscription(const QString& path, QObject* parent)
    : QObject(parent),
      path(path)
{
    OscSubscriptionRegistry::getInstance().subscribe(this->path, this);
}

OscSubscription::~OscSubscription()
{
    OscSubscriptionRegistry::getInstance().unsubscribe(this);
}

const QString& OscSubscription::getPath() const
{
    return this->path;
}

void OscSubscription::notifySubscriber(const QList<QVariant>& arguments)
{
    emit subscriptionReceived(this->path, arguments);
}

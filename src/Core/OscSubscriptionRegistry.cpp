#include "OscSubscriptionRegistry.h"
#include "OscSubscription.h"

Q_GLOBAL_STATIC(OscSubscriptionRegistry, oscSubscriptionRegistry)

OscSubscriptionRegistry::OscSubscriptionRegistry()
{
}

OscSubscriptionRegistry& OscSubscriptionRegistry::getInstance()
{
    return *oscSubscriptionRegistry();
}

void OscSubscriptionRegistry::subscribe(const QString& path, OscSubscription* subscription)
{
    this->subscriptions[path].append(subscription);
}

void OscSubscriptionRegistry::unsubscribe(OscSubscription* subscription)
{
    const QString& path = subscription->getPath();
    auto it = this->subscriptions.find(path);
    if (it != this->subscriptions.end())
    {
        it->removeOne(subscription);
        if (it->isEmpty())
            this->subscriptions.erase(it);
    }
}

void OscSubscriptionRegistry::dispatch(const QString& eventPath, const QList<QVariant>& arguments)
{
    // 1. Exact match — covers monitor subscriptions that include IP prefix
    //    e.g. eventPath = "192.168.1.5/channel/1/stage/layer/10/foreground/file/time"
    //         subscription path = "192.168.1.5/channel/1/stage/layer/10/foreground/file/time"
    auto it = this->subscriptions.find(eventPath);
    if (it != this->subscriptions.end())
    {
        for (OscSubscription* sub : *it)
            sub->notifySubscriber(arguments);
    }

    // 2. Suffix match — covers control/websocket subscriptions without IP prefix
    //    e.g. eventPath = "192.168.1.5/control/abc123/stop"
    //         subscription path = "/control/abc123/stop"
    int slashPos = eventPath.indexOf('/');
    if (slashPos > 0)
    {
        QString suffix = eventPath.mid(slashPos);
        auto it2 = this->subscriptions.find(suffix);
        if (it2 != this->subscriptions.end())
        {
            for (OscSubscription* sub : *it2)
                sub->notifySubscriber(arguments);
        }
    }
}

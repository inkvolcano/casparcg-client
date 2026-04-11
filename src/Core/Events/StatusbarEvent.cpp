#include "StatusbarEvent.h"

#include "Global.h"

StatusbarEvent::StatusbarEvent(const QString& message, int timeout, bool isError)
    : timeout(timeout), isError(isError), message(message)
{
}

const QString& StatusbarEvent::getMessage() const
{
    return this->message;
}

int StatusbarEvent::getTimeout() const
{
    return this->timeout;
}

bool StatusbarEvent::getIsError() const
{
    return this->isError;
}

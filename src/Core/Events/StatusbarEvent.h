#pragma once

#include "../Shared.h"

#include <QtCore/QString>

class CORE_EXPORT StatusbarEvent
{
    public:
        explicit StatusbarEvent(const QString& message, int timeout = 3000, bool isError = false);

        int getTimeout() const;
        const QString& getMessage() const;
        bool getIsError() const;

    private:
        int timeout;
        bool isError;
        QString message;
};

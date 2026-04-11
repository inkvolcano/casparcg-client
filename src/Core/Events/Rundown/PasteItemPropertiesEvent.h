#pragma once

#include "../../Shared.h"

#include <QtCore/QString>

class CORE_EXPORT PasteItemPropertiesEvent
{
    public:
        explicit PasteItemPropertiesEvent(bool noData = false);

        bool getNoData() const;

    private:
        bool noData = false;
};

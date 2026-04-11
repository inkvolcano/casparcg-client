#include "PasteItemPropertiesEvent.h"

#include "Global.h"

PasteItemPropertiesEvent::PasteItemPropertiesEvent(bool noData)
    : noData(noData)
{
}

bool PasteItemPropertiesEvent::getNoData() const
{
    return this->noData;
}

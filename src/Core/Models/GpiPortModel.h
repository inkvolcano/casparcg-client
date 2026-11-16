#pragma once

#include "../Shared.h"
#include "Global.h"
#include "Playout.h"

#include <QtCore/QString>

class CORE_EXPORT GpiPortModel
{
    public:
        explicit GpiPortModel(int port, bool risingEdge, Playout::PlayoutType action);

        int getPort() const;
        bool isRisingEdge() const;
        Playout::PlayoutType getAction() const;

    private:
        int port = 0;
        bool risingEdge = false;
        Playout::PlayoutType action;
};

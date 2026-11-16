#pragma once

#include "../Shared.h"

#include <QtCore/QString>

class CORE_EXPORT FormatModel
{
    public:
        explicit FormatModel(int id, const QString& name, int width, int height, const QString& fps);

        int getId() const;
        const QString& getName() const;
        int getWidth() const;
        int getHeight() const;
        const QString& getFramesPerSecond() const;

    private:
        int id = 0;
        QString name;
        int width = 0;
        int height = 0;
        QString fps;
};



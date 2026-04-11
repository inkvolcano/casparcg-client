#pragma once

#include "../Shared.h"

#include <QtCore/QString>

class CORE_EXPORT KeyValueModel
{
    public:
        explicit KeyValueModel(const QString& key, const QString& value, int mode = 0, const QString& cycleValues = "");

        const QString& getKey() const;
        const QString& getValue() const;
        int getMode() const;
        const QString& getCycleValues() const;

        void setKey(const QString& key);
        void setValue(const QString& value);
        void setMode(int mode);
        void setCycleValues(const QString& cycleValues);

    private:
        QString key;
        QString value;
        int mode = 0;
        QString cycleValues;
};

#pragma once

#include "../Shared.h"

#include <QtCore/QString>

// A saved panel arrangement. Scope tells the normal layout apart from the one
// Simple Mode uses; Data is the JSON of the configuration keys that scope owns.
// See src/Common/LayoutPreset.h for which keys those are.
class CORE_EXPORT LayoutPresetModel
{
    public:
        explicit LayoutPresetModel(int id, const QString& name, const QString& scope, const QString& data);

        int getId() const;
        const QString& getName() const;
        const QString& getScope() const;
        const QString& getData() const;

    private:
        int id;
        QString name;
        QString scope;
        QString data;
};

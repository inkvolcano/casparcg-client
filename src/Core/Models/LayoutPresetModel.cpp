#include "LayoutPresetModel.h"

LayoutPresetModel::LayoutPresetModel(int id, const QString& name, const QString& scope, const QString& data)
    : id(id), name(name), scope(scope), data(data)
{
}

int LayoutPresetModel::getId() const
{
    return this->id;
}

const QString& LayoutPresetModel::getName() const
{
    return this->name;
}

const QString& LayoutPresetModel::getScope() const
{
    return this->scope;
}

const QString& LayoutPresetModel::getData() const
{
    return this->data;
}

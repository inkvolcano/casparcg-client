#include "KeyValueModel.h"

KeyValueModel::KeyValueModel(const QString& key, const QString& value, int mode, const QString& cycleValues)
{
    this->key = key;
    this->value = value;
    this->mode = mode;
    this->cycleValues = cycleValues;
}

const QString& KeyValueModel::getKey() const
{
    return this->key;
}

const QString& KeyValueModel::getValue() const
{
    return this->value;
}

int KeyValueModel::getMode() const
{
    return this->mode;
}

void KeyValueModel::setKey(const QString& key)
{
    this->key = key;
}

void KeyValueModel::setValue(const QString& value)
{
    this->value = value;
}

void KeyValueModel::setMode(int mode)
{
    this->mode = mode;
}

const QString& KeyValueModel::getCycleValues() const
{
    return this->cycleValues;
}

void KeyValueModel::setCycleValues(const QString& cycleValues)
{
    this->cycleValues = cycleValues;
}

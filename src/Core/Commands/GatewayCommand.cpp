#include "GatewayCommand.h"

#include <QtCore/QTime>
#include <QtCore/QXmlStreamWriter>

GatewayCommand::GatewayCommand(QObject* parent)
    : AbstractCommand(parent)
{
}

const QString& GatewayCommand::getGatewayId() const
{
    return this->gatewayId;
}

bool GatewayCommand::getIsExit() const
{
    return this->isExit;
}

void GatewayCommand::setGatewayId(const QString& gatewayId)
{
    this->gatewayId = gatewayId;
    emit gatewayIdChanged(this->gatewayId);
}

void GatewayCommand::setIsExit(bool isExit)
{
    this->isExit = isExit;
    emit isExitChanged(this->isExit);
}

const QString& GatewayCommand::getExitLabel() const
{
    return this->exitLabel;
}

const QString& GatewayCommand::getSelectedExitLabel() const
{
    return this->selectedExitLabel;
}

void GatewayCommand::setExitLabel(const QString& exitLabel)
{
    this->exitLabel = exitLabel;
    emit exitLabelChanged(this->exitLabel);
}

void GatewayCommand::setSelectedExitLabel(const QString& selectedExitLabel)
{
    this->selectedExitLabel = selectedExitLabel;
    emit selectedExitLabelChanged(this->selectedExitLabel);
}

bool GatewayCommand::getConditionEnabled() const
{
    return this->conditionEnabled;
}

int GatewayCommand::getConditionOperator() const
{
    return this->conditionOperator;
}

int GatewayCommand::getConditionHour() const
{
    return this->conditionHour;
}

int GatewayCommand::getConditionMinute() const
{
    return this->conditionMinute;
}

int GatewayCommand::getConditionSecond() const
{
    return this->conditionSecond;
}

const QString& GatewayCommand::getConditionExitLabel() const
{
    return this->conditionExitLabel;
}

void GatewayCommand::setConditionEnabled(bool enabled)
{
    this->conditionEnabled = enabled;
    emit conditionEnabledChanged(this->conditionEnabled);
}

void GatewayCommand::setConditionOperator(int op)
{
    this->conditionOperator = op;
    emit conditionOperatorChanged(this->conditionOperator);
}

void GatewayCommand::setConditionHour(int hour)
{
    this->conditionHour = qBound(0, hour, 23);
    emit conditionHourChanged(this->conditionHour);
}

void GatewayCommand::setConditionMinute(int minute)
{
    this->conditionMinute = qBound(0, minute, 59);
    emit conditionMinuteChanged(this->conditionMinute);
}

void GatewayCommand::setConditionSecond(int second)
{
    this->conditionSecond = qBound(0, second, 59);
    emit conditionSecondChanged(this->conditionSecond);
}

void GatewayCommand::setConditionExitLabel(const QString& label)
{
    this->conditionExitLabel = label;
    emit conditionExitLabelChanged(this->conditionExitLabel);
}

QString GatewayCommand::getEffectiveExitLabel() const
{
    if (!this->conditionEnabled || this->conditionExitLabel.isEmpty())
        return this->selectedExitLabel;

    QTime now = QTime::currentTime();
    QTime condTime(this->conditionHour, this->conditionMinute, this->conditionSecond);
    if (!condTime.isValid())
        return this->selectedExitLabel;
    bool result = false;
    switch (this->conditionOperator)
    {
        case 0: result = now > condTime; break;
        case 1: result = now < condTime; break;
        case 2: result = now >= condTime; break;
        case 3: result = now <= condTime; break;
        case 4: result = (now.hour() == condTime.hour() && now.minute() == condTime.minute() && now.second() == condTime.second()); break;
        default: break;
    }
    return result ? this->conditionExitLabel : this->selectedExitLabel;
}

void GatewayCommand::readProperties(boost::property_tree::wptree& pt)
{
    AbstractCommand::readProperties(pt);

    if (pt.count(L"gatewayid") > 0) setGatewayId(QString::fromStdWString(pt.get<std::wstring>(L"gatewayid")));
    if (pt.count(L"isexit") > 0) setIsExit(pt.get<bool>(L"isexit"));
    if (pt.count(L"exitlabel") > 0) setExitLabel(QString::fromStdWString(pt.get<std::wstring>(L"exitlabel")));
    if (pt.count(L"selectedexitlabel") > 0) setSelectedExitLabel(QString::fromStdWString(pt.get<std::wstring>(L"selectedexitlabel")));
    if (pt.count(L"conditionenabled") > 0) setConditionEnabled(pt.get<bool>(L"conditionenabled"));
    if (pt.count(L"conditionoperator") > 0) setConditionOperator(pt.get<int>(L"conditionoperator"));
    if (pt.count(L"conditionhour") > 0) setConditionHour(pt.get<int>(L"conditionhour"));
    if (pt.count(L"conditionminute") > 0) setConditionMinute(pt.get<int>(L"conditionminute"));
    if (pt.count(L"conditionsecond") > 0) setConditionSecond(pt.get<int>(L"conditionsecond"));
    if (pt.count(L"conditionexitlabel") > 0) setConditionExitLabel(QString::fromStdWString(pt.get<std::wstring>(L"conditionexitlabel")));
}

void GatewayCommand::writeProperties(QXmlStreamWriter& writer)
{
    AbstractCommand::writeProperties(writer);

    writer.writeTextElement("gatewayid", this->gatewayId);
    writer.writeTextElement("isexit", (this->isExit == true) ? "true" : "false");
    writer.writeTextElement("exitlabel", this->exitLabel);
    writer.writeTextElement("selectedexitlabel", this->selectedExitLabel);
    writer.writeTextElement("conditionenabled", this->conditionEnabled ? "true" : "false");
    writer.writeTextElement("conditionoperator", QString::number(this->conditionOperator));
    writer.writeTextElement("conditionhour", QString::number(this->conditionHour));
    writer.writeTextElement("conditionminute", QString::number(this->conditionMinute));
    writer.writeTextElement("conditionsecond", QString::number(this->conditionSecond));
    writer.writeTextElement("conditionexitlabel", this->conditionExitLabel);
}

#pragma once

#include "../Shared.h"
#include "AbstractCommand.h"

#include "Global.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <QtCore/QString>

class QObject;

class CORE_EXPORT GatewayCommand : public AbstractCommand
{
    Q_OBJECT

    public:
        explicit GatewayCommand(QObject* parent = 0);

        const QString& getGatewayId() const;
        bool getIsExit() const;
        const QString& getExitLabel() const;
        const QString& getSelectedExitLabel() const;

        bool getConditionEnabled() const;
        int getConditionOperator() const;
        int getConditionHour() const;
        int getConditionMinute() const;
        int getConditionSecond() const;
        const QString& getConditionExitLabel() const;
        QString getEffectiveExitLabel() const;

        void setGatewayId(const QString& gatewayId);
        void setIsExit(bool isExit);
        void setExitLabel(const QString& exitLabel);
        void setSelectedExitLabel(const QString& selectedExitLabel);

        void setConditionEnabled(bool enabled);
        void setConditionOperator(int op);
        void setConditionHour(int hour);
        void setConditionMinute(int minute);
        void setConditionSecond(int second);
        void setConditionExitLabel(const QString& label);

        virtual void readProperties(boost::property_tree::wptree& pt);
        virtual void writeProperties(QXmlStreamWriter& writer);

    private:
        QString gatewayId;
        bool isExit = false;
        QString exitLabel = "Exit A";
        QString selectedExitLabel = "Exit A";

        bool conditionEnabled = false;
        int conditionOperator = 0;
        int conditionHour = 0;
        int conditionMinute = 0;
        int conditionSecond = 0;
        QString conditionExitLabel;

    Q_SIGNALS:
        void gatewayIdChanged(const QString&);
        void isExitChanged(bool);
        void exitLabelChanged(const QString&);
        void selectedExitLabelChanged(const QString&);
        void conditionEnabledChanged(bool);
        void conditionOperatorChanged(int);
        void conditionHourChanged(int);
        void conditionMinuteChanged(int);
        void conditionSecondChanged(int);
        void conditionExitLabelChanged(const QString&);
};

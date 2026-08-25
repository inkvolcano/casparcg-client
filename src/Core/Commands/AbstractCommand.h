#pragma once

#include "../Shared.h"
#include "AbstractProperties.h"

#include "Global.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <QtCore/QObject>

class QXmlStreamWriter;

class CORE_EXPORT AbstractCommand : public QObject, public AbstractProperties
{
    Q_OBJECT

    public:
        virtual ~AbstractCommand();

        virtual int getDelay() const;
        virtual int getDuration() const;
        virtual int getChannel() const;
        virtual int getVideolayer() const;
        virtual bool getAllowGpi() const;
        virtual bool getAllowRemoteTriggering() const;
        virtual QString getRemoteTriggerId() const;
        virtual QString getStoryId() const;
        virtual int getTriggerBank() const;
        virtual QString getCloneGroupId() const;
        virtual bool getDisabled() const;

        virtual void setChannel(int channel);
        virtual void setVideolayer(int videolayer);
        virtual void setDelay(int delay);
        virtual void setDuration(int duration);
        virtual void setAllowGpi(bool allowGpi);
        virtual void setAllowRemoteTriggering(bool allowRemoteTriggering);
        virtual void setRemoteTriggerId(const QString& remoteTriggerId);
        virtual void setStoryId(const QString& storyId);
        virtual void setTriggerBank(int triggerBank);
        virtual void setCloneGroupId(const QString& cloneGroupId);
        virtual void setDisabled(bool disabled);

        // Temporary channel override for preview mode. Does not emit signals.
        // Use getBaseChannel() when you need the configured channel (e.g. inspector display).
        void setChannelOverride(int channel);
        void clearChannelOverride();
        int getBaseChannel() const;

        virtual void readProperties(boost::property_tree::wptree& pt);
        virtual void writeProperties(QXmlStreamWriter& writer);

    protected:
        explicit AbstractCommand(QObject* parent = 0);

        QString storyId = "";
        QString remoteTriggerId = Output::DEFAULT_REMOTE_TRIGGER_ID;

        int channel = Output::DEFAULT_CHANNEL;
        int channelOverrideValue = 0;
        int videolayer = Output::DEFAULT_VIDEOLAYER;
        int delay = Output::DEFAULT_DELAY;
        int duration = Output::DEFAULT_DURATION ;
        bool allowGpi = Output::DEFAULT_ALLOW_GPI;
        bool allowRemoteTriggering = Output::DEFAULT_ALLOW_REMOTE_TRIGGERING;
        int triggerBank = 0;
        QString cloneGroupId;
        bool disabled = false;

    signals:
        void channelChanged(int);
        void videolayerChanged(int);
        void delayChanged(int);
        void durationChanged(int);
        void allowGpiChanged(bool);
        void allowRemoteTriggeringChanged(bool);
        void remoteTriggerIdChanged(const QString&);
        void storyIdChanged(const QString&);
        void triggerBankChanged(int);
        void cloneGroupIdChanged(const QString&);
        void disabledChanged(bool);
        void propertyChanged();
};

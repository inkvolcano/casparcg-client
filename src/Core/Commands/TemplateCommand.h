#pragma once

#include "../Shared.h"
#include "AbstractCommand.h"
#include "Models/KeyValueModel.h"
#include "TransformData.h"

#include "Global.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QJsonObject>
#include <QJsonDocument>

class QObject;
class QXmlStreamWriter;

class CORE_EXPORT TemplateCommand : public AbstractCommand
{
    Q_OBJECT

    public:
        explicit TemplateCommand(QObject* parent = 0);


        virtual void readProperties(boost::property_tree::wptree& pt);
        virtual void writeProperties(QXmlStreamWriter& writer);

        int getFlashlayer() const;
        const QString& getInvoke() const;
        const QStringList& getInvokes() const;
        QString getInvokeAt(int index) const;
        int getInvokeCount() const;
        int getInvokeHotkeyIndex() const;
        const QStringList& getInvokeLabels() const;
        QString getInvokeLabelAt(int index) const;
        bool getUseStoredData() const;
        bool getSendAsJson() const;
        bool getUseUppercaseData() const;
        const QString& getTemplateName() const;
        const QString getTemplateData() const;
        const QList<KeyValueModel>& getTemplateDataModels() const;
        bool getTriggerOnNext() const;
        bool getAutoPlay() const;
        bool getAutoLoop() const;
        int getAutoLoopDelay() const;
        int getNewlineBehavior() const;

        void setFlashlayer(int flashlayer);
        void setInvoke(const QString& invoke);
        void setInvokes(const QStringList& invokes);
        void setInvokeHotkeyIndex(int index);
        void setInvokeLabels(const QStringList& labels);
        void setUseStoredData(bool useStoredData);
        void setSendAsJson(bool sendAsJson);
        void setUseUppercaseData(bool useUppercaseData);
        void setTemplateName(const QString& templateName);
        void setTemplateDataModels(const QList<KeyValueModel>& models);
        void setTriggerOnNext(bool triggerOnNext);
        void setAutoPlay(bool autoPlay);
        void setAutoLoop(bool autoLoop);
        void setAutoLoopDelay(int autoLoopDelay);
        void setNewlineBehavior(int newlineBehavior);

        void setPendingInvokeOverride(const QString& override);
        QString takePendingInvokeOverride();

        TransformData& getTransform();
        const TransformData& getTransform() const;

    private:
        int flashlayer = Template::DEFAULT_FLASHLAYER;
        QStringList invokes = { Template::DEFAULT_INVOKE };
        QStringList invokeLabels = { QString() };   // parallel to invokes; empty = no label
        int invokeHotkeyIndex = 0;
        QString pendingInvokeOverride;
        bool useStoredData = Template::DEFAULT_USE_STORED_DATA;
        bool useUppercaseData = Template::DEFAULT_USE_UPPERCASE_DATA;
        QString templateName = Template::DEFAULT_TEMPLATENAME;
        // The tab, the key column and the column mapping all come from the template
        // itself, so the item only has to remember which row it represents.
        QList<KeyValueModel> models;
        bool triggerOnNext = Template::DEFAULT_TRIGGER_ON_NEXT;
        bool autoPlay = false;
        bool sendAsJson = Template::DEFAULT_SEND_AS_JSON;
        int newlineBehavior = Template::DEFAULT_NEWLINE_BEHAVIOR;
        bool autoLoop = Template::DEFAULT_AUTO_LOOP;
        int autoLoopDelay = Template::DEFAULT_AUTO_LOOP_DELAY;
        TransformData m_transform;

        Q_SIGNAL void flashlayerChanged(int);
        Q_SIGNAL void invokesChanged(const QStringList&);
        Q_SIGNAL void invokeLabelsChanged(const QStringList&);
        Q_SIGNAL void useStoredDataChanged(bool);
        Q_SIGNAL void sendAsJsonChanged(bool);
        Q_SIGNAL void useUppercaseDataChanged(bool);
        Q_SIGNAL void templateNameChanged(const QString&);
        Q_SIGNAL void templateDataChanged(const QList<KeyValueModel>&);
        Q_SIGNAL void triggerOnNextChanged(bool);
        Q_SIGNAL void autoPlayChanged(bool);
        Q_SIGNAL void autoLoopChanged(bool);
        Q_SIGNAL void autoLoopDelayChanged(int);
        Q_SIGNAL void newlineBehaviorChanged(int);
};

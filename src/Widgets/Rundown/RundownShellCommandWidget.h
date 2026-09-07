#pragma once

#include "../Shared.h"
#include "AbstractRundownWidget.h"
#include "ui_RundownShellCommandWidget.h"

#include "Global.h"

#include "GpiDevice.h"

#include "OscSubscription.h"
#include "Animations/ActiveAnimation.h"
#include "Commands/AbstractCommand.h"
#include "Commands/AbstractPlayoutCommand.h"
#include "Commands/ShellCommand.h"
#include "Events/Inspector/LabelChangedEvent.h"
#include "Models/LibraryModel.h"
#include "Utils/ItemScheduler.h"

#include <QtCore/QProcess>
#include <QtCore/QString>

#include <QtWidgets/QWidget>

class WIDGETS_EXPORT RundownShellCommandWidget : public QWidget, Ui::RundownShellCommandWidget, public AbstractRundownWidget, public AbstractPlayoutCommand
{
    Q_OBJECT

    public:
        explicit RundownShellCommandWidget(const LibraryModel& model, QWidget* parent = 0, const QString& color = Color::DEFAULT_TRANSPARENT_COLOR,
                                           bool active = false, bool inGroup = false, bool compactView = false);

        virtual AbstractRundownWidget* clone();

        virtual bool isGroup() const;
        virtual bool isInGroup() const;
        virtual bool executeCommand(Playout::PlayoutType type);

        virtual AbstractCommand* getCommand();
        virtual LibraryModel* getLibraryModel();

        virtual QString getColor() const;

        virtual void setExpanded(bool /* expanded */) {}
        virtual void setActive(bool active);
        virtual void setInGroup(bool inGroup);
        virtual void setColor(const QString& color);
        virtual void readProperties(boost::property_tree::wptree& pt);
        virtual void writeProperties(QXmlStreamWriter& writer);
        virtual void setCompactView(bool compactView);
        virtual void clearDelayedCommands();
        virtual void setUsed(bool used);
        virtual void setSelected(bool selected);
        virtual void setRundownDisabled(bool disabled);

        // Shell command items are refused unless the client has been told to allow
        // them, so a rundown from elsewhere cannot run a program by being opened.
        static bool shellCommandsAllowed();

    private:
        bool active;
        bool inGroup;
        bool compactView;
        QString color;
        LibraryModel model;
        ShellCommand command;
        ActiveAnimation* animation;
        bool markUsedItems;
        bool selected = false;

        OscSubscription* stopControlSubscription;
        OscSubscription* playControlSubscription;
        OscSubscription* playNowControlSubscription;
        OscSubscription* updateControlSubscription;
        OscSubscription* clearControlSubscription;
        OscSubscription* clearVideolayerControlSubscription;
        OscSubscription* clearChannelControlSubscription;

        ItemScheduler itemScheduler;

        // The process this item last started, kept only so a still-running one can
        // be killed by Stop. Cleared when it finishes.
        QProcess* process = nullptr;

        void checkGpiConnection();
        void configureOscSubscriptions();
        void updateCommandLabel();

        Q_SLOT void executeStop();
        Q_SLOT void executePlay();
        Q_SLOT void delayChanged(int);
        Q_SLOT void commandLineChanged(const QString&);
        Q_SLOT void allowGpiChanged(bool);
        Q_SLOT void remoteTriggerIdChanged(const QString&);
        Q_SLOT void gpiConnectionStateChanged(bool, GpiDevice*);
        Q_SLOT void stopControlSubscriptionReceived(const QString&, const QList<QVariant>&);
        Q_SLOT void playControlSubscriptionReceived(const QString&, const QList<QVariant>&);
        Q_SLOT void playNowControlSubscriptionReceived(const QString&, const QList<QVariant>&);
        Q_SLOT void updateControlSubscriptionReceived(const QString&, const QList<QVariant>&);
        Q_SLOT void clearControlSubscriptionReceived(const QString&, const QList<QVariant>&);
        Q_SLOT void clearVideolayerControlSubscriptionReceived(const QString&, const QList<QVariant>&);
        Q_SLOT void clearChannelControlSubscriptionReceived(const QString&, const QList<QVariant>&);
        Q_SLOT void labelChanged(const LabelChangedEvent&);
};

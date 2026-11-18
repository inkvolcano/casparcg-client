#pragma once

#include "../Shared.h"
#include "AbstractCommand.h"

#include "Global.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <QtCore/QString>

class QObject;
class QXmlStreamWriter;

// Plays an NDI source on a layer, the way DeckLink Input plays a card's input:
// PLAY 1-10 [NDI] "MACHINE (Source)". The AMCP is built by src/Common/NdiInputRules.h.
class CORE_EXPORT NdiInputCommand : public AbstractCommand
{
    Q_OBJECT

    public:
        explicit NdiInputCommand(QObject* parent = 0);

        virtual void readProperties(boost::property_tree::wptree& pt);
        virtual void writeProperties(QXmlStreamWriter& writer);

        const QString& getSource() const;
        bool getLowBandwidth() const;
        const QString& getTransition() const;
        int getTransitionDuration() const;
        const QString& getTween() const;
        const QString& getDirection() const;

        void setSource(const QString& source);
        void setLowBandwidth(bool lowBandwidth);
        void setTransition(const QString& transition);
        void setTransitionDuration(int transitionDuration);
        void setTween(const QString& tween);
        void setDirection(const QString& direction);

    private:
        QString source = "";
        bool lowBandwidth = false;
        QString transition = Mixer::DEFAULT_TRANSITION;
        int transitionDuration = Mixer::DEFAULT_DURATION;
        QString tween = Mixer::DEFAULT_TWEEN;
        QString direction = Mixer::DEFAULT_DIRECTION;

        Q_SIGNAL void sourceChanged(const QString&);
        Q_SIGNAL void lowBandwidthChanged(bool);
        Q_SIGNAL void transitionChanged(const QString&);
        Q_SIGNAL void transitionDurationChanged(int);
        Q_SIGNAL void directionChanged(const QString&);
        Q_SIGNAL void tweenChanged(const QString&);
};

#pragma once

#include "../Shared.h"
#include "AbstractCommand.h"
#include "TransformData.h"

#include "Global.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <QtCore/QString>

class QObject;
class QXmlStreamWriter;

class CORE_EXPORT StillCommand : public AbstractCommand
{
    Q_OBJECT

    public:
        explicit StillCommand(QObject* parent = 0);

        virtual void readProperties(boost::property_tree::wptree& pt);
        virtual void writeProperties(QXmlStreamWriter& writer);

        const QString& getImageName() const;
        const QString& getTransition() const;
        int getTransitionDuration() const;
        const QString& getTween() const;
        const QString& getDirection() const;
        bool getTriggerOnNext() const;
        bool getUseAuto() const;
        bool getAutoPlay() const;

        void setImageName(const QString& imageName);
        void setTransition(const QString& transition);
        void setTransitionDuration(int transitionDuration);
        void setTween(const QString& tween);
        void setDirection(const QString& direction);
        void setTriggerOnNext(bool triggerOnNext);
        void setUseAuto(bool useAuto);
        void setAutoPlay(bool autoPlay);

        TransformData& getTransform();
        const TransformData& getTransform() const;

    private:
        QString imageName = Still::DEFAULT_NAME;
        QString transition = Mixer::DEFAULT_TRANSITION;
        int transitionDuration = Mixer::DEFAULT_DURATION;
        QString tween = Mixer::DEFAULT_TWEEN;
        QString direction = Mixer::DEFAULT_DIRECTION;
        bool triggerOnNext = Still::DEFAULT_TRIGGER_ON_NEXT;
        bool useAuto = Still::DEFAULT_USE_AUTO;
        bool autoPlay = false;
        TransformData m_transform;

        Q_SIGNAL void imageNameChanged(const QString&);
        Q_SIGNAL void transitionChanged(const QString&);
        Q_SIGNAL void transitionDurationChanged(int);
        Q_SIGNAL void directionChanged(const QString&);
        Q_SIGNAL void tweenChanged(const QString&);
        Q_SIGNAL void triggerOnNextChanged(bool);
        Q_SIGNAL void useAutoChanged(bool);
        Q_SIGNAL void autoPlayChanged(bool);
};

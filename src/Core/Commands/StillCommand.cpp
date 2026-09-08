#include "StillCommand.h"

#include <QtCore/QXmlStreamWriter>

StillCommand::StillCommand(QObject* parent)
    : AbstractCommand(parent)
{
}

TransformData& StillCommand::getTransform() { return this->m_transform; }
const TransformData& StillCommand::getTransform() const { return this->m_transform; }

const QString& StillCommand::getImageName() const
{
    return this->imageName;
}

const QString& StillCommand::getTransition() const
{
    return this->transition;
}

int StillCommand::getTransitionDuration() const
{
    return this->transitionDuration;
}

const QString& StillCommand::getDirection() const
{
    return this->direction;
}

const QString& StillCommand::getTween() const
{
    return this->tween;
}

bool StillCommand::getTriggerOnNext() const
{
    return this->triggerOnNext;
}

bool StillCommand::getUseAuto() const
{
    return this->useAuto;
}

bool StillCommand::getAutoPlay() const
{
    return this->autoPlay;
}

bool StillCommand::getAutoLoop() const
{
    return this->autoLoop;
}

int StillCommand::getAutoLoopDelay() const
{
    return this->autoLoopDelay;
}

void StillCommand::setImageName(const QString& imageName)
{
    this->imageName = imageName;
    emit imageNameChanged(this->imageName);
    emit propertyChanged();
}

void StillCommand::setTransition(const QString& transition)
{
    this->transition = transition;
    emit transitionChanged(this->transition);
    emit propertyChanged();
}

void StillCommand::setTransitionDuration(int transitionDuration)
{
    this->transitionDuration = transitionDuration;
    emit transitionDurationChanged(this->transitionDuration);
    emit propertyChanged();
}

void StillCommand::setDirection(const QString& direction)
{
    this->direction = direction;
    emit directionChanged(this->direction);
    emit propertyChanged();
}

void StillCommand::setTween(const QString& tween)
{
    this->tween = tween;
    emit tweenChanged(this->tween);
    emit propertyChanged();
}

void StillCommand::setTriggerOnNext(bool triggerOnNext)
{
    this->triggerOnNext = triggerOnNext;
    emit triggerOnNextChanged(this->triggerOnNext);
    emit propertyChanged();
}

void StillCommand::setUseAuto(bool useAuto)
{
    this->useAuto = useAuto;
    emit useAutoChanged(this->useAuto);
    emit propertyChanged();
}

void StillCommand::setAutoPlay(bool autoPlay)
{
    this->autoPlay = autoPlay;
    emit autoPlayChanged(this->autoPlay);
    emit propertyChanged();
}

void StillCommand::setAutoLoop(bool autoLoop)
{
    this->autoLoop = autoLoop;
    emit autoLoopChanged(this->autoLoop);
    emit propertyChanged();
}

void StillCommand::setAutoLoopDelay(int autoLoopDelay)
{
    this->autoLoopDelay = autoLoopDelay;
    emit autoLoopDelayChanged(this->autoLoopDelay);
    emit propertyChanged();
}

void StillCommand::readProperties(boost::property_tree::wptree& pt)
{
    AbstractCommand::readProperties(pt);

    setTransition(QString::fromStdWString(pt.get(L"transition", Mixer::DEFAULT_TRANSITION.toStdWString())));
    setTransitionDuration(pt.get(L"transitionDuration", Mixer::DEFAULT_DURATION));
    setTween(QString::fromStdWString(pt.get(L"tween", Mixer::DEFAULT_TWEEN.toStdWString())));
    setDirection(QString::fromStdWString(pt.get(L"direction", Mixer::DEFAULT_DIRECTION.toStdWString())));
    setUseAuto(pt.get(L"useauto", Still::DEFAULT_USE_AUTO));
    setTriggerOnNext(pt.get(L"triggeronnext", Still::DEFAULT_TRIGGER_ON_NEXT));
    setAutoPlay(pt.get(L"autoplay", Still::DEFAULT_AUTO_PLAY));
    setAutoLoopDelay(pt.get(L"autoloopdelay", Still::DEFAULT_AUTO_LOOP_DELAY));
    setAutoLoop(pt.get(L"autoloop", Still::DEFAULT_AUTO_LOOP));

    if (pt.count(L"transform") > 0)
        m_transform.readProperties(pt.get_child(L"transform"));
}

void StillCommand::writeProperties(QXmlStreamWriter& writer)
{
    AbstractCommand::writeProperties(writer);

    writer.writeTextElement("transition", this->getTransition());
    writer.writeTextElement("transitionDuration", QString::number(this->getTransitionDuration()));
    writer.writeTextElement("tween", this->getTween());
    writer.writeTextElement("direction", this->getDirection());
    writer.writeTextElement("useauto", (getUseAuto() == true) ? "true" : "false");
    writer.writeTextElement("triggeronnext", (getTriggerOnNext() == true) ? "true" : "false");
    writer.writeTextElement("autoplay", (getAutoPlay() == true) ? "true" : "false");
    writer.writeTextElement("autoloop", (getAutoLoop() == true) ? "true" : "false");
    writer.writeTextElement("autoloopdelay", QString::number(getAutoLoopDelay()));

    m_transform.writeProperties(writer);
}

#include "AudioCommand.h"

#include <QtCore/QXmlStreamWriter>

AudioCommand::AudioCommand(QObject* parent)
    : AbstractCommand(parent)
{
    this->videolayer = Output::DEFAULT_AUDIO_VIDEOLAYER;
}

TransformData& AudioCommand::getTransform() { return this->m_transform; }
const TransformData& AudioCommand::getTransform() const { return this->m_transform; }

const QString& AudioCommand::getAudioName() const
{
    return this->audioName;
}

const QString& AudioCommand::getTransition() const
{
    return this->transition;
}

int AudioCommand::getTransitionDuration() const
{
    return this->transitionDuration;
}

const QString& AudioCommand::getDirection() const
{
    return this->direction;
}

const QString& AudioCommand::getTween() const
{
    return this->tween;
}

bool AudioCommand::getLoop() const
{
    return this->loop;
}

bool AudioCommand::getTriggerOnNext() const
{
    return this->triggerOnNext;
}

bool AudioCommand::getUseAuto() const
{
    return this->useAuto;
}

void AudioCommand::setAudioName(const QString& audioName)
{
    this->audioName = audioName;
    emit audioNameChanged(this->audioName);
    emit propertyChanged();
}

void AudioCommand::setTransition(const QString& transition)
{
    this->transition = transition;
    emit transitionChanged(this->transition);
    emit propertyChanged();
}

void AudioCommand::setTransitionDuration(int transitionDuration)
{
    this->transitionDuration = transitionDuration;
    emit transitionDurationChanged(this->transitionDuration);
    emit propertyChanged();
}

void AudioCommand::setDirection(const QString& direction)
{
    this->direction = direction;
    emit directionChanged(this->direction);
    emit propertyChanged();
}

void AudioCommand::setTween(const QString& tween)
{
    this->tween = tween;
    emit tweenChanged(this->tween);
    emit propertyChanged();
}

void AudioCommand::setLoop(bool loop)
{
    this->loop = loop;
    emit loopChanged(this->loop);
    emit propertyChanged();
}

void AudioCommand::setTriggerOnNext(bool triggerOnNext)
{
    this->triggerOnNext = triggerOnNext;
    emit triggerOnNextChanged(this->triggerOnNext);
    emit propertyChanged();
}

void AudioCommand::setUseAuto(bool useAuto)
{
    this->useAuto = useAuto;
    emit useAutoChanged(this->useAuto);
    emit propertyChanged();
}

void AudioCommand::readProperties(boost::property_tree::wptree& pt)
{
    AbstractCommand::readProperties(pt);

    setTransition(QString::fromStdWString(pt.get(L"transition", Mixer::DEFAULT_TRANSITION.toStdWString())));
    setTransitionDuration(pt.get(L"transitionDuration", Mixer::DEFAULT_DURATION));
    setTween(QString::fromStdWString(pt.get(L"tween", Mixer::DEFAULT_TWEEN.toStdWString())));
    setDirection(QString::fromStdWString(pt.get(L"direction", Mixer::DEFAULT_DIRECTION.toStdWString())));
    setLoop(pt.get(L"loop", Audio::DEFAULT_LOOP));
    setUseAuto(pt.get(L"useauto", Audio::DEFAULT_USE_AUTO));
    setTriggerOnNext(pt.get(L"triggeronnext", Audio::DEFAULT_TRIGGER_ON_NEXT));

    if (pt.count(L"transform") > 0)
        m_transform.readProperties(pt.get_child(L"transform"));
}

void AudioCommand::writeProperties(QXmlStreamWriter& writer)
{
    AbstractCommand::writeProperties(writer);

    writer.writeTextElement("transition", getTransition());
    writer.writeTextElement("transitionDuration", QString::number(getTransitionDuration()));
    writer.writeTextElement("tween", getTween());
    writer.writeTextElement("direction", getDirection());
    writer.writeTextElement("loop", (getLoop() == true) ? "true" : "false");
    writer.writeTextElement("useauto", (getUseAuto() == true) ? "true" : "false");
    writer.writeTextElement("triggeronnext", (getTriggerOnNext() == true) ? "true" : "false");

    m_transform.writeProperties(writer);
}

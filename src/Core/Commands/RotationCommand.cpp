#include "RotationCommand.h"

#include <QtCore/QXmlStreamWriter>

RotationCommand::RotationCommand(QObject* parent)
    : AbstractCommand(parent)
{
}

float RotationCommand::getRotation() const
{
    return this->rotation;
}

int RotationCommand::getTransitionDuration() const
{
    return this->transitionDuration;
}

bool RotationCommand::getTriggerOnNext() const
{
    return this->triggerOnNext;
}

const QString& RotationCommand::getTween() const
{
    return this->tween;
}

bool RotationCommand::getDefer() const
{
    return this->defer;
}

void RotationCommand::setRotation(float rotation)
{
    this->rotation = rotation;
    emit rotationChanged(this->rotation);
    emit propertyChanged();
}

void RotationCommand::setTransitionDuration(int transitionDuration)
{
    this->transitionDuration = transitionDuration;
    emit transitionDurationChanged(this->transitionDuration);
    emit propertyChanged();
}

void RotationCommand::setTween(const QString& tween)
{
    this->tween = tween;
    emit tweenChanged(this->tween);
    emit propertyChanged();
}

void RotationCommand::setTriggerOnNext(bool triggerOnNext)
{
    this->triggerOnNext = triggerOnNext;
    emit triggerOnNextChanged(this->triggerOnNext);
    emit propertyChanged();
}

void RotationCommand::setDefer(bool defer)
{
    this->defer = defer;
    emit deferChanged(this->defer);
    emit propertyChanged();
}

void RotationCommand::readProperties(boost::property_tree::wptree& pt)
{
    AbstractCommand::readProperties(pt);

    setRotation(pt.get(L"rotation", Mixer::DEFAULT_ROTATION));
    setTransitionDuration(pt.get(L"transitionDuration", pt.get(L"transtitionDuration", Mixer::DEFAULT_DURATION)));
    setTween(QString::fromStdWString(pt.get(L"tween", Mixer::DEFAULT_TWEEN.toStdWString())));
    setDefer(pt.get(L"defer", Mixer::DEFAULT_DEFER));
}

void RotationCommand::writeProperties(QXmlStreamWriter& writer)
{
    AbstractCommand::writeProperties(writer);

    writer.writeTextElement("rotation", QString::number(getRotation()));
    writer.writeTextElement("transitionDuration", QString::number(getTransitionDuration()));
    writer.writeTextElement("tween", this->getTween());
    writer.writeTextElement("defer", (getDefer() == true) ? "true" : "false");
}

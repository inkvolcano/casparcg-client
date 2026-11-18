#include "NdiInputCommand.h"

#include <QtCore/QXmlStreamWriter>

NdiInputCommand::NdiInputCommand(QObject* parent)
    : AbstractCommand(parent)
{
}

const QString& NdiInputCommand::getSource() const
{
    return this->source;
}

bool NdiInputCommand::getLowBandwidth() const
{
    return this->lowBandwidth;
}

const QString& NdiInputCommand::getTransition() const
{
    return this->transition;
}

int NdiInputCommand::getTransitionDuration() const
{
    return this->transitionDuration;
}

const QString& NdiInputCommand::getDirection() const
{
    return this->direction;
}

const QString& NdiInputCommand::getTween() const
{
    return this->tween;
}

void NdiInputCommand::setSource(const QString& source)
{
    this->source = source;
    emit sourceChanged(this->source);
    emit propertyChanged();
}

void NdiInputCommand::setLowBandwidth(bool lowBandwidth)
{
    this->lowBandwidth = lowBandwidth;
    emit lowBandwidthChanged(this->lowBandwidth);
    emit propertyChanged();
}

void NdiInputCommand::setTransition(const QString& transition)
{
    this->transition = transition;
    emit transitionChanged(this->transition);
    emit propertyChanged();
}

void NdiInputCommand::setTransitionDuration(int transitionDuration)
{
    this->transitionDuration = transitionDuration;
    emit transitionDurationChanged(this->transitionDuration);
    emit propertyChanged();
}

void NdiInputCommand::setDirection(const QString& direction)
{
    this->direction = direction;
    emit directionChanged(this->direction);
    emit propertyChanged();
}

void NdiInputCommand::setTween(const QString& tween)
{
    this->tween = tween;
    emit tweenChanged(this->tween);
    emit propertyChanged();
}

void NdiInputCommand::readProperties(boost::property_tree::wptree& pt)
{
    AbstractCommand::readProperties(pt);

    setSource(QString::fromStdWString(pt.get(L"ndisource", std::wstring())));
    setLowBandwidth(pt.get(L"lowbandwidth", false));
    setTransition(QString::fromStdWString(pt.get(L"transition", Mixer::DEFAULT_TRANSITION.toStdWString())));
    setTransitionDuration(pt.get(L"transitionDuration", Mixer::DEFAULT_DURATION));
    setTween(QString::fromStdWString(pt.get(L"tween", Mixer::DEFAULT_TWEEN.toStdWString())));
    setDirection(QString::fromStdWString(pt.get(L"direction", Mixer::DEFAULT_DIRECTION.toStdWString())));
}

void NdiInputCommand::writeProperties(QXmlStreamWriter& writer)
{
    AbstractCommand::writeProperties(writer);

    writer.writeTextElement("ndisource", getSource());
    writer.writeTextElement("lowbandwidth", getLowBandwidth() ? "true" : "false");
    writer.writeTextElement("transition", getTransition());
    writer.writeTextElement("transitionDuration", QString::number(getTransitionDuration()));
    writer.writeTextElement("tween", getTween());
    writer.writeTextElement("direction", getDirection());
}

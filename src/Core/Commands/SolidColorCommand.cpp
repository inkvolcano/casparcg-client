#include "SolidColorCommand.h"

#include <QtCore/QXmlStreamWriter>

#include <QtGui/QColor>

SolidColorCommand::SolidColorCommand(QObject* parent)
    : AbstractCommand(parent)
{
}

const QString& SolidColorCommand::getColor() const
{
    return this->color;
}

const QString SolidColorCommand::getPremultipliedColor() const
{
    QString hexColor = this->color;
    hexColor.remove('#');

    // Support both #AARRGGBB (8-char) and #RRGGBB (6-char) formats.
    int alpha, red, green, blue;
    if (hexColor.length() >= 8)
    {
        alpha = hexColor.mid(0, 2).toInt(nullptr, 16);
        red = hexColor.mid(2, 2).toInt(nullptr, 16);
        green = hexColor.mid(4, 2).toInt(nullptr, 16);
        blue = hexColor.mid(6, 2).toInt(nullptr, 16);
    }
    else
    {
        alpha = 255;
        red = hexColor.mid(0, 2).toInt(nullptr, 16);
        green = hexColor.mid(2, 2).toInt(nullptr, 16);
        blue = hexColor.mid(4, 2).toInt(nullptr, 16);
    }

    // CasparCG expects premultiplied alpha for the color producer.
    red = (red * alpha) / 255;
    green = (green * alpha) / 255;
    blue = (blue * alpha) / 255;

    return QString("#%1%2%3%4").arg(alpha, 2, 16, QChar('0'))
                               .arg(red, 2, 16, QChar('0'))
                               .arg(green, 2, 16, QChar('0'))
                               .arg(blue, 2, 16, QChar('0'));
}

const QString& SolidColorCommand::getTransition() const
{
    return this->transition;
}

int SolidColorCommand::getTransitionDuration() const
{
    return this->transitionDuration;
}

const QString& SolidColorCommand::getDirection() const
{
    return this->direction;
}

const QString& SolidColorCommand::getTween() const
{
    return this->tween;
}

bool SolidColorCommand::getUseAuto() const
{
    return this->useAuto;
}

bool SolidColorCommand::getTriggerOnNext() const
{
    return this->triggerOnNext;
}

void SolidColorCommand::setColor(const QString& color)
{
    this->color = color;
    emit colorChanged(this->color);
    emit propertyChanged();
}

void SolidColorCommand::setTransition(const QString& transition)
{
    this->transition = transition;
    emit transitionChanged(this->transition);
    emit propertyChanged();
}

void SolidColorCommand::setTransitionDuration(int transitionDuration)
{
    this->transitionDuration = transitionDuration;
    emit transitionDurationChanged(this->transitionDuration);
    emit propertyChanged();
}

void SolidColorCommand::setDirection(const QString& direction)
{
    this->direction = direction;
    emit directionChanged(this->direction);
    emit propertyChanged();
}

void SolidColorCommand::setTween(const QString& tween)
{
    this->tween = tween;
    emit tweenChanged(this->tween);
    emit propertyChanged();
}

void SolidColorCommand::setUseAuto(bool useAuto)
{
    this->useAuto = useAuto;
    emit useAutoChanged(this->useAuto);
    emit propertyChanged();
}

void SolidColorCommand::setTriggerOnNext(bool triggerOnNext)
{
    this->triggerOnNext = triggerOnNext;
    emit triggerOnNextChanged(this->triggerOnNext);
    emit propertyChanged();
}

void SolidColorCommand::readProperties(boost::property_tree::wptree& pt)
{
    AbstractCommand::readProperties(pt);

    setTransition(QString::fromStdWString(pt.get(L"transition", Mixer::DEFAULT_TRANSITION.toStdWString())));
    setTransitionDuration(pt.get(L"transitionDuration", pt.get(L"transtitionDuration", Mixer::DEFAULT_DURATION)));
    setTween(QString::fromStdWString(pt.get(L"tween", Mixer::DEFAULT_TWEEN.toStdWString())));
    setDirection(QString::fromStdWString(pt.get(L"direction", Mixer::DEFAULT_DIRECTION.toStdWString())));
    setColor(QString::fromStdWString(pt.get(L"solidcolor", SolidColor::DEFAULT_COLOR.toStdWString())));
    setUseAuto(pt.get(L"useauto", SolidColor::DEFAULT_USE_AUTO));
    setTriggerOnNext(pt.get(L"triggeronnext", SolidColor::DEFAULT_TRIGGER_ON_NEXT));
}

void SolidColorCommand::writeProperties(QXmlStreamWriter& writer)
{
    AbstractCommand::writeProperties(writer);

    writer.writeTextElement("transition", getTransition());
    writer.writeTextElement("transitionDuration", QString::number(getTransitionDuration()));
    writer.writeTextElement("tween", getTween());
    writer.writeTextElement("direction", getDirection());
    writer.writeTextElement("solidcolor", getColor());
    writer.writeTextElement("useauto", (getUseAuto() == true) ? "true" : "false");
    writer.writeTextElement("triggeronnext", (getTriggerOnNext() == true) ? "true" : "false");
}

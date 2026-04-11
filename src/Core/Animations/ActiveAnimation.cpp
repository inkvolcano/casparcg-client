#include "ActiveAnimation.h"

#include "Global.h"

#include <QtCore/QPropertyAnimation>

#include <QtWidgets/QWidget>

ActiveAnimation::ActiveAnimation(QWidget* target, QObject* parent)
    : QObject(parent), target(target)
{
    this->animation = new QPropertyAnimation(this, "color");
    this->animation->setDuration(350);
    this->animation->setKeyValueAt(0, 255);
    this->animation->setKeyValueAt(1, 0);
}

ActiveAnimation::~ActiveAnimation()
{
    this->animation->stop();
}

void ActiveAnimation::start(int loopCount)
{
    this->animation->setLoopCount(loopCount);
    this->animation->start();
}

void ActiveAnimation::stop()
{
    this->animation->stop();
}

void ActiveAnimation::setChannel(int channel)
{
    this->channel = channel;
}

QColor ActiveAnimation::channelColor() const
{
    // Higher saturation and lightness than badge color for animation visibility.
    return QColor::fromHslF(ChannelColor::hue(this->channel) / 360.0, ChannelColor::activeSaturation(), ChannelColor::activeLightness());
}

int ActiveAnimation::color() const
{
    return this->value;
}

void ActiveAnimation::setColor(const int value)
{
    this->value = value;

    if (this->target.isNull())
        return;

    // Animate from white flash to channel color.
    QColor baseColor = channelColor();
    double t = (255.0 - value) / 255.0;  // t goes from 0 to 1 as animation progresses
    int r = static_cast<int>(255 + t * (baseColor.red() - 255));
    int g = static_cast<int>(255 + t * (baseColor.green() - 255));
    int b = static_cast<int>(255 + t * (baseColor.blue() - 255));

    this->target->setStyleSheet(QString("background-color: rgb(%1,%2,%3);").arg(r).arg(g).arg(b));
}

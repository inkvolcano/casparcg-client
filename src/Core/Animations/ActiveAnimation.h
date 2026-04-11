#pragma once

#include "../Shared.h"

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtGui/QColor>

class QWidget;
class QPropertyAnimation;

class CORE_EXPORT ActiveAnimation : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int color READ color WRITE setColor)

    public:
        explicit ActiveAnimation(QWidget* target, QObject* parent = 0);
        ~ActiveAnimation();

        void start(int loopCount = -1);
        void stop();

        void setChannel(int channel);

    private:
        int value = 255;
        int channel = 1;
        QPointer<QWidget> target;
        QPropertyAnimation* animation = nullptr;

        int color() const;
        void setColor(const int value);

        QColor channelColor() const;
};

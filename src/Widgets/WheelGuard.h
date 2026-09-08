#pragma once

#include <QtCore/QEvent>
#include <QtCore/QObject>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QAbstractScrollArea>
#include <QtWidgets/QAbstractSpinBox>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QSlider>
#include <QtWidgets/QWidget>

// Stops the mouse wheel from changing the value of input boxes that are not
// focused: a box only reacts to the wheel after it has been clicked. An
// unfocused box hands the wheel to the nearest scroll area instead, so
// scrolling a busy inspector page never edits values by accident.
class WheelGuard : public QObject
{
    public:
        explicit WheelGuard(QObject* parent) : QObject(parent) {}

        bool eventFilter(QObject* watched, QEvent* event) override
        {
            if (event->type() != QEvent::Wheel)
                return QObject::eventFilter(watched, event);

            QWidget* box = qobject_cast<QWidget*>(watched);
            if (box == nullptr || box->hasFocus())
                return QObject::eventFilter(watched, event);

            // Forward to the nearest scroll area so the page still scrolls.
            QWheelEvent* wheel = static_cast<QWheelEvent*>(event);
            QWidget* p = box->parentWidget();
            while (p != nullptr && qobject_cast<QAbstractScrollArea*>(p) == nullptr)
                p = p->parentWidget();
            if (QAbstractScrollArea* area = qobject_cast<QAbstractScrollArea*>(p))
            {
                QWheelEvent forwarded(
                    QPointF(area->viewport()->mapFromGlobal(wheel->globalPosition().toPoint())),
                    wheel->globalPosition(), wheel->pixelDelta(), wheel->angleDelta(),
                    wheel->buttons(), wheel->modifiers(), wheel->phase(), wheel->inverted());
                QApplication::sendEvent(area->viewport(), &forwarded);
            }

            return true; // the box itself never sees the wheel
        }

        // Guard one input widget.
        static void apply(QWidget* input)
        {
            input->setFocusPolicy(Qt::StrongFocus); // wheel alone can't grab focus
            input->installEventFilter(new WheelGuard(input));
        }

        // Guard every spin box, combo box and slider currently under root.
        static void applyToInputs(QWidget* root)
        {
            for (QAbstractSpinBox* spinBox : root->findChildren<QAbstractSpinBox*>())
                apply(spinBox);
            for (QComboBox* comboBox : root->findChildren<QComboBox*>())
                apply(comboBox);
            for (QSlider* slider : root->findChildren<QSlider*>())
                apply(slider);
        }
};

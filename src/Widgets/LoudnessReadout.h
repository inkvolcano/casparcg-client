#pragma once

#include "Shared.h"

#include <QtCore/QPointer>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE
class QLabel;
QT_END_NAMESPACE

class LoudnessMonitor;

// The loudness of a server channel beside its audio meters (build 312):
// short-term large, integrated and momentary under it, and where it comes
// from - R128 from NDI, or an estimate from OSC peaks marked with a ~.
// Measuring starts the first time it is shown and then keeps going; a
// double-click starts the integrated reading again.
class WIDGETS_EXPORT LoudnessReadout : public QWidget
{
    public:
        LoudnessReadout(const QString& server, int channel, QWidget* parent = nullptr);

    protected:
        void showEvent(QShowEvent* event) override;
        void mouseDoubleClickEvent(QMouseEvent* event) override;

    private:
        QPointer<LoudnessMonitor> monitor;
        QLabel* labelShort = nullptr;
        QLabel* labelDetail = nullptr;
        QLabel* labelSource = nullptr;
        QString shownColour;

        void refresh();
};

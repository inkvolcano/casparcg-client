#pragma once

#include "Shared.h"

#include "Global.h"
#include "Playout.h"

#include <QtCore/QByteArray>
#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>

QT_BEGIN_NAMESPACE
class QTreeWidgetItem;
class QWidget;
QT_END_NAMESPACE

class AbstractCommand;
class AbstractRundownWidget;

// The live run sheet (build 305): every open rundown as a page any phone or
// laptop on the network can open - http://<this machine>:<port>/runsheet -
// with what is on air marked and a countdown to the end of each playing clip,
// and of each fired item that has a length.
//
// Read-only, and open to anyone on the network (the user's choice): it shows
// the rundowns, it changes nothing. Off unless Settings > General > Serve a
// live run sheet is on; served by the client's own web server
// (SheetCacheServer), which that setting starts too.
//
// What fired when comes from RundownTreeWidget::executeCommand, the path every
// key, button, remote trigger and time trigger goes through; clips report their
// own server time (AbstractRundownWidget::clipTime). The rules are in
// Common/LiveRunSheetRules.h, tools/test-liverunsheet.
class WIDGETS_EXPORT LiveRunSheet : public QObject
{
    Q_OBJECT

    public:
        static LiveRunSheet& getInstance();

        static bool isEnabled();
        static void setEnabled(bool on);

        // Where the open rundowns are (MainWindow's rundown area), and which one is on screen.
        void setArea(QWidget* rundownArea);

        // A playout action ran on an item.
        void noteExecuted(AbstractRundownWidget* widget, Playout::PlayoutType type, int channel);

        // The page, and its data.
        static QByteArray page();
        QByteArray snapshot();

    private:
        explicit LiveRunSheet() {}

        QPointer<QWidget> area;

        struct OnAir
        {
            qint64 firedAt = 0;
            QString device;
            int channel = 0;
            int layer = 0;
        };
        QHash<const AbstractCommand*, OnAir> onAir;

        void forget(const AbstractCommand* command);
};

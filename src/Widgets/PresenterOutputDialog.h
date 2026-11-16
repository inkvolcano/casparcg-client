#pragma once

#include "Shared.h"

#include <QtWidgets/QDialog>

QT_BEGIN_NAMESPACE
class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
QT_END_NAMESPACE

// Rundown > Presenter View On A Channel (build 308): the live run sheet's
// presenter view - what is on air, its countdown, what comes next with its
// note - played by a server's HTML producer on a channel of the operator's
// choosing, for a confidence monitor fed from a server output.
//
// The server fetches the page from this client, so the live run sheet has to
// be served; the dialog says so and offers to turn it on. Take Off clears the
// place the page was last put on (remembered across restarts), never whatever
// the form happens to show, so it cannot clear something else on air. The
// strings are in Common/PresenterOutputRules.h (tools/test-presenteroutput).
class WIDGETS_EXPORT PresenterOutputDialog : public QDialog
{
    Q_OBJECT

    public:
        static void open(QWidget* parent);

        explicit PresenterOutputDialog(QWidget* parent = nullptr);

    private:
        QLabel* servingLabel = nullptr;
        QPushButton* buttonServe = nullptr;
        QComboBox* comboServer = nullptr;
        QSpinBox* spinChannel = nullptr;
        QSpinBox* spinLayer = nullptr;
        QLabel* statusLabel = nullptr;
        QPushButton* buttonOn = nullptr;
        QPushButton* buttonOff = nullptr;

        void refresh();
        void serverChosen();
        void putOn();
        void takeOff();
};

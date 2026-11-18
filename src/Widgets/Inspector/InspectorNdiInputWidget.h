#pragma once

#include "../Shared.h"

#include "Commands/NdiInputCommand.h"
#include "Events/Rundown/RundownItemSelectedEvent.h"
#include "Models/LibraryModel.h"

#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <QtWidgets/QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QSpinBox;
class QToolButton;

// The NDI Input item's section: which source, low bandwidth, and the transition.
// The source list comes from the item's server (NDI LIST), fetched the first
// time an NDI item on that server is selected and again with Find; any name
// can be typed as well, for a source that is not up yet.
class WIDGETS_EXPORT InspectorNdiInputWidget : public QWidget
{
    Q_OBJECT

    public:
        explicit InspectorNdiInputWidget(QWidget* parent = 0);

    private:
        LibraryModel* model = nullptr;
        NdiInputCommand* command = nullptr;

        QComboBox* comboSource = nullptr;
        QToolButton* buttonFind = nullptr;
        QLabel* labelSourceStatus = nullptr;
        QCheckBox* checkLowBandwidth = nullptr;
        QComboBox* comboTransition = nullptr;
        QSpinBox* spinDuration = nullptr;
        QComboBox* comboTween = nullptr;
        QComboBox* comboDirection = nullptr;

        // Sources per server name, as last listed; a server is asked once on its
        // own and after that only when Find is pressed.
        QHash<QString, QStringList> listed;
        bool asking = false;

        void blockAllSignals(bool block);
        void fillSources(const QStringList& sources);
        void findSources();
        void sourceEdited(const QString& source);

        Q_SLOT void rundownItemSelected(const RundownItemSelectedEvent&);
};

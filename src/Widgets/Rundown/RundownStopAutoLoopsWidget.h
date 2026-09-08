#pragma once

#include "../Shared.h"
#include "AbstractRundownWidget.h"
#include "ui_RundownStopAutoLoopsWidget.h"

#include "Global.h"

#include "Animations/ActiveAnimation.h"
#include "Commands/AbstractCommand.h"
#include "Commands/AbstractPlayoutCommand.h"
#include "Commands/StopAutoLoopsCommand.h"
#include "Events/Inspector/LabelChangedEvent.h"
#include "Models/LibraryModel.h"

#include <QtCore/QString>
#include <QtCore/QObject>

#include <QtWidgets/QWidget>

// Panic item: when fired, stops every running auto-loop countdown in the client
// so no auto-looping item re-fires until re-armed.
class WIDGETS_EXPORT RundownStopAutoLoopsWidget : public QWidget, Ui::RundownStopAutoLoopsWidget, public AbstractRundownWidget, public AbstractPlayoutCommand
{
    Q_OBJECT

    public:
        explicit RundownStopAutoLoopsWidget(const LibraryModel& model, QWidget* parent = 0,
                                            const QString& color = Color::DEFAULT_TRANSPARENT_COLOR,
                                            bool active = false, bool inGroup = false, bool compactView = false);

        virtual AbstractRundownWidget* clone();

        virtual bool isGroup() const;
        virtual bool isInGroup() const;
        virtual bool executeCommand(Playout::PlayoutType type);

        virtual AbstractCommand* getCommand();
        virtual LibraryModel* getLibraryModel();

        virtual QString getColor() const;

        virtual void setExpanded(bool /* expanded */) {}
        virtual void setActive(bool active);
        virtual void setInGroup(bool inGroup);
        virtual void setColor(const QString& color);
        virtual void readProperties(boost::property_tree::wptree& pt);
        virtual void writeProperties(QXmlStreamWriter& writer);
        virtual void setCompactView(bool compactView);
        virtual void clearDelayedCommands() {}
        virtual void setUsed(bool used);
        virtual void setSelected(bool selected);
        virtual void setRundownDisabled(bool disabled);

    private:
        bool active;
        bool inGroup;
        bool compactView;
        QString color;
        LibraryModel model;
        StopAutoLoopsCommand command;
        ActiveAnimation* animation;
        bool markUsedItems;
        bool selected = false;

        Q_SLOT void labelChanged(const LabelChangedEvent&);
};

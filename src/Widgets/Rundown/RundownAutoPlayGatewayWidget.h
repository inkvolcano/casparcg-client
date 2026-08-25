#pragma once

#include "../Shared.h"
#include "AbstractRundownWidget.h"
#include "ui_RundownAutoPlayGatewayWidget.h"

#include "Global.h"

#include "Animations/ActiveAnimation.h"
#include "Commands/AbstractCommand.h"
#include "Commands/AbstractPlayoutCommand.h"
#include "Commands/GatewayCommand.h"
#include "Events/Inspector/LabelChangedEvent.h"
#include "Models/LibraryModel.h"

#include <stdexcept>

#include <QtCore/QString>
#include <QtCore/QObject>

#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

class QTimer;
class QTreeWidgetItem;

class WIDGETS_EXPORT RundownAutoPlayGatewayWidget : public QWidget, Ui::RundownAutoPlayGatewayWidget, public AbstractRundownWidget, public AbstractPlayoutCommand
{
    Q_OBJECT

    public:
        explicit RundownAutoPlayGatewayWidget(const LibraryModel& model, QWidget* parent = 0,
                                             const QString& color = Color::DEFAULT_AUTOPLAYGATEWAY_COLOR,
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
        virtual void setUsed(bool /* used */) {}
        virtual void setSelected(bool selected);
        virtual void setRundownDisabled(bool disabled);

        void updateVisuals();
        void setTreeItem(QTreeWidgetItem* item);
        void rebuildExitButtons();

    Q_SIGNALS:
        void requestFocusJumpToEntrance(const QString& gatewayId);

    private:
        bool active;
        bool inGroup;
        bool compactView;
        QString color;
        LibraryModel model;
        GatewayCommand command;
        ActiveAnimation* animation;
        bool selected = false;

        QWidget* buttonContainer = nullptr;
        QVBoxLayout* buttonLayout = nullptr;
        QTreeWidgetItem* treeItem = nullptr;
        QTimer* conditionTimer = nullptr;

        Q_SLOT void labelChanged(const LabelChangedEvent&);
        Q_SLOT void gatewayExitsChanged(const QString& gatewayId);
};

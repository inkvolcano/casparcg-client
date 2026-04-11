#pragma once

#include "../Shared.h"
#include "AbstractRundownWidget.h"
#include "Commands/AbstractPlayoutCommand.h"
#include "ui_RundownCommandGatewayWidget.h"

#include "Global.h"

#include "Animations/ActiveAnimation.h"
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

class WIDGETS_EXPORT RundownCommandGatewayWidget : public QWidget, Ui::RundownCommandGatewayWidget, public AbstractRundownWidget, public AbstractPlayoutCommand
{
    Q_OBJECT

    public:
        explicit RundownCommandGatewayWidget(const LibraryModel& model, QWidget* parent = 0,
                                             const QString& color = Color::DEFAULT_COMMANDGATEWAY_COLOR,
                                             bool active = false, bool inGroup = false, bool compactView = false);

        virtual AbstractRundownWidget* clone();
        virtual bool isGroup() const;
        virtual bool isInGroup() const;
        virtual AbstractCommand* getCommand();
        virtual LibraryModel* getLibraryModel();

        virtual QString getColor() const;
        virtual void setColor(const QString& color);
        virtual void setActive(bool active);
        virtual void setInGroup(bool inGroup);
        virtual bool executeCommand(Playout::PlayoutType type);
        virtual void readProperties(boost::property_tree::wptree& pt);
        virtual void writeProperties(QXmlStreamWriter& writer);
        virtual void setCompactView(bool compactView);
        virtual void clearDelayedCommands() {}
        virtual void setUsed(bool /* used */) {}
        virtual void setExpanded(bool /* expanded */) {}
        virtual void setSelected(bool selected);

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

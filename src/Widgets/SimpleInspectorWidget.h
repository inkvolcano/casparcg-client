#pragma once

#include "Shared.h"

#include "Commands/AbstractCommand.h"
#include "Events/Rundown/EmptyRundownEvent.h"
#include "Events/Rundown/RundownItemSelectedEvent.h"
#include "Models/LibraryModel.h"

#include <QtCore/QPointer>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMenu>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QTreeWidgetItem>
#include <QtWidgets/QWidget>

class TemplateCommand;

// Simple Mode's stripped-down inspector panel: label, channel/videolayer and
// (for templates) the key/value data with an Update button. Header buttons and
// editing behavior mirror the full template inspector (+/- top right, edit via
// double-click); the panel carries the standard panel menu (move, size, anchor).
class WIDGETS_EXPORT SimpleInspectorWidget : public QWidget
{
    Q_OBJECT

    public:
        explicit SimpleInspectorWidget(QWidget* parent = nullptr);

    private:
        // QPointers: the command dies whenever its rundown item is deleted or
        // rebuilt (move, undo, reload) — these auto-null instead of dangling.
        QPointer<AbstractCommand> command;
        LibraryModel* model = nullptr;
        QPointer<TemplateCommand> templateCommand;
        bool loading = false;
        bool collapsed = false;

        QTabWidget* tabWidget;
        QLineEdit* lineEditLabel;
        QSpinBox* spinBoxChannel;
        QSpinBox* spinBoxVideolayer;
        QTreeWidget* treeWidgetData;
        QPushButton* buttonUpdate;
        QToolButton* buttonAddKey;
        QToolButton* buttonRemoveKey;

        QToolButton* menuButton = nullptr;
        QMenu* dropdownMenu = nullptr;
        QAction* expandCollapseAction = nullptr;

        void setupMenus();
        void syncDataToCommand();

        Q_SLOT void rundownItemSelected(const RundownItemSelectedEvent&);
        Q_SLOT void emptyRundown(const EmptyRundownEvent&);
        Q_SLOT void updateClicked();
        Q_SLOT void addKeyClicked();
        Q_SLOT void editKeyClicked();
        Q_SLOT void removeKeyClicked();
        Q_SLOT void toggleExpandCollapse();
};

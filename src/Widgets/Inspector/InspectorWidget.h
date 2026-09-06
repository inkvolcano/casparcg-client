#pragma once

#include "../Shared.h"
#include "ui_InspectorWidget.h"

#include "Events/Library/LibraryItemSelectedEvent.h"
#include "Events/Rundown/RundownItemSelectedEvent.h"
#include "Events/Rundown/EmptyRundownEvent.h"
#include "Events/Rundown/RepositoryRundownEvent.h"

#include <QtCore/QEvent>
#include <QtCore/QObject>

#include <QtWidgets/QMenu>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QWidget>

class WIDGETS_EXPORT InspectorWidget : public QWidget, Ui::InspectorWidget
{
    Q_OBJECT

    public:
        explicit InspectorWidget(QWidget* parent = 0);

    private:
        bool collapsed = false;
        QTabWidget* tabWidgetInspector;
        QToolButton* menuButton = nullptr;
        QMenu* dropdownMenu = nullptr;
        QAction* expandCollapseAction = nullptr;

        // The Simple Mode section is declared last but shown first; every index in the
        // .cpp is a declaration index and goes through here to reach the right row.
        static const int SIMPLE_MODE_SECTION = 42;
        static const int TEMPLATE_SECTION = 3;

        // Mounted directly under the Template section and collapsed by default. Held
        // by pointer, never by index: it is inserted mid-tree, so sectionItem() adds
        // a row for everything declared after Template once this exists.
        QTreeWidgetItem* templateSettingsTopLevel = nullptr;
        QTreeWidgetItem* sectionItem(int declaredIndex) const;

        void setDefaultVisibleWidgets();

        Q_SLOT void toggleExpandCollapse();
        Q_SLOT void toggleExpandItem(QTreeWidgetItem*, int);
        Q_SLOT void emptyRundown(const EmptyRundownEvent&);
        Q_SLOT void rundownItemSelected(const RundownItemSelectedEvent&);
        Q_SLOT void libraryItemSelected(const LibraryItemSelectedEvent&);
        Q_SLOT void repositoryRundown(const RepositoryRundownEvent&);
};

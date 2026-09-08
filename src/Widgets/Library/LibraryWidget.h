#pragma once

#include "../Shared.h"
#include "ui_LibraryWidget.h"

#include "CasparDevice.h"

#include "Events/DataChangedEvent.h"
#include "Events/MediaChangedEvent.h"
#include "Events/ExportPresetEvent.h"
#include "Events/ImportPresetEvent.h"
#include "Events/PresetChangedEvent.h"
#include "Events/Inspector/TemplateChangedEvent.h"
#include "Events/Rundown/RepositoryRundownEvent.h"
#include "Models/LibraryModel.h"

#include <QtCore/QPoint>

#include <QtGui/QKeyEvent>

#include <QtGui/QAction>
#include <QtWidgets/QMenu>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QWidget>

class WIDGETS_EXPORT LibraryWidget : public QWidget, Ui::LibraryWidget
{
    Q_OBJECT

    public:
        explicit LibraryWidget(QWidget* parent = 0);

        static int dropChannel();
        static int dropVideolayer();

    private:
        // The Library could sort by name and nothing else, while the server sent
        // the size and the date on every listing and the client threw them away.
        QComboBox* comboBoxSort = nullptr;

        void buildSortControl();
        void applySort(QList<LibraryModel>& models) const;

        bool lock = false;
        bool useDropFrameNotation = false;
        bool collapsed = false;
        QTabWidget* tabWidgetLibrary;
        QToolButton* menuButton = nullptr;
        QMenu* dropdownMenu = nullptr;
        QAction* expandCollapseAction = nullptr;

        QMenu* contextMenu;
        QMenu* contextMenuImage;
        QMenu* contextMenuPreset;
        QMenu* contextMenuData;
        QSharedPointer<LibraryModel> model;

        void setupTools();
        void setupUiMenu();
        void checkEmptyFilter();

        Q_SLOT void loadLibrary();
        Q_SLOT void toggleExpandItem(QTreeWidgetItem*, int);
        Q_SLOT void filterLibrary();
        Q_SLOT void contextMenuTriggered(QAction*);
        Q_SLOT void contextMenuImageTriggered(QAction*);
        Q_SLOT void contextMenuPresetTriggered(QAction*);
        Q_SLOT void contextMenuDataTriggered(QAction*);
        Q_SLOT void customContextMenuRequested(const QPoint&);
        Q_SLOT void customContextMenuImageRequested(const QPoint&);
        Q_SLOT void customContextMenuPresetRequested(const QPoint&);
        Q_SLOT void customContextMenuDataRequested(const QPoint&);
        Q_SLOT void currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*);
        Q_SLOT void itemDoubleClicked(QTreeWidgetItem*, int);
        Q_SLOT void mediaChanged(const MediaChangedEvent&);
        // Appends the OGraf graphics found under each device's template folder to
        // the Templates list. Does nothing at all unless OGraf is switched on,
        // which is what keeps a template folder off the refresh path for anyone
        // who does not use it.
        void appendOgrafGraphics();

        Q_SLOT void templateChanged(const TemplateChangedEvent&);
        Q_SLOT void dataChanged(const DataChangedEvent&);
        Q_SLOT void presetChanged(const PresetChangedEvent&);
        Q_SLOT void importPreset(const ImportPresetEvent&);
        Q_SLOT void exportPreset(const ExportPresetEvent&);
        Q_SLOT void repositoryRundown(const RepositoryRundownEvent&);
        Q_SLOT void toggleExpandCollapse();
};

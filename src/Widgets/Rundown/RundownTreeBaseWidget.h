#pragma once

#include "../Shared.h"
#include "AbstractRundownWidget.h"

#include "Global.h"

#include "OscSubscription.h"
#include "Events/AddPresetItemEvent.h"
#include "Events/Inspector/ChannelChangedEvent.h"
#include "Events/Rundown/RepositoryRundownEvent.h"
#include "Models/LibraryModel.h"
#include "Models/RepositoryChangeModel.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <QtCore/QModelIndexList>
#include <QtCore/QMimeData>
#include <QtCore/QRect>
#include <QtCore/QXmlStreamWriter>

#include <QtGui/QDragEnterEvent>
#include <QtGui/QDropEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPixmap>

#include <QtCore/QPointer>
#include <QtCore/QVector>

#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QTreeWidgetItem>
#include <QtGui/QUndoStack>
#include <QtWidgets/QWidget>

class AbstractCommand;

class WIDGETS_EXPORT RundownTreeBaseWidget : public QTreeWidget
{
    Q_OBJECT

    public:
        explicit RundownTreeBaseWidget(QWidget* parent = 0);

        bool getCompactView() const;
        void setCompactView(bool compactView);

        bool isLocked() const;
        void setLocked(bool locked);

        QStringList mimeTypes() const;
        Qt::DropActions supportedDropActions() const;
        void dragEnterEvent(QDragEnterEvent* event);

        AbstractRundownWidget* readProperties(boost::property_tree::wptree& pt);
        void writeProperties(QTreeWidgetItem* item, QXmlStreamWriter& writer) const;

        bool pasteSelectedItems(bool repositoryRundown = false, bool preserveCloneLinks = false);
        bool pasteAsLinkedClones();
        bool pasteItemProperties();
        bool pasteItemPropertiesNoData();
        bool duplicateSelectedItems();
        bool copySelectedItems();
        bool hasItemBelow() const;

        void moveItemUp();
        void moveItemDown();
        void moveItemIntoGroup();
        // Move the current item into a named group rather than the one above it.
        // The Simple Mode shotbox needs this: the target is a key on a grid, not
        // a neighbouring row. Returns false when the move is refused.
        bool moveCurrentItemInto(QTreeWidgetItem* targetGroup);
        void moveItemOutOfGroup();
        void groupItems();
        void ungroupItems();
        void removeSelectedItems();
        void toggleDisableSelectedItems();
        void applyDisabledVisual(QTreeWidgetItem* item, bool effectiveDisabled);
        void refreshDisabledVisual(QTreeWidgetItem* item);
        void removeAllItems();
        void selectItemAbove();
        void selectItemBelow();
        void checkEmptyRundown();
        void checRepositoryChanges();
        void applyRepositoryChanges();
        void copyItemProperties();
        void addRepositoryChange(const RepositoryChangeModel& model);
        void setExpanded(bool expanded);
        void updateGroupWidget(QTreeWidgetItem* item);
        void updateAllGroupWidgets();

        virtual bool dropMimeData(QTreeWidgetItem* parent, int index, const QMimeData* data, Qt::DropAction action);

        QUndoStack* undoStack() const { return m_undoStack; }
        QString serializeTree() const;
        void restoreFromSnapshot(const QString& xml);
        void beginUndoSnapshot(const QString& description);
        void endUndoSnapshot();
        bool isUndoRestoring() const { return m_undoRestoring; }

        static int getItemDepth(QTreeWidgetItem* item);

        static RundownTreeBaseWidget* dragSourceWidget;
        static bool s_isCutOperation;
        static QVector<QPointer<AbstractCommand>> s_copiedCommands;

    protected:
        void keyPressEvent(QKeyEvent* event);
        void mouseMoveEvent(QMouseEvent* event);
        void mousePressEvent(QMouseEvent* event);
        void dragMoveEvent(QDragMoveEvent* event);
        void dragLeaveEvent(QDragLeaveEvent* event);
        void paintEvent(QPaintEvent* event);

    private:
        bool compactView;
        QString theme;
        bool lock;

        QUndoStack* m_undoStack = nullptr;
        QString m_pendingUndoBefore;
        QString m_pendingUndoDescription;
        int m_undoNestingDepth = 0;
        bool m_undoRestoring = false;

        QPoint dragStartPosition;
        QRect m_dropIndicatorRect;
        bool m_showDropIndicator = false;

        QList<RepositoryChangeModel> repositoryChanges;

        QString currentItemStoryId();
        void removeRepositoryItem(const QString& storyId);
        bool containsStoryId(const QString& storyId, const QString& data);
        void addRepositoryItem(const QString& storyId, const QString& data);

        Q_SLOT void repositoryRundown(const RepositoryRundownEvent&);
    Q_SLOT void channelChanged(const ChannelChangedEvent&);
    Q_SLOT void undoLimitChanged(int limit);

    Q_SIGNALS:
        void libraryItemDropped(const LibraryModel& model);
        void presetItemDropped(const QString& preset);
        void itemsChanged();
};

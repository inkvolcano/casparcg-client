#pragma once

#include "Shared.h"

#include "Events/Rundown/ActiveRundownChangedEvent.h"
#include "Events/Rundown/CloseRundownEvent.h"
#include "Events/Rundown/EmptyRundownEvent.h"
#include "Events/Rundown/OpenRundownEvent.h"
#include "Events/Rundown/ReloadRundownEvent.h"
#include "Events/Inspector/LabelChangedEvent.h"
#include "Events/Rundown/RundownItemSelectedEvent.h"

#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QWidget>

class QKeyEvent;
class QResizeEvent;
class QShowEvent;

class RundownWidget;
class TemplateCommand;
class RundownTreeWidget;
class RundownTreeBaseWidget;
class AbstractCommand;
class QTreeWidgetItem;

// Simple Mode center view: a Companion-style grid of square button slots.
// Flagged rundown items occupy slots (persisted per item as simpleModeSlot in
// the rundown XML); remaining slots render as empty placeholders. The bottom
// bar moves the selected button between slots.
class WIDGETS_EXPORT SimpleModeWidget : public QWidget
{
    Q_OBJECT

    public:
        explicit SimpleModeWidget(RundownWidget* rundownWidget, QWidget* parent = nullptr);

        // Rebuild the grid from the active rundown's flagged items.
        void refresh();

    private:
        // One invoke sub-button on a grid key. For a template key these come from
        // the template itself; for a group key they are lifted from child
        // templates flagged with "Show invokes on group button".
        struct InvokeButton
        {
            QTreeWidgetItem* item = nullptr;      // item the invoke fires on
            TemplateCommand* command = nullptr;
            QString function;
            QString label;
        };

        struct SlotEntry
        {
            QTreeWidgetItem* item = nullptr;
            AbstractCommand* command = nullptr;
            int spanRows = 1;   // buttons with sub-buttons grow downward into extra slots
            int spanCols = 1;   // explicit double-width keys occupy two columns
        };

        RundownWidget* rundownWidget;
        QScrollArea* scrollArea;
        QWidget* gridContainer;
        QGridLayout* gridLayout;
        QWidget* moveBar;
        QPushButton* moveButton;
        QPushButton* removeButton;
        QLabel* moveHint;
        QSpinBox* spinBoxColumns;
        QSpinBox* spinBoxRows;
        bool moveMode = false;

        QMap<int, SlotEntry> slotEntries;         // slot index -> occupant
        QMap<int, QFrame*> emptySlotFrames;       // slot index -> placeholder (move targets)
        QList<QFrame*> cellFrames;                // parallel to buttonCommands
        QList<AbstractCommand*> buttonCommands;   // for selection highlight
        QList<QWidget*> cellTallies;              // last-fired strip along each key's top
        QList<QTreeWidgetItem*> cellItems;        // which item each key fires
        QMap<int, QTreeWidgetItem*> lastFiredByChannel;
        // QPointer: auto-nulls if the selected item is deleted from the rundown.
        QPointer<AbstractCommand> selectedCommand;
        int gridColumns = 4;
        int gridRows = 3;
        int renderedRows = 3;   // rows actually built (page rows + overflow + spare)
        int cellSize = 116;
        int cursorSlot = -1;    // arrow-key cursor position (buttons and empty slots)
        bool showPreviewControls = true;   // PVW control on each key (Settings -> Simple Mode)
        QTimer* resizeDebounce = nullptr;
        QTimer* contentDebounce = nullptr;   // coalesces invoke edits into one rebuild
        bool rebuildPending = false;         // structure changed while hidden

        void clearGrid();
        void updateTallies();
        void materializeSlots();
        void remapSlotsForColumnCount();
        // A grid edit is an edit to the rundown, so it belongs on the rundown's undo
        // stack. The snapshot already carries the Simple Mode properties, because they
        // are persisted with the item.
        RundownTreeBaseWidget* undoTarget() const;

        void collectFlaggedItems(RundownTreeWidget* tree, QList<QTreeWidgetItem*>& result) const;
        int computeControlsHeight(AbstractCommand* command, bool showPlayStop) const;
        int computeSpanCols(AbstractCommand* command) const;
        int computeSpanRows(RundownTreeWidget* tree, QTreeWidgetItem* item, AbstractCommand* command, bool showPlayStop) const;
        QList<QTreeWidgetItem*> collectShotboxRows(RundownTreeWidget* tree, QTreeWidgetItem* item) const;
        QList<InvokeButton> collectInvokeButtons(RundownTreeWidget* tree, QTreeWidgetItem* item) const;
        void assignSlots(RundownTreeWidget* tree, const QList<QTreeWidgetItem*>& items, bool showPlayStop);
        QWidget* buildButtonCell(RundownTreeWidget* tree, QTreeWidgetItem* item, bool showPlayStop, int cellSize, int spanRows, int spanCols);
        QWidget* buildEmptySlot(int slotIndex, int cellSize);
        void applySelectionStyles();
        void styleCellFrame(QFrame* frame, const QString& color, bool selected) const;
        void styleEmptySlot(QFrame* frame, bool targetable, bool cursor) const;
        void updateEmptySlotStyles();
        void updateMoveBarState();
        void setMoveMode(bool active);
        void placeSelectedAt(int targetSlot);
        void navigateCursor(int dx, int dy);
        int computeCellSize() const;

        Q_SLOT void rundownChanged();
        Q_SLOT void rundownStructureChanged();
        Q_SLOT void rundownItemFired(QTreeWidgetItem* item, int channel);
        Q_SLOT void invokeDataChanged();
        Q_SLOT void labelChanged(const LabelChangedEvent&);
        Q_SLOT void rundownItemSelected(const RundownItemSelectedEvent&);

    protected:
        void resizeEvent(QResizeEvent* event) override;
        void showEvent(QShowEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;
};

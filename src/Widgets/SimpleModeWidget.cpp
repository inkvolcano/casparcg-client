#include "SimpleModeWidget.h"

#include "Global.h"

#include "DatabaseManager.h"
#include "EventManager.h"
#include "Commands/AbstractCommand.h"
#include "Commands/GroupCommand.h"
#include "Commands/TemplateCommand.h"
#include "Events/Rundown/ExecuteRundownItemEvent.h"
#include "Rundown/AbstractRundownWidget.h"
#include "Rundown/RundownTreeWidget.h"
#include "Rundown/RundownUndoCommands.h"
#include "Rundown/RundownWidgetHelper.h"
#include "Rundown/RundownWidget.h"
#include "SimpleIconDialog.h"
#include "WheelGuard.h"

#include <functional>

#include <QtCore/QSet>
#include <QtGui/QFont>
#include <QtGui/QFontMetrics>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QResizeEvent>
#include <QtGui/QShowEvent>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QTreeWidgetItem>
#include <QtWidgets/QVBoxLayout>

namespace
{
    const int GRID_SPACING = 6;
    const int SPARE_ROWS = 1;        // empty rows shown below the last occupied one
    const int MIN_CELL_SIZE = 90;
    const int MAX_CELL_SIZE = 420;
    const int CELL_MARGIN = 5;       // buildButtonCell contents margins
    const int CELL_SPACING = 3;      // spacing inside a cell (controls row etc.)
    const int INVOKE_ROW_HEIGHT = 24;
    const int DROPDOWN_HEIGHT = 30;  // chooser on a "treat as dropdown" group key
    const int SUBLABEL_HEIGHT = 15;  // selected-item line under a dropdown key's label
    const int BADGE_HEIGHT = 15;     // channel/layer bar under the control row
    const int TALLY_HEIGHT = 5;      // last-fired strip along the top of a key

    // Clickable cell frame: left press selects the item, right press opens the
    // button's context menu (label size, icon, remove).
    class SimpleCellFrame : public QFrame
    {
        public:
            explicit SimpleCellFrame(QWidget* parent) : QFrame(parent) {}
            std::function<void()> onPress;
            std::function<void(const QPoint&)> onContext;   // global position

        protected:
            void mousePressEvent(QMouseEvent* event) override
            {
                if (event->button() == Qt::LeftButton && this->onPress)
                    this->onPress();
                else if (event->button() == Qt::RightButton && this->onContext)
                    this->onContext(event->globalPosition().toPoint());
                QFrame::mousePressEvent(event);
            }
    };
}

SimpleModeWidget::SimpleModeWidget(RundownWidget* rundownWidget, QWidget* parent)
    : QWidget(parent), rundownWidget(rundownWidget)
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // Arrow keys walk a cursor over the grid (buttons and empty slots alike).
    setFocusPolicy(Qt::StrongFocus);

    this->scrollArea = new QScrollArea(this);
    this->scrollArea->setWidgetResizable(false); // container sizes itself from the fixed grid
    this->scrollArea->setFrameShape(QFrame::NoFrame);
    this->scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    this->scrollArea->setFocusPolicy(Qt::NoFocus); // arrows must reach us, not scroll

    this->gridContainer = new QWidget(this->scrollArea);
    this->gridLayout = new QGridLayout(this->gridContainer);
    this->gridLayout->setContentsMargins(0, 0, 0, 0);
    this->gridLayout->setSpacing(GRID_SPACING);
    this->gridLayout->setSizeConstraint(QLayout::SetFixedSize);

    this->scrollArea->setWidget(this->gridContainer);
    mainLayout->addWidget(this->scrollArea, 1);

    // Bottom bar: move the selected button between slots (Companion-style).
    this->moveBar = new QWidget(this);
    QHBoxLayout* moveLayout = new QHBoxLayout(this->moveBar);
    moveLayout->setContentsMargins(2, 0, 2, 0);
    moveLayout->setSpacing(4);

    // Grid dimensions: cells scale so this many columns x rows fill the panel.
    QLabel* gridLabel = new QLabel("Grid:", this->moveBar);
    gridLabel->setStyleSheet("color: rgba(170, 170, 170, 200); font-size: 11px;");
    moveLayout->addWidget(gridLabel);

    this->spinBoxColumns = new QSpinBox(this->moveBar);
    this->spinBoxColumns->setRange(1, 12);
    WheelGuard::apply(this->spinBoxColumns); // wheel only after clicking the box
    this->spinBoxColumns->setToolTip("Columns");
    QString colsValue = DatabaseManager::getInstance().getConfigurationByName("SimpleModeColumns").getValue();
    this->spinBoxColumns->setValue(colsValue.isEmpty() ? 4 : qBound(1, colsValue.toInt(), 12));
    moveLayout->addWidget(this->spinBoxColumns);

    QLabel* xLabel = new QLabel(QString::fromUtf8("\xc3\x97"), this->moveBar);
    xLabel->setStyleSheet("color: rgba(170, 170, 170, 200); font-size: 11px;");
    moveLayout->addWidget(xLabel);

    this->spinBoxRows = new QSpinBox(this->moveBar);
    this->spinBoxRows->setRange(1, 12);
    WheelGuard::apply(this->spinBoxRows); // wheel only after clicking the box
    this->spinBoxRows->setToolTip("Rows");
    QString rowsValue = DatabaseManager::getInstance().getConfigurationByName("SimpleModeRows").getValue();
    this->spinBoxRows->setValue(rowsValue.isEmpty() ? 3 : qBound(1, rowsValue.toInt(), 12));
    moveLayout->addWidget(this->spinBoxRows);

    QObject::connect(this->spinBoxColumns, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "SimpleModeColumns", QString::number(value)));
        refresh();
    });
    QObject::connect(this->spinBoxRows, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "SimpleModeRows", QString::number(value)));
        refresh();
    });

    moveLayout->addSpacing(12);

    // Two-step move (Companion-style): arm Move, then click a free slot.
    this->moveButton = new QPushButton("Move", this->moveBar);
    this->moveButton->setCheckable(true);
    this->moveButton->setFixedHeight(24);
    this->moveButton->setFocusPolicy(Qt::NoFocus);
    this->moveButton->setToolTip("Move the selected button: press, then click a free slot");
    this->moveButton->setStyleSheet(
        "QPushButton { background-color: rgba(50, 50, 50, 220); color: white; border-radius: 3px;"
        " border: 1px solid rgba(80, 80, 80, 200); font-size: 11px; padding: 0px 12px; }"
        "QPushButton:hover { background-color: rgba(75, 75, 75, 220); }"
        "QPushButton:checked { background-color: rgba(60, 100, 150, 230); border: 1px solid rgba(100, 140, 190, 220); }"
        "QPushButton:disabled { color: rgba(100, 100, 100, 150); }");
    moveLayout->addWidget(this->moveButton);

    QObject::connect(this->moveButton, &QPushButton::toggled, this, [this](bool checked) {
        setMoveMode(checked);
    });

    moveLayout->addSpacing(12);

    // Remove: takes the button off the grid by clearing its "Show in Simple Mode"
    // flag — the rundown item itself is never touched.
    this->removeButton = new QPushButton("Remove", this->moveBar);
    this->removeButton->setFixedHeight(24);
    this->removeButton->setFocusPolicy(Qt::NoFocus);
    this->removeButton->setToolTip("Remove the selected button from the grid (the rundown item itself is kept)");
    this->removeButton->setStyleSheet(
        "QPushButton { background-color: rgba(110, 45, 45, 220); color: white; border-radius: 3px;"
        " border: 1px solid rgba(140, 75, 75, 200); font-size: 11px; padding: 0px 10px; }"
        "QPushButton:hover { background-color: rgba(140, 60, 60, 220); }"
        "QPushButton:disabled { color: rgba(120, 100, 100, 150); }");
    moveLayout->addWidget(this->removeButton);

    this->moveHint = new QLabel("Arrow keys move the cursor. Select a button to move or remove it; save the rundown to keep the arrangement.", this->moveBar);
    this->moveHint->setStyleSheet("color: rgba(130, 130, 130, 180); font-size: 10px;");
    moveLayout->addWidget(this->moveHint);
    moveLayout->addStretch();

    mainLayout->addWidget(this->moveBar, 0);

    QObject::connect(this->removeButton, &QPushButton::clicked, this, [this]() {
        if (this->selectedCommand == nullptr)
            return;
        setMoveMode(false);

        {
            UndoScope scope(undoTarget(), "Remove From Simple Mode");
            this->selectedCommand->setShowInSimpleMode(false);
        }

        refresh();
    });

    // Debounced responsive sizing: cells recompute to fill the panel width.
    this->resizeDebounce = new QTimer(this);
    this->resizeDebounce->setSingleShot(true);
    this->resizeDebounce->setInterval(120);
    QObject::connect(this->resizeDebounce, &QTimer::timeout, this, [this]() {
        if (this->isVisible() && computeCellSize() != this->cellSize)
            refresh();
    });

    // Invoke edits (reorder, label change) rebuild the grid so sub-buttons match
    // the inspector; coalesced so typing a label doesn't rebuild per keystroke.
    this->contentDebounce = new QTimer(this);
    this->contentDebounce->setSingleShot(true);
    this->contentDebounce->setInterval(250);
    QObject::connect(this->contentDebounce, &QTimer::timeout, this, [this]() {
        if (this->isVisible())
            refresh();
    });

    QObject::connect(&EventManager::getInstance(), SIGNAL(activeRundownChanged(const ActiveRundownChangedEvent&)), this, SLOT(rundownChanged()));
    QObject::connect(&EventManager::getInstance(), SIGNAL(openRundown(const OpenRundownEvent&)), this, SLOT(rundownChanged()));
    QObject::connect(&EventManager::getInstance(), SIGNAL(closeRundown(const CloseRundownEvent&)), this, SLOT(rundownChanged()));
    QObject::connect(&EventManager::getInstance(), SIGNAL(reloadRundown(const ReloadRundownEvent&)), this, SLOT(rundownChanged()));
    QObject::connect(&EventManager::getInstance(), SIGNAL(emptyRundown(const EmptyRundownEvent&)), this, SLOT(rundownChanged()));
    QObject::connect(&EventManager::getInstance(), SIGNAL(rundownStructureChanged()), this, SLOT(rundownStructureChanged()));
    QObject::connect(&EventManager::getInstance(), SIGNAL(rundownItemFired(QTreeWidgetItem*, int)), this, SLOT(rundownItemFired(QTreeWidgetItem*, int)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(rundownItemSelected(const RundownItemSelectedEvent&)), this, SLOT(rundownItemSelected(const RundownItemSelectedEvent&)));
}

void SimpleModeWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (this->resizeDebounce != nullptr)
        this->resizeDebounce->start();
}

void SimpleModeWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    if (this->rebuildPending)
        refresh();

    setFocus(); // arrow keys work immediately when Simple Mode appears
}

void SimpleModeWidget::keyPressEvent(QKeyEvent* event)
{
    int dx = 0, dy = 0;
    switch (event->key())
    {
        case Qt::Key_Left:  dx = -1; break;
        case Qt::Key_Right: dx = 1;  break;
        case Qt::Key_Up:    dy = -1; break;
        case Qt::Key_Down:  dy = 1;  break;
        default:
            QWidget::keyPressEvent(event);
            return;
    }

    navigateCursor(dx, dy);
    event->accept();
}

void SimpleModeWidget::navigateCursor(int dx, int dy)
{
    int totalSlots = this->renderedRows * this->gridColumns;
    if (totalSlots <= 0)
        return;

    // Map every covered cell to its button's anchor so a spanning button acts
    // as one cell: the cursor lands on it once and steps past its whole span.
    QMap<int, int> cellAnchor;
    for (auto it = this->slotEntries.constBegin(); it != this->slotEntries.constEnd(); ++it)
        for (int r = 0; r < it.value().spanRows; r++)
            for (int c = 0; c < it.value().spanCols; c++)
                cellAnchor[it.key() + r * this->gridColumns + c] = it.key();

    int target;
    if (this->cursorSlot < 0)
    {
        // No cursor yet: the first press lands on the first button (or slot 1).
        target = this->slotEntries.isEmpty() ? 0 : this->slotEntries.firstKey();
    }
    else
    {
        int row = this->cursorSlot / this->gridColumns;
        int col = this->cursorSlot % this->gridColumns;
        if (dy > 0 && this->slotEntries.contains(this->cursorSlot))
            row += this->slotEntries[this->cursorSlot].spanRows - 1; // leave from the span's bottom

        row += dy;
        col += dx;
        if (col < 0 || col >= this->gridColumns || row < 0 || row >= this->renderedRows)
            return; // grid edge — stay put

        target = row * this->gridColumns + col;
        target = cellAnchor.value(target, target); // snap onto a span's anchor
        if (target == this->cursorSlot)
            return;
    }

    this->cursorSlot = target;

    if (this->slotEntries.contains(target))
    {
        // Landing on a button selects it, exactly like clicking it.
        RundownTreeWidget* tree = (this->rundownWidget != nullptr) ? this->rundownWidget->activeTreeWidget() : nullptr;
        if (tree != nullptr)
            tree->treeWidget()->setCurrentItem(this->slotEntries[target].item);

        for (int i = 0; i < this->buttonCommands.count(); i++)
        {
            if (this->buttonCommands[i] == this->slotEntries[target].command)
            {
                this->scrollArea->ensureWidgetVisible(this->cellFrames[i]);
                break;
            }
        }
    }
    else if (this->emptySlotFrames.contains(target))
    {
        this->scrollArea->ensureWidgetVisible(this->emptySlotFrames[target]);
    }

    updateEmptySlotStyles();
}

int SimpleModeWidget::computeCellSize() const
{
    // Square cells sized so the configured columns x rows grid fills the panel
    // in both directions; the smaller dimension wins to preserve the square.
    int cols = qMax(1, this->spinBoxColumns->value());
    int rows = qMax(1, this->spinBoxRows->value());

    int availableW = this->scrollArea->viewport()->width() - GRID_SPACING * (cols - 1) - 2;
    int availableH = this->scrollArea->viewport()->height() - GRID_SPACING * (rows - 1) - 2;

    int size = qMin(availableW / cols, availableH / rows);
    return qBound(MIN_CELL_SIZE, size, MAX_CELL_SIZE);
}

void SimpleModeWidget::invokeDataChanged()
{
    if (this->contentDebounce != nullptr)
        this->contentDebounce->start();
}

void SimpleModeWidget::rundownChanged()
{
    if (this->isVisible())
        refresh();
}

void SimpleModeWidget::rundownItemSelected(const RundownItemSelectedEvent& event)
{
    if (event.getCommand() != this->selectedCommand)
        setMoveMode(false); // changing selection cancels a pending move

    this->selectedCommand = event.getCommand();

    // Keep the arrow-key cursor on the selected button (however it was selected).
    for (auto it = this->slotEntries.constBegin(); it != this->slotEntries.constEnd(); ++it)
    {
        if (it.value().command == this->selectedCommand && this->selectedCommand != nullptr)
        {
            this->cursorSlot = it.key();
            break;
        }
    }

    if (this->isVisible())
    {
        applySelectionStyles();
        updateEmptySlotStyles();
        updateMoveBarState();
    }
}

RundownTreeBaseWidget* SimpleModeWidget::undoTarget() const
{
    RundownTreeWidget* tree = (this->rundownWidget != nullptr) ? this->rundownWidget->activeTreeWidget() : nullptr;
    return (tree != nullptr) ? tree->treeWidget() : nullptr;
}

void SimpleModeWidget::collectFlaggedItems(RundownTreeWidget* tree, QList<QTreeWidgetItem*>& result) const
{
    std::function<void(QTreeWidgetItem*)> walk = [&](QTreeWidgetItem* item) {
        QWidget* widget = tree->treeWidget()->itemWidget(item, 0);
        AbstractRundownWidget* rundownItem = dynamic_cast<AbstractRundownWidget*>(widget);
        if (rundownItem != nullptr && rundownItem->getCommand() != nullptr
            && rundownItem->getCommand()->getShowInSimpleMode())
            result.append(item);

        for (int i = 0; i < item->childCount(); i++)
            walk(item->child(i));
    };

    QTreeWidgetItem* root = tree->treeWidget()->invisibleRootItem();
    for (int i = 0; i < root->childCount(); i++)
        walk(root->child(i));
}

int SimpleModeWidget::computeControlsHeight(AbstractCommand* command, bool showPlayStop) const
{
    // The ▶ / ■ / ⏭ controls are true squares: each one's height equals its
    // share of the cell width, so more controls in the row means smaller squares.
    int count = (showPlayStop ? 2 : 0) + (command->getSimpleModeNextButton() ? 1 : 0)
              + (this->showPreviewControls ? 1 : 0);
    if (count == 0)
        return 0;

    // Floor is deliberately low: it must not bind in normal use, otherwise the
    // controls would end up taller than wide (e.g. 3 controls on a minimum cell).
    int innerWidth = this->cellSize - 2 * CELL_MARGIN - CELL_SPACING * (count - 1);
    return qMax(20, innerWidth / count);
}

// Invoke sub-buttons for one grid key. A template key contributes its own
// labeled invokes; a group key lifts the labeled invokes of every descendant
// template flagged with "Show invokes on group button". Only labeled invokes
// ever appear — an unlabeled invoke has nothing to print on a button.
QList<SimpleModeWidget::InvokeButton> SimpleModeWidget::collectInvokeButtons(RundownTreeWidget* tree, QTreeWidgetItem* item) const
{
    QList<InvokeButton> result;
    if (tree == nullptr || item == nullptr)
        return result;

    auto appendInvokes = [&result](QTreeWidgetItem* target, TemplateCommand* templateCommand) {
        const QStringList& invokes = templateCommand->getInvokes();
        for (int i = 0; i < invokes.count(); i++)
        {
            QString label = templateCommand->getInvokeLabelAt(i).trimmed();
            if (label.isEmpty())
                continue;
            result.append({ target, templateCommand, invokes[i], label });
        }
    };

    QWidget* widget = tree->treeWidget()->itemWidget(item, 0);
    AbstractRundownWidget* rundownItem = dynamic_cast<AbstractRundownWidget*>(widget);
    if (rundownItem == nullptr || rundownItem->getCommand() == nullptr)
        return result;

    if (TemplateCommand* own = dynamic_cast<TemplateCommand*>(rundownItem->getCommand()))
    {
        appendInvokes(item, own);
        return result;
    }

    if (!rundownItem->isGroup())
        return result;

    // Groups may nest one level, so walk descendants rather than direct children.
    std::function<void(QTreeWidgetItem*)> walk = [&](QTreeWidgetItem* parent) {
        for (int i = 0; i < parent->childCount(); i++)
        {
            QTreeWidgetItem* child = parent->child(i);
            AbstractRundownWidget* childItem =
                dynamic_cast<AbstractRundownWidget*>(tree->treeWidget()->itemWidget(child, 0));
            if (childItem != nullptr && childItem->getCommand() != nullptr
                && childItem->getCommand()->getSimpleModeGroupInvokes())
            {
                if (TemplateCommand* childTemplate = dynamic_cast<TemplateCommand*>(childItem->getCommand()))
                    appendInvokes(child, childTemplate);
            }
            walk(child);
        }
    };
    walk(item);

    return result;
}

// Explicit key width in slots, clamped to the grid (1 = normal, 2 = double).
int SimpleModeWidget::computeSpanCols(AbstractCommand* command) const
{
    return qBound(1, command->getSimpleModeWidth(), qMax(1, this->gridColumns));
}

int SimpleModeWidget::computeSpanRows(RundownTreeWidget* tree, QTreeWidgetItem* item, AbstractCommand* command, bool showPlayStop) const
{
    // Mirror buildButtonCell exactly — same widgets, same heights, same spacing —
    // and measure the real text, so a key grows into a second slot only when its
    // content genuinely does not fit rather than on a padded guess.
    // An explicit height wins outright — the operator asked for that size.
    if (command->getSimpleModeHeight() > 0)
        return command->getSimpleModeHeight();

    const int invokeCount = collectInvokeButtons(tree, item).count();

    bool isDropdown = false;
    if (GroupCommand* group = dynamic_cast<GroupCommand*>(command))
        isDropdown = group->getTreatAsDropdown() && item != nullptr && item->childCount() > 0;

    int spanCols = computeSpanCols(command);
    int innerWidth = qMax(1, spanCols * this->cellSize + (spanCols - 1) * GRID_SPACING - 2 * CELL_MARGIN);
    int content = 0;
    int items = 0;   // layout items, including the two centring stretches

    int controlsHeight = computeControlsHeight(command, showPlayStop);
    if (controlsHeight > 0)
    {
        content += controlsHeight;
        items++;
    }

    items++;   // leading stretch

    if (!command->getSimpleModeIcon().isEmpty())
    {
        QFont iconFont = font();
        iconFont.setPixelSize(26);
        content += QFontMetrics(iconFont).height();
        items++;
    }

    // The label wraps, so measure the actual text at the key's inner width.
    int labelSize = command->getSimpleModeLabelSize() > 0 ? command->getSimpleModeLabelSize() : 13;
    QFont labelFont = font();
    labelFont.setPixelSize(labelSize);
    labelFont.setBold(true);
    QString labelText;
    if (tree != nullptr && item != nullptr)
    {
        AbstractRundownWidget* rundownItem =
            dynamic_cast<AbstractRundownWidget*>(tree->treeWidget()->itemWidget(item, 0));
        if (rundownItem != nullptr && rundownItem->getLibraryModel() != nullptr)
            labelText = rundownItem->getLibraryModel()->getLabel().split('/').last();
    }
    content += QFontMetrics(labelFont).boundingRect(
        QRect(0, 0, innerWidth, 0), Qt::AlignCenter | Qt::TextWordWrap, labelText).height();
    items++;

    if (isDropdown)
    {
        content += SUBLABEL_HEIGHT;   // selected-item line, sits under the label
        items++;
    }

    items++;   // trailing stretch

    if (isDropdown)
    {
        content += DROPDOWN_HEIGHT;   // the chooser
        items++;
    }

    content += invokeCount * (INVOKE_ROW_HEIGHT - 2);
    items += invokeCount;

    content += BADGE_HEIGHT;   // channel badge under the control row
    items++;

    content += TALLY_HEIGHT;   // last-fired strip along the top edge
    items++;

    int needed = 2 * CELL_MARGIN + content + CELL_SPACING * qMax(0, items - 1);

    int span = 1;
    while (needed > span * this->cellSize + (span - 1) * GRID_SPACING)
        span++;
    return span;
}

void SimpleModeWidget::assignSlots(RundownTreeWidget* tree, const QList<QTreeWidgetItem*>& items, bool showPlayStop)
{
    this->slotEntries.clear();

    QSet<int> occupiedCells;

    // Keys are rectangles now (explicit double width, auto/explicit height), so
    // placement checks every cell they would cover and never straddles the right
    // edge of the grid.
    auto canPlace = [this, &occupiedCells](int anchor, int rows, int cols) {
        if (anchor < 0)
            return false;
        if (anchor % this->gridColumns + cols > this->gridColumns)
            return false;
        for (int r = 0; r < rows; r++)
            for (int c = 0; c < cols; c++)
                if (occupiedCells.contains(anchor + r * this->gridColumns + c))
                    return false;
        return true;
    };
    auto place = [this, &occupiedCells](int anchor, QTreeWidgetItem* item, AbstractCommand* command, int rows, int cols) {
        for (int r = 0; r < rows; r++)
            for (int c = 0; c < cols; c++)
                occupiedCells.insert(anchor + r * this->gridColumns + c);
        this->slotEntries[anchor] = { item, command, rows, cols };
    };

    // First pass: items with an explicit slot claim it (first claimant wins).
    QList<QTreeWidgetItem*> unplaced;
    for (QTreeWidgetItem* item : items)
    {
        AbstractRundownWidget* rundownItem = dynamic_cast<AbstractRundownWidget*>(tree->treeWidget()->itemWidget(item, 0));
        AbstractCommand* command = rundownItem->getCommand();
        int rows = computeSpanRows(tree, item, command, showPlayStop);
        int cols = computeSpanCols(command);

        int slot = command->getSimpleModeSlot();
        if (slot >= 0 && canPlace(slot, rows, cols))
            place(slot, item, command, rows, cols);
        else
            unplaced.append(item);
    }

    // Second pass: auto items take the lowest anchor where the whole key fits.
    for (QTreeWidgetItem* item : unplaced)
    {
        AbstractRundownWidget* rundownItem = dynamic_cast<AbstractRundownWidget*>(tree->treeWidget()->itemWidget(item, 0));
        AbstractCommand* command = rundownItem->getCommand();
        int rows = computeSpanRows(tree, item, command, showPlayStop);
        int cols = computeSpanCols(command);

        int anchor = 0;
        while (!canPlace(anchor, rows, cols))
            anchor++;
        place(anchor, item, command, rows, cols);
    }
}

// Give every rendered key an explicit slot. Without this, a key with no saved slot
// is auto-placed into the lowest free one — so removing any key makes every later
// key slide up to fill the hole. Writing the position down the first time a key is
// drawn is what keeps the surface still when something is removed from it.
void SimpleModeWidget::materializeSlots()
{
    for (auto it = this->slotEntries.constBegin(); it != this->slotEntries.constEnd(); ++it)
    {
        // Only ever write a position that has never been written. A key that already
        // has one may have been placed somewhere else this pass because a neighbour
        // grew and took its slot — that is a temporary fallback for one render, not a
        // decision to move it. Persisting it would let a window resize permanently
        // rearrange the surface; leaving it alone means the key returns home once
        // there is room again.
        if (it.value().command != nullptr && it.value().command->getSimpleModeSlot() < 0)
            it.value().command->setSimpleModeSlot(it.key());
    }
}

// A slot is a flat index, so it only means the same position while the grid keeps
// the same width. When the column count changes, translate every saved slot through
// its row and column so keys stay where the operator put them.
void SimpleModeWidget::remapSlotsForColumnCount()
{
    QString stored = DatabaseManager::getInstance().getConfigurationByName("SimpleModeSlotColumns").getValue();
    int previousColumns = stored.toInt();

    if (previousColumns == this->gridColumns)
        return;

    if (previousColumns > 0)
    {
        RundownTreeWidget* tree = (this->rundownWidget != nullptr) ? this->rundownWidget->activeTreeWidget() : nullptr;
        if (tree != nullptr)
        {
            QList<QTreeWidgetItem*> items;
            collectFlaggedItems(tree, items);

            for (QTreeWidgetItem* item : items)
            {
                AbstractRundownWidget* rundownItem =
                    dynamic_cast<AbstractRundownWidget*>(tree->treeWidget()->itemWidget(item, 0));
                if (rundownItem == nullptr || rundownItem->getCommand() == nullptr)
                    continue;

                int slot = rundownItem->getCommand()->getSimpleModeSlot();
                if (slot < 0)
                    continue;

                int row = slot / previousColumns;
                int column = slot % previousColumns;
                if (column >= this->gridColumns)
                    column = this->gridColumns - 1;   // narrower grid: pull it inside

                rundownItem->getCommand()->setSimpleModeSlot(row * this->gridColumns + column);
            }
        }
    }

    DatabaseManager::getInstance().updateConfiguration(
        ConfigurationModel(0, "SimpleModeSlotColumns", QString::number(this->gridColumns)));
}

// The rundown just marked an item as its active one for this channel. Mirror that:
// one lit key per channel, matching the rundown's own per-channel behaviour (or a
// single lit key overall when the indicator is configured to be global).
void SimpleModeWidget::rundownItemFired(QTreeWidgetItem* item, int channel)
{
    bool perChannel = DatabaseManager::getInstance()
        .getConfigurationByName("ActiveIndicatorPerChannel").getValue() != "false";

    if (!perChannel)
        this->lastFiredByChannel.clear();

    this->lastFiredByChannel[channel] = item;

    updateTallies();
}

// A key's tally is lit when that key's item is the last thing fired on its channel.
// The strip always occupies its height so nothing moves when it lights up.
void SimpleModeWidget::updateTallies()
{
    for (int i = 0; i < this->cellTallies.count(); i++)
    {
        if (this->cellTallies[i] == nullptr || i >= this->cellItems.count() || i >= this->buttonCommands.count())
            continue;

        int channel = (this->buttonCommands[i] != nullptr) ? this->buttonCommands[i]->getBaseChannel() : 0;
        bool lit = (this->lastFiredByChannel.value(channel, nullptr) == this->cellItems[i]);

        this->cellTallies[i]->setStyleSheet(lit
            ? QString("background-color: %1; border: none; border-radius: 2px;")
                  .arg(RundownWidgetHelper::activeColor(channel).name())
            : QString("background-color: transparent; border: none;"));
    }
}

// Drop every cell and every pointer the grid is holding. Called before a rebuild,
// and on its own when the rundown is torn down while the grid is not visible.
void SimpleModeWidget::clearGrid()
{
    QLayoutItem* child;
    while ((child = this->gridLayout->takeAt(0)) != nullptr)
    {
        if (child->widget())
            child->widget()->deleteLater();
        delete child;
    }
    this->cellFrames.clear();
    this->buttonCommands.clear();
    this->cellTallies.clear();
    this->cellItems.clear();
    this->slotEntries.clear();
    this->emptySlotFrames.clear();
}

// Items were destroyed or rebuilt: the pointers in slotEntries and inside every
// key's handlers are now dangling, so they must go before anything can touch them.
void SimpleModeWidget::rundownStructureChanged()
{
    this->lastFiredByChannel.clear();   // those items no longer exist
    clearGrid();

    if (this->isVisible())
        refresh();
    else
        this->rebuildPending = true;
}

void SimpleModeWidget::refresh()
{
    clearGrid();
    this->rebuildPending = false;

    // A rebuild always leaves move mode (placement itself triggers a rebuild).
    this->moveMode = false;
    this->moveButton->blockSignals(true);
    this->moveButton->setChecked(false);
    this->moveButton->blockSignals(false);
    this->moveHint->setText("Arrow keys move the cursor. Select a button to move or remove it; save the rundown to keep the arrangement.");

    this->gridColumns = qMax(1, this->spinBoxColumns->value());
    this->gridRows = qMax(1, this->spinBoxRows->value());
    this->cellSize = computeCellSize();

    RundownTreeWidget* tree = (this->rundownWidget != nullptr) ? this->rundownWidget->activeTreeWidget() : nullptr;
    if (tree == nullptr)
    {
        QLabel* empty = new QLabel("No rundown open.", this->gridContainer);
        empty->setAlignment(Qt::AlignCenter);
        this->gridLayout->addWidget(empty, 0, 0);
        updateMoveBarState();
        return;
    }

    bool showPlayStop = true;
    QString psValue = DatabaseManager::getInstance().getConfigurationByName("SimpleModeShowPlayStop").getValue();
    if (!psValue.isEmpty())
        showPlayStop = (psValue == "true");

    QString pvValue = DatabaseManager::getInstance().getConfigurationByName("SimpleModeShowPreview").getValue();
    this->showPreviewControls = pvValue.isEmpty() || pvValue == "true";

    remapSlotsForColumnCount();

    QList<QTreeWidgetItem*> items;
    collectFlaggedItems(tree, items);
    assignSlots(tree, items, showPlayStop);

    // Freeze the arrangement so removing a key never moves the others.
    materializeSlots();

    // Cells covered by spanning buttons must not render empty placeholders.
    QSet<int> coveredCells;
    int highestCell = -1;
    for (auto it = this->slotEntries.constBegin(); it != this->slotEntries.constEnd(); ++it)
    {
        for (int r = 0; r < it.value().spanRows; r++)
        {
            for (int c = 0; c < it.value().spanCols; c++)
            {
                int cell = it.key() + r * this->gridColumns + c;
                coveredCells.insert(cell);
                highestCell = qMax(highestCell, cell);
            }
        }
    }

    // At least the configured page is always visible; content beyond it adds
    // scrollable rows (plus one spare row to move buttons into).
    int rows = this->gridRows;
    if (highestCell >= 0)
        rows = qMax(rows, (highestCell / this->gridColumns) + 1 + SPARE_ROWS);
    int totalSlots = rows * this->gridColumns;

    this->renderedRows = rows;
    if (this->cursorSlot >= totalSlots)
        this->cursorSlot = -1;

    for (int slot = 0; slot < totalSlots; slot++)
    {
        if (this->slotEntries.contains(slot))
        {
            const SlotEntry& entry = this->slotEntries[slot];
            QWidget* cell = buildButtonCell(tree, entry.item, showPlayStop, this->cellSize,
                                            entry.spanRows, entry.spanCols);
            this->gridLayout->addWidget(cell, slot / this->gridColumns, slot % this->gridColumns,
                                        entry.spanRows, entry.spanCols);
        }
        else if (!coveredCells.contains(slot))
        {
            this->gridLayout->addWidget(buildEmptySlot(slot, this->cellSize),
                                        slot / this->gridColumns, slot % this->gridColumns);
        }
    }

    if (this->slotEntries.isEmpty())
    {
        QLabel* hint = new QLabel(
            "No items flagged for Simple Mode yet \xe2\x80\x94 switch to the normal interface,\n"
            "select an item and enable \"Show as button\" in the inspector's Output section.",
            this->gridContainer);
        hint->setAlignment(Qt::AlignCenter);
        hint->setStyleSheet("color: rgba(150, 150, 150, 200); font-size: 12px;");
        this->gridLayout->addWidget(hint, rows, 0, 1, this->gridColumns);
    }

    applySelectionStyles();
    updateTallies();
    updateMoveBarState();
}

QWidget* SimpleModeWidget::buildEmptySlot(int slotIndex, int cellSize)
{
    SimpleCellFrame* frame = new SimpleCellFrame(this->gridContainer);
    frame->setFixedSize(cellSize, cellSize);
    styleEmptySlot(frame, false, slotIndex == this->cursorSlot);
    frame->onPress = [this, slotIndex]() {
        this->setFocus();
        if (this->moveMode)
        {
            placeSelectedAt(slotIndex);
        }
        else
        {
            this->cursorSlot = slotIndex;
            updateEmptySlotStyles();
        }
    };

    QVBoxLayout* layout = new QVBoxLayout(frame);
    QLabel* number = new QLabel(QString::number(slotIndex + 1), frame);
    number->setAlignment(Qt::AlignCenter);
    number->setAttribute(Qt::WA_TransparentForMouseEvents);
    number->setStyleSheet("border: none; background: transparent; color: rgba(90, 90, 90, 140); font-size: 16px;");
    layout->addWidget(number);

    this->emptySlotFrames[slotIndex] = frame;
    return frame;
}

void SimpleModeWidget::styleEmptySlot(QFrame* frame, bool targetable, bool cursor) const
{
    QString border;
    QString background;
    if (targetable)
    {
        border = cursor ? "2px solid rgba(150, 200, 250, 240)" : "2px dashed rgba(100, 160, 220, 220)";
        background = "rgba(45, 60, 80, 140)";
    }
    else if (cursor)
    {
        // Arrow-key cursor resting on an empty slot.
        border = "2px solid rgba(200, 200, 200, 220)";
        background = "rgba(55, 55, 55, 160)";
    }
    else
    {
        border = "1px dashed rgba(80, 80, 80, 160)";
        background = "rgba(35, 35, 35, 120)";
    }
    frame->setStyleSheet(QString(
        "QFrame { border: %1; border-radius: 6px; background-color: %2; }").arg(border, background));
}

void SimpleModeWidget::updateEmptySlotStyles()
{
    for (auto it = this->emptySlotFrames.constBegin(); it != this->emptySlotFrames.constEnd(); ++it)
        styleEmptySlot(it.value(), this->moveMode, it.key() == this->cursorSlot);
}

void SimpleModeWidget::setMoveMode(bool active)
{
    if (active && this->selectedCommand == nullptr)
        active = false;

    this->moveMode = active;

    this->moveButton->blockSignals(true);
    this->moveButton->setChecked(active);
    this->moveButton->blockSignals(false);

    this->moveHint->setText(active
        ? "Click a free slot to place the button \xe2\x80\x94 press Move again to cancel."
        : "Arrow keys move the cursor. Select a button to move or remove it; save the rundown to keep the arrangement.");

    updateEmptySlotStyles();
}

void SimpleModeWidget::placeSelectedAt(int targetSlot)
{
    if (!this->moveMode || this->selectedCommand.isNull())
        return;

    int currentSlot = -1;
    int movedRows = 1;
    int movedCols = 1;
    for (auto it = this->slotEntries.constBegin(); it != this->slotEntries.constEnd(); ++it)
    {
        if (it.value().command == this->selectedCommand)
        {
            currentSlot = it.key();
            movedRows = it.value().spanRows;
            movedCols = it.value().spanCols;
            break;
        }
    }
    if (currentSlot < 0)
        return;

    // A key must land whole: inside the grid horizontally and on free cells only
    // (its own current cells count as free).
    if (targetSlot % this->gridColumns + movedCols > this->gridColumns)
        return;

    QSet<int> movedCells;
    for (int r = 0; r < movedRows; r++)
        for (int c = 0; c < movedCols; c++)
            movedCells.insert(targetSlot + r * this->gridColumns + c);

    for (auto it = this->slotEntries.constBegin(); it != this->slotEntries.constEnd(); ++it)
    {
        if (it.value().command == this->selectedCommand)
            continue;

        for (int r = 0; r < it.value().spanRows; r++)
            for (int c = 0; c < it.value().spanCols; c++)
                if (movedCells.contains(it.key() + r * this->gridColumns + c))
                    return; // occupied — placement blocked
    }

    {
        // Inside the scope: materialising writes the positions of every other button,
        // and undoing the move has to put those back the way they were too.
        UndoScope scope(undoTarget(), "Move Simple Mode Button");

        materializeSlots();
        this->selectedCommand->setSimpleModeSlot(targetSlot);
    }

    refresh(); // also leaves move mode
}

QWidget* SimpleModeWidget::buildButtonCell(RundownTreeWidget* tree, QTreeWidgetItem* item, bool showPlayStop, int cellSize, int spanRows, int spanCols)
{
    QWidget* widget = tree->treeWidget()->itemWidget(item, 0);
    AbstractRundownWidget* rundownItem = dynamic_cast<AbstractRundownWidget*>(widget);
    AbstractCommand* command = rundownItem->getCommand();

    // The whole cell is one bordered "key": pressing its body selects the item,
    // and the selection border wraps everything including the sub-buttons.
    SimpleCellFrame* cell = new SimpleCellFrame(this->gridContainer);
    cell->setFixedSize(cellSize * spanCols + GRID_SPACING * (spanCols - 1),
                       cellSize * spanRows + GRID_SPACING * (spanRows - 1));
    cell->setProperty("itemColor", rundownItem->getColor());
    cell->onPress = [this, tree, item]() {
        this->setFocus();
        tree->treeWidget()->setCurrentItem(item);
    };
    cell->onContext = [this, tree, item, command](const QPoint& globalPos) {
        tree->treeWidget()->setCurrentItem(item); // the menu acts on this button

        QMenu menu(this);

        QMenu* sizeMenu = menu.addMenu("Label Size");
        const QList<QPair<QString, int>> sizes = {
            {"Small", 11}, {"Normal (default)", 0}, {"Large", 16}, {"Extra Large", 20}, {"Huge", 26}
        };
        int currentSize = command->getSimpleModeLabelSize();
        for (const auto& sizeOption : sizes)
        {
            QAction* action = sizeMenu->addAction(sizeOption.first);
            action->setCheckable(true);
            action->setChecked(currentSize == sizeOption.second);
            int value = sizeOption.second;
            QObject::connect(action, &QAction::triggered, this, [this, command, value]() {
                UndoScope scope(undoTarget(), "Change Label Size");
                command->setSimpleModeLabelSize(value);
                refresh();
            });
        }

        // Explicit key size. Height "Auto" grows only as far as the content needs.
        QMenu* buttonSizeMenu = menu.addMenu("Button Size");

        QMenu* widthMenu = buttonSizeMenu->addMenu("Width");
        const QList<QPair<QString, int>> widths = { {"Single", 1}, {"Double", 2} };
        for (const auto& widthOption : widths)
        {
            QAction* action = widthMenu->addAction(widthOption.first);
            action->setCheckable(true);
            action->setChecked(qMax(1, command->getSimpleModeWidth()) == widthOption.second);
            int value = widthOption.second;
            QObject::connect(action, &QAction::triggered, this, [this, command, value]() {
                UndoScope scope(undoTarget(), "Change Button Size");
                command->setSimpleModeWidth(value);
                refresh();
            });
        }

        QMenu* heightMenu = buttonSizeMenu->addMenu("Height");
        const QList<QPair<QString, int>> heights = { {"Auto (fit content)", 0}, {"Single", 1}, {"Double", 2} };
        for (const auto& heightOption : heights)
        {
            QAction* action = heightMenu->addAction(heightOption.first);
            action->setCheckable(true);
            action->setChecked(command->getSimpleModeHeight() == heightOption.second);
            int value = heightOption.second;
            QObject::connect(action, &QAction::triggered, this, [this, command, value]() {
                UndoScope scope(undoTarget(), "Change Button Size");
                command->setSimpleModeHeight(value);
                refresh();
            });
        }

        menu.addAction("Set Icon...", this, [this, command]() {
            SimpleIconDialog dialog(command->getSimpleModeIcon(), this);
            if (dialog.exec() == QDialog::Accepted)
            {
                {
                    UndoScope scope(undoTarget(), "Set Button Icon");
                    command->setSimpleModeIcon(dialog.getIcon());
                }

                refresh();
            }
        });

        menu.addSeparator();
        menu.addAction("Remove from Grid", this, [this, command]() {
            {
                UndoScope scope(undoTarget(), "Remove From Simple Mode");
                command->setShowInSimpleMode(false);
            }

            refresh();
        });

        menu.exec(globalPos);
    };

    QVBoxLayout* cellLayout = new QVBoxLayout(cell);
    cellLayout->setContentsMargins(CELL_MARGIN, CELL_MARGIN, CELL_MARGIN, CELL_MARGIN);
    cellLayout->setSpacing(CELL_SPACING);

    // Tally strip along the top edge — lit while this key is the last thing fired on
    // its channel. It always occupies its height, so lighting up never moves anything.
    QWidget* tally = new QWidget(cell);
    tally->setFixedHeight(TALLY_HEIGHT);
    tally->setAttribute(Qt::WA_TransparentForMouseEvents);
    tally->setStyleSheet("background-color: transparent; border: none;");
    cellLayout->addWidget(tally, 0);

    bool showNext = command->getSimpleModeNextButton();
    if (showPlayStop || showNext || this->showPreviewControls)
    {
        QHBoxLayout* controls = new QHBoxLayout();
        controls->setSpacing(CELL_SPACING);

        int controlsHeight = computeControlsHeight(command, showPlayStop);
        int glyphSize = qBound(16, controlsHeight * 2 / 5, 40);

        auto makeControl = [cell, controlsHeight, glyphSize](const QString& glyph, const QString& colorCss, const QString& toolTip) {
            QPushButton* button = new QPushButton(glyph, cell);
            button->setFixedHeight(controlsHeight);
            button->setFocusPolicy(Qt::NoFocus);
            button->setToolTip(toolTip);
            button->setStyleSheet(QString(
                "QPushButton { %1 color: white; border-radius: 4px; font-size: %2px; font-weight: bold;"
                " padding: 0px; text-align: center; }"
                "QPushButton:hover { border: 2px solid rgba(220, 220, 220, 220); }")
                .arg(colorCss).arg(glyphSize));
            return button;
        };

        if (showPlayStop)
        {
            QPushButton* play = makeControl(QString::fromUtf8("\xe2\x96\xb6"),
                "background-color: rgba(40, 100, 40, 230); border: 1px solid rgba(70, 130, 70, 200);", "Play");
            QObject::connect(play, &QPushButton::clicked, this, [this, item]() {
                this->setFocus();
                EventManager::getInstance().fireExecuteRundownItemEvent(
                    ExecuteRundownItemEvent(Playout::PlayoutType::Play, item));
            });
            controls->addWidget(play, 1);

            QPushButton* stop = makeControl(QString::fromUtf8("\xe2\x96\xa0"),
                "background-color: rgba(120, 40, 40, 230); border: 1px solid rgba(150, 70, 70, 200);", "Stop");
            QObject::connect(stop, &QPushButton::clicked, this, [this, item]() {
                this->setFocus();
                EventManager::getInstance().fireExecuteRundownItemEvent(
                    ExecuteRundownItemEvent(Playout::PlayoutType::Stop, item));
            });
            controls->addWidget(stop, 1);
        }

        if (this->showPreviewControls)
        {
            // Plays on the device's preview channel (PlayoutType::Preview), which
            // groups pass down to their children just like a normal play.
            QPushButton* preview = makeControl("PVW",
                "background-color: rgba(125, 95, 35, 230); border: 1px solid rgba(160, 130, 70, 200);", "Preview");
            preview->setStyleSheet(preview->styleSheet()
                + QString("QPushButton { font-size: %1px; }").arg(qBound(9, controlsHeight / 4, 18)));
            QObject::connect(preview, &QPushButton::clicked, this, [this, item]() {
                this->setFocus();
                EventManager::getInstance().fireExecuteRundownItemEvent(
                    ExecuteRundownItemEvent(Playout::PlayoutType::Preview, item));
            });
            controls->addWidget(preview, 1);
        }

        if (showNext)
        {
            QPushButton* next = makeControl(QString::fromUtf8("\xe2\x8f\xad"),
                "background-color: rgba(45, 75, 110, 230); border: 1px solid rgba(75, 105, 145, 200);", "Next");
            QObject::connect(next, &QPushButton::clicked, this, [this, item]() {
                this->setFocus();
                EventManager::getInstance().fireExecuteRundownItemEvent(
                    ExecuteRundownItemEvent(Playout::PlayoutType::Next, item));
            });
            controls->addWidget(next, 1);
        }

        cellLayout->addLayout(controls);
    }

    // Channel badge directly under the controls, using the same colour logic as the
    // rundown's own badges so a channel reads the same everywhere in the client.
    int badgeChannel = command->getBaseChannel();
    QLabel* badge = new QLabel(RundownWidgetHelper::badgeText(badgeChannel, command->getVideolayer()), cell);
    badge->setAlignment(Qt::AlignCenter);
    badge->setAttribute(Qt::WA_TransparentForMouseEvents);
    badge->setFixedHeight(BADGE_HEIGHT);
    badge->setStyleSheet(QString(
        "background-color: %1; color: white; border: none; border-radius: 2px;"
        " font-size: 10px; font-weight: bold;")
        .arg(RundownWidgetHelper::channelColor(badgeChannel).name()));
    cellLayout->addWidget(badge, 0);

    // Icon, label and (for dropdown keys) the selected-item line form one tight
    // cluster: stretches above and below centre it as a unit, so the icon sits
    // directly above the label and the sub-label directly below it.
    cellLayout->addStretch(1);

    // Optional icon (a text glyph, XML-portable) above the label.
    QString icon = command->getSimpleModeIcon();
    if (!icon.isEmpty())
    {
        QLabel* iconLabel = new QLabel(icon, cell);
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        iconLabel->setStyleSheet("border: none; background: transparent; font-size: 26px;");
        cellLayout->addWidget(iconLabel, 0);
    }

    // Label fills the key face: word-wrapped, centered, transparent to clicks.
    int labelSize = command->getSimpleModeLabelSize() > 0 ? command->getSimpleModeLabelSize() : 13;
    QString labelText = rundownItem->getLibraryModel()->getLabel().split('/').last();
    QLabel* label = new QLabel(labelText, cell);
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignCenter);
    label->setAttribute(Qt::WA_TransparentForMouseEvents);
    label->setStyleSheet(QString("border: none; background: transparent; color: white; font-size: %1px; font-weight: bold;").arg(labelSize));
    cellLayout->addWidget(label, 0);

    this->cellFrames.append(cell);
    this->buttonCommands.append(command);
    this->cellTallies.append(tally);
    this->cellItems.append(item);

    bool clusterClosed = false;   // set once a trailing stretch has been added

    // Dropdown group: the key becomes a chooser over the group's children. The
    // selection is what Play / F2 fires (RundownTreeWidget redirects it), and it
    // is remembered in the rundown XML.
    if (GroupCommand* dropdownGroup = dynamic_cast<GroupCommand*>(command))
    {
        if (dropdownGroup->getTreatAsDropdown() && item->childCount() > 0)
        {
            QComboBox* chooser = new QComboBox(cell);
            chooser->setFixedHeight(DROPDOWN_HEIGHT);
            chooser->setToolTip("Choose which item in this group Play/F2 fires");
            chooser->setStyleSheet(
                "QComboBox { background-color: rgba(35, 35, 45, 235); color: white; border-radius: 3px;"
                " border: 1px solid rgba(90, 90, 120, 220); font-size: 11px; padding: 0px 4px; }");

            for (int i = 0; i < item->childCount(); i++)
            {
                AbstractRundownWidget* child =
                    dynamic_cast<AbstractRundownWidget*>(tree->treeWidget()->itemWidget(item->child(i), 0));
                QString childLabel = (child != nullptr && child->getLibraryModel() != nullptr)
                    ? child->getLibraryModel()->getLabel().split('/').last() : QString("Item %1").arg(i + 1);
                chooser->addItem(childLabel);
            }

            int selectedIndex = qBound(0, dropdownGroup->getDropdownIndex(), item->childCount() - 1);
            chooser->setCurrentIndex(selectedIndex);

            // Wheel only after clicking, so scrolling the grid can't change what is armed.
            WheelGuard::apply(chooser);
            chooser->setFocusPolicy(Qt::ClickFocus);

            // Sub-label: the armed choice, readable at a glance from the key face
            // without opening the chooser. Elided so a long name can't stretch
            // the key or wrap onto a second line.
            QLabel* selectionLabel = new QLabel(cell);
            selectionLabel->setAlignment(Qt::AlignCenter);
            selectionLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
            selectionLabel->setFixedHeight(SUBLABEL_HEIGHT);
            QFont selectionFont = selectionLabel->font();
            selectionFont.setPixelSize(11);
            selectionLabel->setFont(selectionFont);
            selectionLabel->setStyleSheet(
                "border: none; background: transparent; color: rgba(190, 200, 225, 230);");

            int selectionWidth = cellSize * spanCols + GRID_SPACING * (spanCols - 1) - 2 * CELL_MARGIN - 4;
            auto showSelection = [selectionLabel, selectionFont, selectionWidth](const QString& text) {
                selectionLabel->setText(QFontMetrics(selectionFont).elidedText(text, Qt::ElideRight, selectionWidth));
                selectionLabel->setToolTip(text);
            };
            showSelection(chooser->itemText(selectedIndex));

            cellLayout->addWidget(selectionLabel, 0);
            cellLayout->addStretch(1);   // closes the cluster; chooser stays at the bottom

            QObject::connect(chooser, QOverload<int>::of(&QComboBox::activated), this,
                             [this, tree, item, dropdownGroup, chooser, showSelection](int index) {
                dropdownGroup->setDropdownIndex(index);
                showSelection(chooser->itemText(index));    // keep the sub-label in step
                tree->treeWidget()->setCurrentItem(item);   // hotkeys must target this key
            });

            cellLayout->addWidget(chooser, 0);
            clusterClosed = true;
        }

        QObject::connect(dropdownGroup, SIGNAL(treatAsDropdownChanged(bool)),
                         this, SLOT(invokeDataChanged()), Qt::UniqueConnection);
    }

    if (!clusterClosed)
        cellLayout->addStretch(1);

    // Keep the key in step with the inspector even when it currently has no
    // labeled invokes (adding the first label must re-render this key).
    if (TemplateCommand* ownTemplate = dynamic_cast<TemplateCommand*>(command))
    {
        QObject::connect(ownTemplate, SIGNAL(invokesChanged(const QStringList&)),
                         this, SLOT(invokeDataChanged()), Qt::UniqueConnection);
        QObject::connect(ownTemplate, SIGNAL(invokeLabelsChanged(const QStringList&)),
                         this, SLOT(invokeDataChanged()), Qt::UniqueConnection);
    }

    // Invoke sub-buttons: a template key shows its own labeled invokes; a group
    // key shows those its children were flagged to publish. Each fires on the
    // item that owns it, so a group invoke hits the right child template.
    const QList<InvokeButton> invokeButtons = collectInvokeButtons(tree, item);
    for (const InvokeButton& invoke : invokeButtons)
    {
        QObject::connect(invoke.command, SIGNAL(invokesChanged(const QStringList&)),
                         this, SLOT(invokeDataChanged()), Qt::UniqueConnection);
        QObject::connect(invoke.command, SIGNAL(invokeLabelsChanged(const QStringList&)),
                         this, SLOT(invokeDataChanged()), Qt::UniqueConnection);

        QPushButton* invokeButton = new QPushButton(invoke.label, cell);
        invokeButton->setFixedHeight(INVOKE_ROW_HEIGHT - 2);
        invokeButton->setFocusPolicy(Qt::NoFocus);
        invokeButton->setStyleSheet(
            "QPushButton { background-color: rgba(55, 55, 75, 230); color: white; border-radius: 3px;"
            " border: 1px solid rgba(85, 85, 115, 200); font-size: 11px; }"
            "QPushButton:hover { background-color: rgba(80, 80, 110, 230); }");

        TemplateCommand* invokeCommand = invoke.command;
        QTreeWidgetItem* invokeItem = invoke.item;
        QString invokeFunction = invoke.function;
        QObject::connect(invokeButton, &QPushButton::clicked, this, [this, invokeCommand, invokeFunction, invokeItem]() {
            this->setFocus();
            invokeCommand->setPendingInvokeOverride(invokeFunction);
            EventManager::getInstance().fireExecuteRundownItemEvent(
                ExecuteRundownItemEvent(Playout::PlayoutType::Invoke, invokeItem));
        });
        cellLayout->addWidget(invokeButton, 0);
    }

    return cell;
}

void SimpleModeWidget::styleCellFrame(QFrame* frame, const QString& color, bool selected) const
{
    QString background = (!color.isEmpty() && color != Color::DEFAULT_TRANSPARENT_COLOR)
        ? color : "rgba(50, 50, 50, 235)";
    QString border = selected ? "3px solid #50c878" : "1px solid rgba(95, 95, 95, 220)";
    frame->setStyleSheet(QString(
        "SimpleCellFrame, QFrame { background-color: %1; border: %2; border-radius: 8px; }")
        .arg(background, border));
}

void SimpleModeWidget::applySelectionStyles()
{
    for (int i = 0; i < this->cellFrames.count(); i++)
    {
        QString color = this->cellFrames[i]->property("itemColor").toString();
        styleCellFrame(this->cellFrames[i], color, this->buttonCommands[i] == this->selectedCommand);
    }
}

void SimpleModeWidget::updateMoveBarState()
{
    bool hasSelection = false;
    for (auto it = this->slotEntries.constBegin(); it != this->slotEntries.constEnd(); ++it)
    {
        if (it.value().command == this->selectedCommand && this->selectedCommand != nullptr)
        {
            hasSelection = true;
            break;
        }
    }

    this->moveButton->setEnabled(hasSelection);
    this->removeButton->setEnabled(hasSelection);
    if (!hasSelection)
        setMoveMode(false);
}

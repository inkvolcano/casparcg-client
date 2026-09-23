#include "InspectorUndo.h"

#include "InspectorUndoRules.h"
#include "EventManager.h"
#include "Rundown/AbstractRundownWidget.h"
#include "Rundown/RundownTreeBaseWidget.h"
#include "Models/LibraryModel.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QEvent>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QAbstractSlider>
#include <QtWidgets/QAbstractSpinBox>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QTextEdit>

InspectorUndo::InspectorUndo(const QList<QWidget*>& roots, std::function<RundownTreeBaseWidget*()> activeTree, QObject* parent)
    : QObject(parent),
      activeTree(activeTree)
{
    for (QWidget* root : roots)
        this->roots.append(root);

    qApp->installEventFilter(this);

    // Another item in the Inspector is the end of work on the last one's field.
    QObject::connect(&EventManager::getInstance(), SIGNAL(rundownItemSelected(const RundownItemSelectedEvent&)), this, SLOT(commit()));
    QObject::connect(&EventManager::getInstance(), SIGNAL(libraryItemSelected(const LibraryItemSelectedEvent&)), this, SLOT(commit()));
    QObject::connect(&EventManager::getInstance(), SIGNAL(emptyRundown(const EmptyRundownEvent&)), this, SLOT(commit()));

    // A rundown operation - move, delete, paste - is recorded after the edit that
    // came before it, not folded into it. And an undo that rebuilds the rundown
    // makes the capture in hand stale, so a gesture on that rundown is dropped.
    RundownTreeBaseWidget::s_beforeUndoStep = [this]() { commit(); };
    RundownTreeBaseWidget::s_beforeRestore = [this](RundownTreeBaseWidget* restored) { discard(restored); };
}

InspectorUndo::~InspectorUndo()
{
    RundownTreeBaseWidget::s_beforeUndoStep = nullptr;
    RundownTreeBaseWidget::s_beforeRestore = nullptr;
}

// The field a press or key belongs to: the nearest input widget above the target,
// inside one of the Inspector panels. The Inspector's own section list is not a
// field - clicking a section header edits nothing.
QWidget* InspectorUndo::fieldOf(QObject* target) const
{
    QWidget* widget = qobject_cast<QWidget*>(target);
    QWidget* candidate = nullptr;

    while (widget != nullptr)
    {
        for (const QPointer<QWidget>& root : this->roots)
        {
            if (root == widget)
                return candidate;
        }

        if (candidate == nullptr)
        {
            const bool isInput = qobject_cast<QAbstractButton*>(widget) || qobject_cast<QAbstractSpinBox*>(widget)
                              || qobject_cast<QComboBox*>(widget) || qobject_cast<QLineEdit*>(widget)
                              || qobject_cast<QTextEdit*>(widget) || qobject_cast<QPlainTextEdit*>(widget)
                              || qobject_cast<QAbstractSlider*>(widget)
                              || (qobject_cast<QAbstractItemView*>(widget) && widget->objectName() != "treeWidgetInspector");
            if (isInput)
                candidate = widget;
        }

        if (widget->isWindow())
            return nullptr;

        widget = widget->parentWidget();
    }

    return nullptr;
}

void InspectorUndo::begin(QWidget* field)
{
    RundownTreeBaseWidget* tree = this->activeTree ? this->activeTree() : nullptr;
    if (tree == nullptr || tree->isUndoRestoring())
        return;

    QElapsedTimer clock;
    clock.start();

    this->tree = tree;
    this->field = field;
    this->before = tree->serializeTree();
    this->pending = true;

    const qint64 took = clock.elapsed();
    if (took >= 50)
        qDebug("Capturing the rundown for Inspector undo took %lld ms", took);
}

void InspectorUndo::commit()
{
    if (!this->pending)
        return;

    this->pending = false;
    this->field = nullptr;

    RundownTreeBaseWidget* tree = this->tree;
    this->tree = nullptr;
    const QString before = this->before;
    this->before.clear();

    if (tree == nullptr)
        return;

    QString description = "Edit";
    if (QTreeWidgetItem* current = tree->currentItem())
    {
        if (AbstractRundownWidget* widget = dynamic_cast<AbstractRundownWidget*>(tree->itemWidget(current, 0)))
        {
            const QString label = widget->getLibraryModel()->getLabel().isEmpty()
                ? widget->getLibraryModel()->getName() : widget->getLibraryModel()->getLabel();
            if (!label.isEmpty())
                description = QString("Edit %1").arg(label);
        }
    }

    tree->pushSnapshotStep(description, before);
}

void InspectorUndo::discard(RundownTreeBaseWidget* restored)
{
    if (this->pending && this->tree == restored)
    {
        this->pending = false;
        this->field = nullptr;
        this->tree = nullptr;
        this->before.clear();
    }
}

bool InspectorUndo::eventFilter(QObject* watched, QEvent* event)
{
    const QEvent::Type type = event->type();
    if (type != QEvent::MouseButtonPress && type != QEvent::KeyPress && type != QEvent::ShortcutOverride)
        return QObject::eventFilter(watched, event);

    // Undo and redo act on what has been recorded, so the gesture in progress is
    // recorded first - otherwise Ctrl+Z would undo the step before it.
    if (type == QEvent::ShortcutOverride)
    {
        QKeyEvent* key = static_cast<QKeyEvent*>(event);
        if (key->matches(QKeySequence::Undo) || key->matches(QKeySequence::Redo)
            || (key->key() == Qt::Key_Y && key->modifiers() == Qt::ControlModifier))
            commit();

        return QObject::eventFilter(watched, event);
    }

    // A menu or a list's popup open: its presses belong to the field that opened it.
    if (type == QEvent::MouseButtonPress && QApplication::activePopupWidget() != nullptr)
        return QObject::eventFilter(watched, event);

    if (type == QEvent::KeyPress)
    {
        QKeyEvent* key = static_cast<QKeyEvent*>(event);
        if (!InspectorUndoRules::keyCanEdit(key->key()))
            return QObject::eventFilter(watched, event);
    }

    // A press nobody accepts is delivered again to each parent in turn; every
    // delivery resolves to the same field, so the repeats change nothing.
    QWidget* field = fieldOf(watched);
    if (field == nullptr)
        return QObject::eventFilter(watched, event);

    if (this->pending && this->field != field)
        commit();

    if (!this->pending)
        begin(field);

    return QObject::eventFilter(watched, event);
}

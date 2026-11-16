#include "FindReplaceDialog.h"

#include "EventManager.h"
#include "Global.h"
#include "Rundown/AbstractRundownWidget.h"
#include "Rundown/RundownTreeBaseWidget.h"
#include "Rundown/RundownTreeWidget.h"
#include "Rundown/RundownUndoCommands.h"
#include "Rundown/RundownWidget.h"

#include "Commands/AbstractCommand.h"
#include "Commands/CustomCommand.h"
#include "Commands/HtmlCommand.h"
#include "Commands/ShellCommand.h"
#include "Commands/TemplateCommand.h"
#include "Events/Inspector/LabelChangedEvent.h"
#include "Events/Rundown/RundownItemSelectedEvent.h"
#include "Models/KeyValueModel.h"
#include "Models/LibraryModel.h"

#include <QtCore/QFileInfo>
#include <QtCore/QMetaObject>
#include <QtCore/QSet>

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>

using namespace FindReplaceRules;

namespace
{
    const int HIT_ROLE = Qt::UserRole + 1;

    QString rundownName(RundownTreeWidget* rundown)
    {
        return rundown->getActiveRundown() == Rundown::DEFAULT_NAME ? QString("Untitled rundown")
                                                                   : QFileInfo(rundown->getActiveRundown()).completeBaseName();
    }

    // A custom command item's AMCP lines, by the name the Inspector gives them.
    struct AmcpLine
    {
        const char* name;
        const QString& (CustomCommand::*get)() const;
        void (CustomCommand::*set)(const QString&);
    };

    const QList<AmcpLine>& amcpLines()
    {
        static const QList<AmcpLine> lines = {
            {"Stop", &CustomCommand::getStopCommand, &CustomCommand::setStopCommand},
            {"Play", &CustomCommand::getPlayCommand, &CustomCommand::setPlayCommand},
            {"Load", &CustomCommand::getLoadCommand, &CustomCommand::setLoadCommand},
            {"Pause", &CustomCommand::getPauseCommand, &CustomCommand::setPauseCommand},
            {"Next", &CustomCommand::getNextCommand, &CustomCommand::setNextCommand},
            {"Update", &CustomCommand::getUpdateCommand, &CustomCommand::setUpdateCommand},
            {"Invoke", &CustomCommand::getInvokeCommand, &CustomCommand::setInvokeCommand},
            {"Preview", &CustomCommand::getPreviewCommand, &CustomCommand::setPreviewCommand},
            {"Clear", &CustomCommand::getClearCommand, &CustomCommand::setClearCommand},
            {"Clear Videolayer", &CustomCommand::getClearVideolayerCommand, &CustomCommand::setClearVideolayerCommand},
            {"Clear Channel", &CustomCommand::getClearChannelCommand, &CustomCommand::setClearChannelCommand},
        };
        return lines;
    }
}

void FindReplaceDialog::open(RundownWidget* area, QWidget* parent)
{
    FindReplaceDialog* dialog = new FindReplaceDialog(area, parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

FindReplaceDialog::FindReplaceDialog(RundownWidget* area, QWidget* parent)
    : QDialog(parent), area(area)
{
    setWindowTitle("Find and Replace");
    resize(780, 520);

    QVBoxLayout* layout = new QVBoxLayout(this);

    QFormLayout* form = new QFormLayout();
    this->editFind = new QLineEdit(this);
    this->editFind->setClearButtonEnabled(true);
    form->addRow("Find", this->editFind);
    this->editReplace = new QLineEdit(this);
    this->editReplace->setClearButtonEnabled(true);
    this->editReplace->setPlaceholderText("Leave empty to remove what is found");
    form->addRow("Replace with", this->editReplace);
    layout->addLayout(form);

    QHBoxLayout* optionsRow = new QHBoxLayout();
    this->checkMatchCase = new QCheckBox("Match case", this);
    this->checkWholeWords = new QCheckBox("Whole words", this);
    this->radioThis = new QRadioButton("This rundown", this);
    this->radioAll = new QRadioButton("All open rundowns", this);
    this->radioThis->setChecked(true);
    optionsRow->addWidget(this->checkMatchCase);
    optionsRow->addWidget(this->checkWholeWords);
    optionsRow->addStretch(1);
    optionsRow->addWidget(this->radioThis);
    optionsRow->addWidget(this->radioAll);
    layout->addLayout(optionsRow);

    QGroupBox* lookIn = new QGroupBox("Look in", this);
    QGridLayout* kindsGrid = new QGridLayout(lookIn);
    const QList<QPair<int, bool>> kinds = {
        {Label, true}, {FieldValue, true}, {Note, true}, {Url, false}, {AmcpCommand, false}, {ShellLine, false}
    };
    const QStringList texts = {"Item labels", "Template field values", "Operator notes",
                               "HTML page addresses", "Custom AMCP commands", "Shell command lines"};
    for (int i = 0; i < kinds.count(); i++)
    {
        QCheckBox* box = new QCheckBox(texts.at(i), lookIn);
        box->setChecked(kinds.at(i).second);
        kindsGrid->addWidget(box, i / 3, i % 3);
        this->kindBoxes.append(qMakePair(box, kinds.at(i).first));
    }
    layout->addWidget(lookIn);

    this->list = new QTreeWidget(this);
    this->list->setRootIsDecorated(false);
    this->list->setColumnCount(4);
    this->list->setHeaderLabels(QStringList() << "Rundown" << "Item" << "Where" << "Text");
    this->list->header()->setStretchLastSection(true);
    this->list->setColumnWidth(0, 140);
    this->list->setColumnWidth(1, 180);
    this->list->setColumnWidth(2, 140);
    layout->addWidget(this->list, 1);

    this->summaryLabel = new QLabel(this);
    this->summaryLabel->setWordWrap(true);
    layout->addWidget(this->summaryLabel);

    QDialogButtonBox* buttons = new QDialogButtonBox(this);
    this->buttonShow = buttons->addButton("Show", QDialogButtonBox::ActionRole);
    this->buttonFind = buttons->addButton("Find All", QDialogButtonBox::ActionRole);
    this->buttonReplace = buttons->addButton("Replace All", QDialogButtonBox::ActionRole);
    buttons->addButton(QDialogButtonBox::Close);
    layout->addWidget(buttons);

    this->buttonFind->setDefault(true);
    this->buttonShow->setEnabled(false);
    this->buttonReplace->setEnabled(false);

    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    QObject::connect(this->buttonFind, &QPushButton::clicked, this, [this]() { findAll(); });
    QObject::connect(this->buttonReplace, &QPushButton::clicked, this, [this]() { replaceAll(); });
    QObject::connect(this->buttonShow, &QPushButton::clicked, this, [this]() { showSelected(); });
    QObject::connect(this->list, &QTreeWidget::itemDoubleClicked, this, [this]() { showSelected(); });
    QObject::connect(this->list, &QTreeWidget::currentItemChanged, this, [this]() {
        this->buttonShow->setEnabled(this->list->currentItem() != nullptr);
    });

    // A list made with other settings is not the list Replace All would change.
    auto stale = [this]() {
        if (this->hits.isEmpty() && this->list->topLevelItemCount() == 0)
            return;
        this->hits.clear();
        this->list->clear();
        this->buttonReplace->setEnabled(false);
        this->summaryLabel->setText("Find All again to see what would change.");
    };
    QObject::connect(this->editFind, &QLineEdit::textChanged, this, stale);
    QObject::connect(this->checkMatchCase, &QCheckBox::toggled, this, stale);
    QObject::connect(this->checkWholeWords, &QCheckBox::toggled, this, stale);
    QObject::connect(this->radioAll, &QRadioButton::toggled, this, stale);
    for (const QPair<QCheckBox*, int>& box : this->kindBoxes)
        QObject::connect(box.first, &QCheckBox::toggled, this, stale);

    this->editFind->setFocus();
}

FindReplaceRules::Options FindReplaceDialog::options() const
{
    Options o;
    o.matchCase = this->checkMatchCase->isChecked();
    o.wholeWords = this->checkWholeWords->isChecked();
    return o;
}

int FindReplaceDialog::kinds() const
{
    int k = 0;
    for (const QPair<QCheckBox*, int>& box : this->kindBoxes)
    {
        if (box.first->isChecked())
            k |= box.second;
    }
    return k;
}

QList<RundownTreeWidget*> FindReplaceDialog::rundowns() const
{
    QList<RundownTreeWidget*> found;
    if (this->area.isNull())
        return found;

    RundownTreeWidget* onScreen = this->area->activeTreeWidget();
    if (this->radioThis->isChecked())
    {
        if (onScreen != nullptr)
            found.append(onScreen);
        return found;
    }

    found = this->area->findChildren<RundownTreeWidget*>();
    if (onScreen != nullptr && found.removeOne(onScreen))
        found.prepend(onScreen);
    return found;
}

void FindReplaceDialog::findAll()
{
    this->hits.clear();
    this->list->clear();

    const QString find = this->editFind->text();
    if (find.isEmpty() || kinds() == 0)
    {
        this->summaryLabel->setText(find.isEmpty() ? "Type what to find." : "Tick at least one kind of text to look in.");
        this->buttonReplace->setEnabled(false);
        return;
    }

    const QList<RundownTreeWidget*> trees = rundowns();
    for (RundownTreeWidget* rundown : trees)
    {
        if (rundown->treeWidget() != nullptr)
            collect(rundown, rundown->treeWidget()->invisibleRootItem());
    }

    int total = 0;
    QSet<QTreeWidgetItem*> items;
    for (int i = 0; i < this->hits.count(); i++)
    {
        const Hit& hit = this->hits.at(i);
        total += hit.matches;
        items.insert(hit.item);

        AbstractRundownWidget* widget = dynamic_cast<AbstractRundownWidget*>(hit.rundown->treeWidget()->itemWidget(hit.item, 0));
        const QString label = (widget != nullptr && widget->getLibraryModel() != nullptr) ? widget->getLibraryModel()->getLabel() : QString();

        QTreeWidgetItem* row = new QTreeWidgetItem(this->list);
        row->setText(0, rundownName(hit.rundown));
        row->setText(1, label);
        row->setText(2, hit.where.isEmpty() ? kindText(hit.kind) : QString("%1: %2").arg(kindText(hit.kind), hit.where));
        row->setText(3, excerpt(hit.text, find, options()));
        row->setToolTip(3, hit.text.toHtmlEscaped().replace("\n", "<br/>"));
        row->setData(0, HIT_ROLE, i);
    }

    if (this->hits.isEmpty())
        this->summaryLabel->setText(QString("\"%1\" is not in %2.").arg(find, trees.count() == 1 ? "this rundown" : "the open rundowns"));
    else
        this->summaryLabel->setText(QString("%1 %2 in %3 %4. Replace All changes them all; Edit > Undo in each rundown takes it back.")
                                    .arg(total).arg(total == 1 ? "match" : "matches")
                                    .arg(items.count()).arg(items.count() == 1 ? "item" : "items"));
    this->buttonReplace->setEnabled(!this->hits.isEmpty());
}

void FindReplaceDialog::collect(RundownTreeWidget* rundown, QTreeWidgetItem* parent)
{
    RundownTreeBaseWidget* tree = rundown->treeWidget();
    for (int i = 0; i < parent->childCount(); i++)
    {
        QTreeWidgetItem* item = parent->child(i);
        AbstractRundownWidget* widget = dynamic_cast<AbstractRundownWidget*>(tree->itemWidget(item, 0));
        if (widget != nullptr)
            collectItem(rundown, item, widget);
        if (item->childCount() > 0)
            collect(rundown, item);
    }
}

void FindReplaceDialog::collectItem(RundownTreeWidget* rundown, QTreeWidgetItem* item, AbstractRundownWidget* widget)
{
    const QString find = this->editFind->text();
    const Options o = options();
    const int k = kinds();
    AbstractCommand* command = widget->getCommand();

    auto add = [&](Kind kind, const QString& where, const QString& text) {
        const int n = count(text, find, o);
        if (n == 0)
            return;
        Hit hit;
        hit.rundown = rundown;
        hit.item = item;
        hit.kind = kind;
        hit.where = where;
        hit.text = text;
        hit.matches = n;
        this->hits.append(hit);
    };

    if ((k & Label) && widget->getLibraryModel() != nullptr)
        add(Label, QString(), widget->getLibraryModel()->getLabel());

    if (command == nullptr)
        return;

    if (k & FieldValue)
    {
        if (TemplateCommand* templateCommand = dynamic_cast<TemplateCommand*>(command))
        {
            for (const KeyValueModel& field : templateCommand->getTemplateDataModels())
                add(FieldValue, field.getKey(), field.getValue());
        }
    }
    if (k & Note)
        add(Note, QString(), command->getNote());
    if (k & Url)
    {
        if (HtmlCommand* html = dynamic_cast<HtmlCommand*>(command))
            add(Url, QString(), html->getUrl());
    }
    if (k & AmcpCommand)
    {
        if (CustomCommand* custom = dynamic_cast<CustomCommand*>(command))
        {
            for (const AmcpLine& line : amcpLines())
                add(AmcpCommand, line.name, (custom->*line.get)());
        }
    }
    if (k & ShellLine)
    {
        if (ShellCommand* shell = dynamic_cast<ShellCommand*>(command))
            add(ShellLine, QString(), shell->getCommandLine());
    }
}

void FindReplaceDialog::replaceAll()
{
    if (this->hits.isEmpty())
        return;

    // Each rundown once, each item in it once, in the order they were listed.
    QList<RundownTreeWidget*> order;
    QHash<RundownTreeWidget*, QList<QTreeWidgetItem*>> itemsOf;
    for (const Hit& hit : this->hits)
    {
        if (hit.rundown.isNull())
            continue;
        if (!itemsOf.contains(hit.rundown.data()))
            order.append(hit.rundown.data());
        QList<QTreeWidgetItem*>& items = itemsOf[hit.rundown.data()];
        if (!items.contains(hit.item))
            items.append(hit.item);
    }

    int replaced = 0;
    int changedItems = 0;
    QStringList locked;
    for (RundownTreeWidget* rundown : order)
    {
        if (rundown->isLocked())
        {
            locked.append(rundownName(rundown));
            continue;
        }

        RundownTreeBaseWidget* tree = rundown->treeWidget();
        int inThis = 0;
        {
            UndoScope scope(tree, QString("Replace \"%1\"").arg(this->editFind->text()));
            for (QTreeWidgetItem* item : itemsOf.value(rundown))
            {
                AbstractRundownWidget* widget = dynamic_cast<AbstractRundownWidget*>(tree->itemWidget(item, 0));
                if (widget == nullptr)
                    continue;
                const int n = replaceIn(rundown, item, widget);
                if (n > 0)
                {
                    inThis += n;
                    changedItems++;
                }
            }
        }
        replaced += inThis;

        // The Inspector shows the item it was showing again, with its new text.
        QTreeWidgetItem* current = tree->currentItem();
        if (inThis > 0 && current != nullptr && rundown == this->area->activeTreeWidget() && itemsOf.value(rundown).contains(current))
        {
            AbstractRundownWidget* widget = dynamic_cast<AbstractRundownWidget*>(tree->itemWidget(current, 0));
            QWidget* parentWidget = current->parent() != nullptr ? tree->itemWidget(current->parent(), 0) : nullptr;
            if (widget != nullptr)
                EventManager::getInstance().fireRundownItemSelectedEvent(
                    RundownItemSelectedEvent(widget->getCommand(), widget->getLibraryModel(), tree->itemWidget(current, 0), parentWidget));
        }
    }

    qInfo("Find and replace: \"%s\" -> \"%s\", %d replaced in %d items", qPrintable(this->editFind->text()),
          qPrintable(this->editReplace->text()), replaced, changedItems);

    this->hits.clear();
    this->list->clear();
    this->buttonReplace->setEnabled(false);

    QString summary = QString("Replaced %1 in %2 %3.").arg(replaced).arg(changedItems).arg(changedItems == 1 ? "item" : "items");
    if (!locked.isEmpty())
        summary += QString(" Left alone because locked: %1.").arg(locked.join(", "));
    this->summaryLabel->setText(summary);
}

int FindReplaceDialog::replaceIn(RundownTreeWidget* rundown, QTreeWidgetItem* item, AbstractRundownWidget* widget)
{
    Q_UNUSED(rundown);

    const QString find = this->editFind->text();
    const QString with = this->editReplace->text();
    const Options o = options();
    const int k = kinds();
    AbstractCommand* command = widget->getCommand();
    int n = 0;

    if ((k & Label) && widget->getLibraryModel() != nullptr)
    {
        const QString label = widget->getLibraryModel()->getLabel();
        const int here = count(label, find, o);
        if (here > 0)
        {
            // Through the item's own handler, which acts only while the item is
            // selected: selected for that one call, then as it was.
            const QString newLabel = replaced(label, find, with, o);
            QWidget* host = dynamic_cast<QWidget*>(widget);
            // A row's flag follows the tree's selection (RundownTreeWidget), so
            // the tree item says what to put back.
            const bool wasSelected = item->isSelected();
            widget->setSelected(true);
            const bool handled = host != nullptr && QMetaObject::invokeMethod(host, "labelChanged", Qt::DirectConnection,
                                                                            Q_ARG(LabelChangedEvent, LabelChangedEvent(newLabel)));
            widget->setSelected(wasSelected);
            if (!handled)
            {
                widget->getLibraryModel()->setLabel(newLabel);
                QLabel* shown = host != nullptr ? host->findChild<QLabel*>("labelLabel") : nullptr;
                if (shown != nullptr)
                    shown->setText(shown->text() == label ? newLabel : newLabel.split('/').last());
            }
            n += here;
        }
    }

    if (command == nullptr)
        return n;

    if (k & FieldValue)
    {
        if (TemplateCommand* templateCommand = dynamic_cast<TemplateCommand*>(command))
        {
            QList<KeyValueModel> fields = templateCommand->getTemplateDataModels();
            int here = 0;
            for (int i = 0; i < fields.count(); i++)
            {
                const KeyValueModel& field = fields.at(i);
                const int c = count(field.getValue(), find, o);
                if (c == 0)
                    continue;
                here += c;
                fields[i] = KeyValueModel(field.getKey(), replaced(field.getValue(), find, with, o), field.getMode(), field.getCycleValues());
            }
            if (here > 0)
            {
                templateCommand->setTemplateDataModels(fields);
                n += here;
            }
        }
    }
    if (k & Note)
    {
        const int here = count(command->getNote(), find, o);
        if (here > 0)
        {
            command->setNote(replaced(command->getNote(), find, with, o));
            n += here;
        }
    }
    if (k & Url)
    {
        if (HtmlCommand* html = dynamic_cast<HtmlCommand*>(command))
        {
            const int here = count(html->getUrl(), find, o);
            if (here > 0)
            {
                html->setUrl(replaced(html->getUrl(), find, with, o));
                n += here;
            }
        }
    }
    if (k & AmcpCommand)
    {
        if (CustomCommand* custom = dynamic_cast<CustomCommand*>(command))
        {
            for (const AmcpLine& line : amcpLines())
            {
                const QString text = (custom->*line.get)();
                const int here = count(text, find, o);
                if (here > 0)
                {
                    (custom->*line.set)(replaced(text, find, with, o));
                    n += here;
                }
            }
        }
    }
    if (k & ShellLine)
    {
        if (ShellCommand* shell = dynamic_cast<ShellCommand*>(command))
        {
            const int here = count(shell->getCommandLine(), find, o);
            if (here > 0)
            {
                shell->setCommandLine(replaced(shell->getCommandLine(), find, with, o));
                n += here;
            }
        }
    }
    return n;
}

void FindReplaceDialog::showSelected()
{
    QTreeWidgetItem* row = this->list->currentItem();
    if (row == nullptr)
        return;

    const int index = row->data(0, HIT_ROLE).toInt();
    if (index < 0 || index >= this->hits.count() || this->hits.at(index).rundown.isNull())
        return;

    const Hit& hit = this->hits.at(index);
    RundownTreeWidget* rundown = hit.rundown.data();

    // Bring its tab to the front, wherever it is.
    for (QWidget* w = rundown; w != nullptr; w = w->parentWidget())
    {
        QWidget* page = w;
        QWidget* stack = page->parentWidget();
        QTabWidget* tabs = stack != nullptr ? qobject_cast<QTabWidget*>(stack->parentWidget()) : nullptr;
        if (tabs != nullptr && tabs->indexOf(page) >= 0)
        {
            tabs->setCurrentWidget(page);
            break;
        }
    }

    RundownTreeBaseWidget* tree = rundown->treeWidget();
    tree->setCurrentItem(hit.item);
    tree->scrollToItem(hit.item);
}

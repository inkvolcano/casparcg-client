#include "InspectorTemplateWidget.h"

#include <algorithm>

#include "../SheetDataResolver.h"
#include "../SheetsProjectRegistry.h"
#include "../WheelGuard.h"
#include "DialogPosition.h"
#include "KeyValueDialog.h"
#include "NumericValueDelegate.h"

#include "Global.h"

#include "OgrafManifest.h"

#include "DatabaseManager.h"
#include "EventManager.h"
#include "Playout.h"
#include "Events/StatusbarEvent.h"
#include "Events/Rundown/ExecutePlayoutCommandEvent.h"
#include "Models/DeviceModel.h"
#include "Models/KeyValueModel.h"

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QDateTime>
#include <QtCore/QFile>
#include <QtCore/QRegularExpression>
#include <QtCore/QTextStream>

#include <QtGui/QClipboard>
#include <QtGui/QCursor>
#include <QtGui/QGuiApplication>
#include <QtGui/QKeyEvent>
#include <QtGui/QResizeEvent>
#include <QtGui/QScreen>

#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>

InspectorTemplateWidget::InspectorTemplateWidget(QWidget* parent)
    : QWidget(parent),
      model(NULL), command(NULL), lock(false)
{
    setupUi(this);

    this->labelFlashlayerField->hide();
    this->spinBoxFlashlayer->hide();

    // Add Auto-play checkbox to the grid layout (below newline behavior, above template data).
    if (QGridLayout* grid = qobject_cast<QGridLayout*>(this->layout()))
    {
        QLabel* labelAutoPlay = new QLabel("Auto-play", this);
        labelAutoPlay->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);

        this->checkBoxAutoPlay = new QCheckBox(this);
        this->checkBoxAutoPlay->setLayoutDirection(Qt::RightToLeft);

        QHBoxLayout* apLayout = new QHBoxLayout();
        apLayout->addWidget(this->checkBoxAutoPlay);
        apLayout->addSpacerItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

        // Find the row of verticalLayoutData and insert before it by shifting rows.
        // Simpler: append at row 99 (effectively last).  Qt allows sparse rows.
        int newRow = 99;
        grid->addWidget(labelAutoPlay, newRow, 0);
        grid->addLayout(apLayout, newRow, 1);

        QObject::connect(this->checkBoxAutoPlay, SIGNAL(stateChanged(int)), this, SLOT(autoPlayChanged(int)));

        QLabel* labelAutoLoop = new QLabel("Auto-Loop", this);
        labelAutoLoop->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);

        this->checkBoxAutoLoop = new QCheckBox(this);
        this->checkBoxAutoLoop->setLayoutDirection(Qt::RightToLeft);

        this->spinBoxAutoLoopDelay = new QSpinBox(this);
        this->spinBoxAutoLoopDelay->setMinimum(1);
        this->spinBoxAutoLoopDelay->setMaximum(3600);
        this->spinBoxAutoLoopDelay->setSuffix(tr(" sec"));
        this->spinBoxAutoLoopDelay->setValue(Template::DEFAULT_AUTO_LOOP_DELAY);

        QHBoxLayout* alLayout = new QHBoxLayout();
        alLayout->addWidget(this->checkBoxAutoLoop);
        alLayout->addWidget(this->spinBoxAutoLoopDelay, 1);

        grid->addWidget(labelAutoLoop, newRow + 1, 0);
        grid->addLayout(alLayout, newRow + 1, 1);

        QObject::connect(this->checkBoxAutoLoop, SIGNAL(stateChanged(int)), this, SLOT(autoLoopChanged(int)));
        QObject::connect(this->spinBoxAutoLoopDelay, SIGNAL(valueChanged(int)), this, SLOT(autoLoopDelayChanged(int)));


        QObject::connect(&SheetDataResolver::getInstance(), &SheetDataResolver::rowsReady,
                         this, &InspectorTemplateWidget::sheetRowsReady);
        QObject::connect(&SheetDataResolver::getInstance(), &SheetDataResolver::rowsFailed,
                         this, &InspectorTemplateWidget::sheetRowsFailed);
    }

    // Update button in the Import Fields row — same header position as the
    // Simple Inspector, so both inspectors share the same look.
    for (QHBoxLayout* rowLayout : this->findChildren<QHBoxLayout*>())
    {
        if (rowLayout->indexOf(this->toolButtonLoadDebugData) < 0)
            continue;

        QPushButton* updateButton = new QPushButton("Update", this);
        updateButton->setFixedHeight(20);
        updateButton->setFocusPolicy(Qt::NoFocus);
        updateButton->setToolTip("Send the template data below to the on-air template");
        updateButton->setStyleSheet(
            "QPushButton { background-color: rgba(50, 80, 120, 220); color: white; border-radius: 3px;"
            " font-size: 10px; font-weight: bold; border: 1px solid rgba(80, 110, 150, 200); padding: 2px 10px; }"
            "QPushButton:hover { background-color: rgba(70, 100, 145, 220); }");
        QObject::connect(updateButton, &QPushButton::clicked, this, [this]() {
            if (this->command == nullptr)
                return;
            updateTemplateDataModels();
            EventManager::getInstance().fireExecutePlayoutCommandEvent(
                ExecutePlayoutCommandEvent(Playout::PlayoutType::Update));
        });
        rowLayout->insertWidget(0, updateButton);
        break;
    }

    this->comboBoxNewlineBehavior->addItem("Ignore");
    this->comboBoxNewlineBehavior->addItem("innerText");
    this->comboBoxNewlineBehavior->addItem("innerHTML");

    // Numeric value delegate for the tree (shows up/down arrows per-row based on mode).
    this->numericDelegate = new NumericValueDelegate(this);
    this->treeWidgetTemplateData->setItemDelegateForColumn(1, this->numericDelegate);
    this->treeWidgetTemplateData->setItemDelegateForColumn(2, this->numericDelegate);
    QObject::connect(this->treeWidgetTemplateData->model(), &QAbstractItemModel::dataChanged,
                     this, [this]() {
        if (this->command && !this->lock)
            updateTemplateDataModels();
    });

    this->treeWidgetTemplateData->setColumnWidth(1, 44);

    // Add a narrow 3rd column for up/down arrow buttons.
    this->treeWidgetTemplateData->setColumnCount(3);
    this->treeWidgetTemplateData->headerItem()->setText(2, "");
    this->treeWidgetTemplateData->header()->resizeSection(2, 20);
    this->treeWidgetTemplateData->header()->setSectionResizeMode(2, QHeaderView::Fixed);
    this->fieldCounter = this->treeWidgetTemplateData->invisibleRootItem()->childCount();

    QObject::connect(&EventManager::getInstance(), SIGNAL(showAddTemplateDataDialog(const ShowAddTemplateDataDialogEvent&)), this, SLOT(showAddTemplateDataDialog(const ShowAddTemplateDataDialogEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(addTemplateData(const AddTemplateDataEvent&)), this, SLOT(addTemplateData(const AddTemplateDataEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(rundownItemSelected(const RundownItemSelectedEvent&)), this, SLOT(rundownItemSelected(const RundownItemSelectedEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(repositoryRundown(const RepositoryRundownEvent&)), this, SLOT(repositoryRundown(const RepositoryRundownEvent&)));

    this->treeWidgetTemplateData->installEventFilter(this);

    buildExpectedBox();

    // The option rows (stored data, uppercase, trigger on next, send as JSON, newline
    // behaviour, auto-play, auto-loop) are set once per template and then left alone,
    // so they go into a panel of their own that the inspector mounts as a separate
    // "Template Settings" section, collapsed by default. The controls stay owned and
    // wired here; only where they are laid out changes. Items are taken from the
    // highest index down so the indices left behind stay valid while this runs.
    if (this->gridLayout != NULL)
    {
        this->settingsPanel = new QWidget();
        QGridLayout* settingsGrid = new QGridLayout(this->settingsPanel);
        settingsGrid->setContentsMargins(this->gridLayout->contentsMargins());
        settingsGrid->setHorizontalSpacing(this->gridLayout->horizontalSpacing());
        settingsGrid->setVerticalSpacing(this->gridLayout->verticalSpacing());

        QList<QLayoutItem*> moved;
        QList<int> rows, cols, rowSpans, colSpans;
        for (int i = this->gridLayout->count() - 1; i >= 0; i--)
        {
            int row = 0, col = 0, rowSpan = 1, colSpan = 1;
            this->gridLayout->getItemPosition(i, &row, &col, &rowSpan, &colSpan);
            bool option = (row >= 1 && row <= 5) || row == 99 || row == 100;
            if (!option)
                continue;

            moved.append(this->gridLayout->takeAt(i));
            rows.append(row); cols.append(col); rowSpans.append(rowSpan); colSpans.append(colSpan);
        }

        // Rows keep their order but close up: 1..5, 99, 100 become 0..6.
        QList<int> distinct = rows;
        std::sort(distinct.begin(), distinct.end());
        distinct.erase(std::unique(distinct.begin(), distinct.end()), distinct.end());

        for (int i = 0; i < moved.count(); i++)
        {
            QLayoutItem* item = moved.at(i);
            int row = distinct.indexOf(rows.at(i));
            if (item->layout() != NULL)
            {
                settingsGrid->addLayout(item->layout(), row, cols.at(i), rowSpans.at(i), colSpans.at(i));
            }
            else if (item->widget() != NULL)
            {
                settingsGrid->addWidget(item->widget(), row, cols.at(i), rowSpans.at(i), colSpans.at(i));
                delete item;   // addWidget made a fresh QWidgetItem for it
            }
            else
            {
                settingsGrid->addItem(item, row, cols.at(i), rowSpans.at(i), colSpans.at(i));
            }
        }

        settingsGrid->setColumnStretch(1, 1);
    }

    // Any change in the number of rows re-sizes the table, wherever it came from:
    // typed in, imported, resolved from a sheet, or undone.
    // A key added through the + dialog arrives as a finished row, so the model reports
    // rowsInserted and never dataChanged. The result box has to hear about that too,
    // or a freshly added "row" key is not answered until the item is reselected.
    auto tableChanged = [this]() {
        resizeDataTreeToContents();
        renderExpectedRow();
        renderExpectedFreshness();
    };
    QObject::connect(this->treeWidgetTemplateData->model(), &QAbstractItemModel::rowsInserted, this, tableChanged);
    QObject::connect(this->treeWidgetTemplateData->model(), &QAbstractItemModel::rowsRemoved, this, tableChanged);
    QObject::connect(this->treeWidgetTemplateData->model(), &QAbstractItemModel::modelReset, this, tableChanged);

    // And the widget-level signal, which an inline edit committed through the value
    // delegate always raises even where the model signal is coalesced.
    QObject::connect(this->treeWidgetTemplateData, &QTreeWidget::itemChanged,
                     this, [this](QTreeWidgetItem*, int) { renderExpectedRow(); renderExpectedFreshness(); });

    // The key lives in the table above, so a change there is a change of question.
    QObject::connect(this->treeWidgetTemplateData->model(), &QAbstractItemModel::dataChanged,
                     this, [this]() { renderExpectedRow(); renderExpectedFreshness(); });

    resizeDataTreeToContents();
}

// Height enough for what is there plus one empty row, so there is always somewhere
// obvious to add the next key without the table claiming space it is not using.
void InspectorTemplateWidget::resizeDataTreeToContents()
{
    QTreeWidget* tree = this->treeWidgetTemplateData;
    if (tree == NULL)
        return;

    int fields = tree->invisibleRootItem()->childCount();

    int rowHeight = (fields > 0) ? tree->sizeHintForRow(0) : 0;
    if (rowHeight <= 0)
        rowHeight = tree->fontMetrics().height() + 6;

    // Past a certain point a taller table stops helping and the inspector becomes
    // one long scroll, so it stops growing and scrolls within itself instead.
    const int MAXIMUM_VISIBLE_ROWS = 20;
    int rows = qMin(fields + 1, MAXIMUM_VISIBLE_ROWS);

    int header = tree->header()->isVisible() ? tree->header()->sizeHint().height() : 0;

    tree->setFixedHeight(header + (rows * rowHeight) + (2 * tree->frameWidth()));

    emit contentChanged();
}

bool InspectorTemplateWidget::eventFilter(QObject* target, QEvent* event)
{
    if (event->type() == QEvent::KeyPress)
    {
        QKeyEvent* keyEvent = dynamic_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Delete)
            return removeRow();
        else if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) && keyEvent->modifiers() == Qt::ShiftModifier)
            return editRow();
        else if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
            return addRow();
        else if (keyEvent->key() == Qt::Key_D && keyEvent->modifiers() == Qt::ControlModifier)
            return duplicateSelectedItem();
        else if (keyEvent->key() == Qt::Key_C && keyEvent->modifiers() == Qt::ControlModifier)
            return copySelectedItem();
        else if (keyEvent->key() == Qt::Key_V && keyEvent->modifiers() == Qt::ControlModifier)
            return pasteSelectedItem();
    }

    return QObject::eventFilter(target, event);
}

void InspectorTemplateWidget::repositoryRundown(const RepositoryRundownEvent& event)
{
    this->lock = event.getRepositoryRundown();
}

void InspectorTemplateWidget::showAddTemplateDataDialog(const ShowAddTemplateDataDialogEvent& event)
{
    Q_UNUSED(event);

    if (this->lock)
        return;

    int index = this->treeWidgetTemplateData->invisibleRootItem()->childCount() - 1;
    this->treeWidgetTemplateData->setCurrentItem(this->treeWidgetTemplateData->invisibleRootItem()->child(index));

    KeyValueDialog* dialog = new KeyValueDialog(this);
    dialog->setTitle("New Template Data");
    dialog->setKey(QString("f%1").arg(this->fieldCounter));
    if (dialog->exec() == QDialog::Accepted)
    {
        QTreeWidgetItem* treeItem = new QTreeWidgetItem();
        treeItem->setText(0, dialog->getKey());
        treeItem->setText(1, dialog->getValue());
        treeItem->setData(0, Qt::UserRole, dialog->getMode());
        treeItem->setData(0, Qt::UserRole + 1, dialog->getCycleValues());

        this->treeWidgetTemplateData->invisibleRootItem()->insertChild(this->treeWidgetTemplateData->currentIndex().row() + 1, treeItem);
        this->treeWidgetTemplateData->setCurrentItem(treeItem);

        this->fieldCounter++;
    }
}

void InspectorTemplateWidget::addTemplateData(const AddTemplateDataEvent& event)
{
    if (!this->model || this->model->getType() != Rundown::TEMPLATE)
        return;

    this->treeWidgetTemplateData->clear();
    this->fieldCounter = 0;

    QTreeWidgetItem* treeItem = new QTreeWidgetItem();
    treeItem->setText(0, QString("f%1").arg(this->fieldCounter));
    treeItem->setText(1, event.getValue());

    this->treeWidgetTemplateData->invisibleRootItem()->addChild(treeItem);
    this->treeWidgetTemplateData->setCurrentItem(treeItem);

    this->checkBoxUseStoredData->setChecked(event.getStoredData());

    this->fieldCounter++;
}

void InspectorTemplateWidget::rundownItemSelected(const RundownItemSelectedEvent& event)
{
    this->command = nullptr;
    this->model = event.getLibraryModel();

    blockAllSignals(true);

    if (dynamic_cast<TemplateCommand*>(event.getCommand()))
    {
        this->command = dynamic_cast<TemplateCommand*>(event.getCommand());

        this->spinBoxFlashlayer->setValue(this->command->getFlashlayer());

        this->checkBoxUseStoredData->setChecked(this->command->getUseStoredData());
        this->checkBoxUseUppercaseData->setChecked(this->command->getUseUppercaseData());
        this->checkBoxTriggerOnNext->setChecked(this->command->getTriggerOnNext());
        if (this->checkBoxAutoPlay != nullptr)
            this->checkBoxAutoPlay->setChecked(this->command->getAutoPlay());
        if (this->checkBoxAutoLoop != nullptr)
            this->checkBoxAutoLoop->setChecked(this->command->getAutoLoop());
        if (this->spinBoxAutoLoopDelay != nullptr)
            this->spinBoxAutoLoopDelay->setValue(this->command->getAutoLoopDelay());
        this->checkBoxSendAsJson->setChecked(this->command->getSendAsJson());
        this->comboBoxNewlineBehavior->setCurrentIndex(this->command->getNewlineBehavior());

        for (int i = this->treeWidgetTemplateData->invisibleRootItem()->childCount() - 1; i >= 0; i--)
            delete this->treeWidgetTemplateData->invisibleRootItem()->child(i);

        this->fieldCounter = 0;
        foreach (KeyValueModel model, this->command->getTemplateDataModels())
        {
            QTreeWidgetItem* treeItem = new QTreeWidgetItem();
            treeItem->setText(0, model.getKey());
            treeItem->setText(1, model.getValue());
            treeItem->setData(0, Qt::UserRole, model.getMode());
            treeItem->setData(0, Qt::UserRole + 1, model.getCycleValues());

            this->treeWidgetTemplateData->invisibleRootItem()->addChild(treeItem);

            this->fieldCounter++;
        }

        resizeDataTreeToContents();
    }

    blockAllSignals(false);

    refreshExpectedBinding();
}

void InspectorTemplateWidget::blockAllSignals(bool block)
{
    this->spinBoxFlashlayer->blockSignals(block);
    this->checkBoxUseStoredData->blockSignals(block);
    this->checkBoxUseUppercaseData->blockSignals(block);
    this->checkBoxTriggerOnNext->blockSignals(block);
    if (this->checkBoxAutoPlay != nullptr)
        this->checkBoxAutoPlay->blockSignals(block);
    if (this->checkBoxAutoLoop != nullptr)
        this->checkBoxAutoLoop->blockSignals(block);
    if (this->spinBoxAutoLoopDelay != nullptr)
        this->spinBoxAutoLoopDelay->blockSignals(block);
    this->checkBoxSendAsJson->blockSignals(block);
    this->comboBoxNewlineBehavior->blockSignals(block);
    this->treeWidgetTemplateData->blockSignals(block);
}

// Rows arrive here for the expected-result box, which is the only thing that reads
// the sheet from this inspector.
void InspectorTemplateWidget::sheetRowsReady(const QString& requestId, const QList<SheetRow>& rows,
                                             const SheetRowsOrigin& origin)
{
    if (requestId != this->expectedRequestId)
        return;

    this->expectedRows = rows;
    this->expectedOrigin = origin;

    renderExpectedRow();
    renderExpectedFreshness();
}

void InspectorTemplateWidget::sheetRowsFailed(const QString& requestId, const QString& reason)
{
    if (requestId != this->expectedRequestId || this->expectedStatus == NULL)
        return;

    this->expectedStatus->setText(reason);
}

void InspectorTemplateWidget::updateTemplateDataModels()
{
    // The tree can outlive the selected command (item deleted, rundown reloaded,
    // selection moved to a non-template item) — never touch a gone command.
    if (this->command.isNull())
        return;

    QList<KeyValueModel> models;
    for (int i = 0; i < this->treeWidgetTemplateData->invisibleRootItem()->childCount(); i++)
    {
        QTreeWidgetItem* child = this->treeWidgetTemplateData->invisibleRootItem()->child(i);
        models.push_back(KeyValueModel(child->text(0), child->text(1),
                                       child->data(0, Qt::UserRole).toInt(),
                                       child->data(0, Qt::UserRole + 1).toString()));
    }

    this->command->setTemplateDataModels(models);
}

bool InspectorTemplateWidget::addRow()
{
    KeyValueDialog* dialog = new KeyValueDialog(this);
    DialogPosition::moveNearCursor(dialog);
    dialog->setTitle("New Template Data");
    dialog->setKey(QString("f%1").arg(this->fieldCounter));
    if (dialog->exec() == QDialog::Accepted)
    {
        QTreeWidgetItem* treeItem = new QTreeWidgetItem();
        treeItem->setText(0, dialog->getKey());
        treeItem->setText(1, dialog->getValue());
        treeItem->setData(0, Qt::UserRole, dialog->getMode());
        treeItem->setData(0, Qt::UserRole + 1, dialog->getCycleValues());

        this->treeWidgetTemplateData->invisibleRootItem()->insertChild(this->treeWidgetTemplateData->currentIndex().row() + 1, treeItem);
        this->treeWidgetTemplateData->setCurrentItem(treeItem);

        this->fieldCounter++;
    }

    return true;
}

bool InspectorTemplateWidget::editRow()
{
    if (this->treeWidgetTemplateData->currentItem() == NULL)
        return true;

    KeyValueDialog* dialog = new KeyValueDialog(this);
    DialogPosition::moveNearCursor(dialog);
    dialog->setTitle("Edit Template Data");
    dialog->setKey(this->treeWidgetTemplateData->currentItem()->text(0));
    dialog->setValue(this->treeWidgetTemplateData->currentItem()->text(1));
    dialog->setMode(this->treeWidgetTemplateData->currentItem()->data(0, Qt::UserRole).toInt());
    dialog->setCycleValues(this->treeWidgetTemplateData->currentItem()->data(0, Qt::UserRole + 1).toString());
    if (dialog->exec() == QDialog::Accepted)
    {
        this->treeWidgetTemplateData->currentItem()->setText(0, dialog->getKey());
        this->treeWidgetTemplateData->currentItem()->setText(1, dialog->getValue());
        this->treeWidgetTemplateData->currentItem()->setData(0, Qt::UserRole, dialog->getMode());
        this->treeWidgetTemplateData->currentItem()->setData(0, Qt::UserRole + 1, dialog->getCycleValues());

        updateTemplateDataModels();
    }

    return true;
}

bool InspectorTemplateWidget::removeRow()
{
    if (this->treeWidgetTemplateData->currentItem() == NULL)
        return true;

    delete this->treeWidgetTemplateData->currentItem();
    updateTemplateDataModels();

    if (this->treeWidgetTemplateData->invisibleRootItem()->childCount() == 0)
        this->fieldCounter = 0;

    return true;
}

bool InspectorTemplateWidget::duplicateSelectedItem()
{
    if (!copySelectedItem())
        return true;

    if (!pasteSelectedItem())
        return true;

    return true;
}

bool InspectorTemplateWidget::copySelectedItem()
{
    QString data;

    if (this->treeWidgetTemplateData->selectedItems().count() == 0)
        return true;

    QTreeWidgetItem* item = this->treeWidgetTemplateData->selectedItems().at(0);
    data = item->text(0);
    data += "#" + item->text(1);
    data += "#" + QString::number(item->data(0, Qt::UserRole).toInt());
    data += "#" + item->data(0, Qt::UserRole + 1).toString();

    qApp->clipboard()->setText(data);

    return true;
}

bool InspectorTemplateWidget::pasteSelectedItem()
{
    if (qApp->clipboard()->text().isEmpty())
        return true;

    QStringList parts = qApp->clipboard()->text().split("#");
    if (parts.count() < 2)
        return true;

    QTreeWidgetItem* treeItem = new QTreeWidgetItem();
    treeItem->setText(0, parts.at(0));
    treeItem->setText(1, parts.at(1));
    if (parts.count() >= 3)
        treeItem->setData(0, Qt::UserRole, parts.at(2).toInt());
    if (parts.count() >= 4)
        treeItem->setData(0, Qt::UserRole + 1, parts.at(3));

    this->treeWidgetTemplateData->invisibleRootItem()->insertChild(this->treeWidgetTemplateData->currentIndex().row() + 1, treeItem);

    return true;
}

void InspectorTemplateWidget::itemDoubleClicked(QTreeWidgetItem* current, int column)
{
    Q_UNUSED(current);

    // Don't open the edit dialog when clicking the arrow button column.
    if (column == 2)
        return;

    editRow();
}

void InspectorTemplateWidget::flashlayerChanged(int flashlayer)
{
    this->command->setFlashlayer(flashlayer);
}

void InspectorTemplateWidget::useStoredDataChanged(int state)
{
    this->command->setUseStoredData((state == Qt::Checked) ? true : false);
}

void InspectorTemplateWidget::sendAsJsonChanged(int state)
{
    this->command->setSendAsJson((state == Qt::Checked) ? true : false);
}

void InspectorTemplateWidget::useUppercaseDataChanged(int state)
{
    this->command->setUseUppercaseData((state == Qt::Checked) ? true : false);
}

void InspectorTemplateWidget::triggerOnNextChanged(int state)
{
    this->command->setTriggerOnNext((state == Qt::Checked) ? true : false);
}

void InspectorTemplateWidget::autoPlayChanged(int state)
{
    if (this->command == NULL)
        return;
    this->command->setAutoPlay((state == Qt::Checked) ? true : false);
}

void InspectorTemplateWidget::autoLoopChanged(int state)
{
    if (this->command == NULL)
        return;
    this->command->setAutoLoop(state == Qt::Checked);
}

void InspectorTemplateWidget::autoLoopDelayChanged(int delay)
{
    if (this->command == NULL)
        return;
    this->command->setAutoLoopDelay(delay);
}

void InspectorTemplateWidget::newlineBehaviorChanged(int index)
{
    if (this->command == NULL)
        return;

    this->command->setNewlineBehavior(index);
}

void InspectorTemplateWidget::currentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous)
{
    Q_UNUSED(previous);

    if (current == NULL)
        return;

    updateTemplateDataModels();
}

void InspectorTemplateWidget::loadDebugData()
{
    try
    {
        if (this->command == NULL || this->model == NULL)
            return;

        QString deviceName = this->model->getDeviceName();
        if (deviceName.isEmpty())
        {
            EventManager::getInstance().fireStatusbarEvent(
                StatusbarEvent("Load debug data: no device assigned to this item"));
            return;
        }

        DeviceModel deviceModel = DatabaseManager::getInstance().getDeviceByName(deviceName);
        QString templatePath = deviceModel.getTemplatePath();

        if (templatePath.isEmpty())
        {
            EventManager::getInstance().fireStatusbarEvent(
                StatusbarEvent("No template path configured for device: " + deviceName));
            return;
        }

        QString templateName = this->command->getTemplateName();
        if (templateName.isEmpty())
        {
            EventManager::getInstance().fireStatusbarEvent(
                StatusbarEvent("Load debug data: no template name set"));
            return;
        }

        // An OGraf graphic says the same thing in a standard way: JSON Schema in
        // its manifest, with GDD's gddType naming the editor. That is the version
        // other tools already speak, so it is tried before this fork's own
        // window.debugData convention.
        QString manifestPath = Ograf::findManifest(templatePath, templateName);
        if (!manifestPath.isEmpty())
        {
            loadOgrafFields(manifestPath);
            return;
        }

        QString filePath = QDir(templatePath).filePath(templateName + ".html");

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            EventManager::getInstance().fireStatusbarEvent(
                StatusbarEvent("Could not open template file: " + filePath));
            return;
        }

        QString content = QTextStream(&file).readAll();
        file.close();

        QRegularExpression debugDataRegex("window\\.debugData\\s*=\\s*\\{([^}]*)\\}");
        QRegularExpressionMatch match = debugDataRegex.match(content);
        if (!match.hasMatch())
        {
            EventManager::getInstance().fireStatusbarEvent(
                StatusbarEvent("No window.debugData found in: " + filePath));
            return;
        }

        QString dataBlock = match.captured(1);
        QRegularExpression kvRegex("['\"]([^'\"]+)['\"]\\s*:\\s*['\"]([^'\"]*)['\"]");
        QRegularExpressionMatchIterator it = kvRegex.globalMatch(dataBlock);

        // Optional per-field edit modes: window.debugDataModes = { "f0": "integer",
        // "align": "cycle:left|center|right", ... }. Flat string map, same parse rules.
        QMap<QString, QString> fieldModes;
        QRegularExpression modesRegex("window\\.debugDataModes\\s*=\\s*\\{([^}]*)\\}");
        QRegularExpressionMatch modesMatch = modesRegex.match(content);
        if (modesMatch.hasMatch())
        {
            QRegularExpressionMatchIterator modeIt = kvRegex.globalMatch(modesMatch.captured(1));
            while (modeIt.hasNext())
            {
                QRegularExpressionMatch modeMatch = modeIt.next();
                fieldModes[modeMatch.captured(1)] = modeMatch.captured(2);
            }
        }

        auto modeToInt = [](const QString& mode) -> int {
            if (mode == "integer") return 1;
            if (mode == "decimal") return 2;
            if (mode == "boolean") return 3;
            if (mode == "color") return 4;
            if (mode.startsWith("cycle")) return 5;
            return 0; // text
        };

        // Merge, don't wipe: keys already in the table keep their row (and the
        // operator's current value); keys missing from the table are appended.
        // Re-importing after deleting a key simply brings that key back.
        QTreeWidgetItem* root = this->treeWidgetTemplateData->invisibleRootItem();
        QMap<QString, QTreeWidgetItem*> existingRows;
        for (int i = 0; i < root->childCount(); i++)
            existingRows[root->child(i)->text(0)] = root->child(i);

        int parsedCount = 0;
        int addedCount = 0;
        while (it.hasNext())
        {
            QRegularExpressionMatch kvMatch = it.next();
            QString key = kvMatch.captured(1);
            QString modeSpec = fieldModes.value(key);
            parsedCount++;

            QTreeWidgetItem* treeItem = existingRows.value(key);
            if (treeItem == nullptr)
            {
                treeItem = new QTreeWidgetItem();
                treeItem->setText(0, key);
                treeItem->setText(1, this->checkBoxImportValues->isChecked() ? kvMatch.captured(2) : QString());
                root->addChild(treeItem);
                existingRows[key] = treeItem;
                addedCount++;
            }

            // The template is the source of truth for declared edit modes; rows
            // without a declared mode keep whatever the operator set manually.
            if (!modeSpec.isEmpty())
            {
                treeItem->setData(0, Qt::UserRole, modeToInt(modeSpec));
                // "cycle:left|center|right" -> cycle values after the colon.
                if (modeSpec.startsWith("cycle:"))
                    treeItem->setData(0, Qt::UserRole + 1, modeSpec.mid(6));
            }
        }

        this->fieldCounter = root->childCount();

        if (parsedCount == 0)
        {
            EventManager::getInstance().fireStatusbarEvent(
                StatusbarEvent("window.debugData found but no key-value pairs parsed in: " + filePath));
            return;
        }

        updateTemplateDataModels();

        EventManager::getInstance().fireStatusbarEvent(
            StatusbarEvent(QString("Imported %1 fields (%2 new) from: %3").arg(parsedCount).arg(addedCount).arg(filePath)));
    }
    catch (...)
    {
        EventManager::getInstance().fireStatusbarEvent(
            StatusbarEvent("Load debug data: unexpected error occurred"));
    }
}



void InspectorTemplateWidget::loadOgrafFields(const QString& manifestPath)
{
    Ograf::Manifest manifest = Ograf::load(manifestPath);

    if (!manifest.valid)
    {
        EventManager::getInstance().fireStatusbarEvent(
            StatusbarEvent(QString("OGraf manifest could not be read: %1").arg(manifest.error)));
        return;
    }

    const QVector<Ograf::Field> fields = Ograf::fieldsFor(manifest.schema);

    if (fields.isEmpty())
    {
        EventManager::getInstance().fireStatusbarEvent(
            StatusbarEvent(QString("\"%1\" declares no editable fields.").arg(manifest.name)));
        return;
    }

    // Merge, don't wipe — the same rule the debugData path follows. A key already
    // in the table keeps its row and whatever the operator has typed into it;
    // only its declared type is refreshed, because the manifest is the authority
    // on that and nothing else is.
    QTreeWidgetItem* root = this->treeWidgetTemplateData->invisibleRootItem();
    QMap<QString, QTreeWidgetItem*> existingRows;
    for (int i = 0; i < root->childCount(); i++)
        existingRows[root->child(i)->text(0)] = root->child(i);

    int addedCount = 0;
    foreach (const Ograf::Field& field, fields)
    {
        QTreeWidgetItem* treeItem = existingRows.value(field.key);

        if (treeItem == nullptr)
        {
            treeItem = new QTreeWidgetItem();
            treeItem->setText(0, field.key);
            treeItem->setText(1, this->checkBoxImportValues->isChecked() ? field.value : QString());
            root->addChild(treeItem);
            existingRows[field.key] = treeItem;
            addedCount++;
        }

        treeItem->setData(0, Qt::UserRole, field.mode);

        if (field.mode == Ograf::Mode::Cycle && !field.cycleValues.isEmpty())
            treeItem->setData(0, Qt::UserRole + 1, field.cycleValues);

        // The schema's title is a better thing to hover than the raw key,
        // especially once a nested group has turned keys into dotted paths.
        if (!field.label.isEmpty() && field.label != field.key)
            treeItem->setToolTip(0, field.label);
    }

    this->fieldCounter = root->childCount();

    updateTemplateDataModels();

    EventManager::getInstance().fireStatusbarEvent(
        StatusbarEvent(QString("Imported %1 fields (%2 new) from OGraf graphic \"%3\"")
            .arg(fields.size()).arg(addedCount).arg(manifest.name)));
}

// ---- expected result ----

// Sits directly under the key/value table because it answers a question about it:
// the operator types a row and this says what that row is.
void InspectorTemplateWidget::buildExpectedBox()
{
    // The tree sits in verticalLayoutData, a QVBoxLayout nested inside this widget's
    // grid. Asking the parent widget for its layout returns the grid, and casting
    // that to a QVBoxLayout fails — which is how this box went unbuilt for eight
    // builds. setupUi() names the nested layout, so it is taken by name.
    QWidget* host = this->treeWidgetTemplateData->parentWidget();
    if (host == NULL || this->gridLayout == NULL)
        return;

    this->expectedBox = new QWidget(host);
    this->expectedBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QVBoxLayout* boxLayout = new QVBoxLayout(this->expectedBox);
    boxLayout->setContentsMargins(0, 4, 0, 0);
    boxLayout->setSpacing(3);
    // Height is pinned to the content by pinExpectedBoxHeight() whenever the content
    // changes; width is left to the row. (SetFixedSize would pin both and leave the
    // box as wide as its longest value.)

    QHBoxLayout* headerRow = new QHBoxLayout();
    headerRow->setSpacing(6);

    this->expectedHeading = new QLabel(this->expectedBox);
    this->expectedHeading->setStyleSheet("font-size: 10px; color: rgba(150, 180, 215, 220); font-weight: bold;");
    headerRow->addWidget(this->expectedHeading, 0);

    this->expectedStatus = new QLabel(this->expectedBox);
    this->expectedStatus->setStyleSheet("font-size: 10px; color: rgba(140, 140, 140, 200);");
    headerRow->addWidget(this->expectedStatus, 1);

    // Where these rows came from and how old they are. Kept apart from the row count
    // because it is the part that decides whether to trust the rest.
    this->expectedFreshness = new QLabel(this->expectedBox);
    this->expectedFreshness->setStyleSheet("font-size: 10px; color: rgba(140, 140, 140, 200);");
    headerRow->addWidget(this->expectedFreshness, 0);

    this->buttonRefreshExpected = new QPushButton(tr("Refresh"), this->expectedBox);
    this->buttonRefreshExpected->setFixedHeight(20);
    this->buttonRefreshExpected->setFocusPolicy(Qt::NoFocus);
    this->buttonRefreshExpected->setToolTip("Read the tab again now");
    this->buttonRefreshExpected->setStyleSheet(
        "QPushButton { background-color: rgba(50, 50, 50, 200); color: white; border-radius: 3px;"
        " font-size: 10px; border: 1px solid rgba(70, 70, 70, 200); padding: 2px 10px; }"
        "QPushButton:hover { background-color: rgba(70, 70, 70, 200); }");
    QObject::connect(this->buttonRefreshExpected, &QPushButton::clicked, this, [this]() {
        // Re-read the declaration as well as the rows: a template fixed while the
        // client is running should not need a restart to be believed.
        if (!this->command.isNull() && this->model != NULL)
            SheetDataResolver::getInstance().forgetConnection(this->model->getDeviceName(),
                                                              this->command->getTemplateName());
        refreshExpectedBinding(true);
    });
    headerRow->addWidget(this->buttonRefreshExpected, 0);

    boxLayout->addLayout(headerRow);

    this->treeExpected = new QTreeWidget(this->expectedBox);
    this->treeExpected->setColumnCount(2);
    this->treeExpected->setHeaderLabels(QStringList() << "Column" << "Value");
    this->treeExpected->setRootIsDecorated(false);
    this->treeExpected->setAlternatingRowColors(true);
    this->treeExpected->setSelectionMode(QAbstractItemView::NoSelection);
    this->treeExpected->setFocusPolicy(Qt::NoFocus);
    this->treeExpected->header()->setStretchLastSection(true);
    boxLayout->addWidget(this->treeExpected);

    // Row 8 of the section grid: under the table (row 6) and the Update row (7),
    // above the option rows moved down in the constructor.
    this->gridLayout->addWidget(this->expectedBox, 8, 0, 1, 2, Qt::AlignTop);
    this->gridLayout->setRowStretch(101, 1);

    this->expectedBox->setVisible(false);
}

// Only for templates that say they read a sheet. Silence means no box, not an
// empty one \xe2\x80\x94 a template driven by the connector has no sheet to show.
void InspectorTemplateWidget::refreshExpectedBinding(bool forceReload)
{
    if (this->expectedBox == NULL)
        return;

    this->sheetConnection = TemplateSheetConnection();
    this->expectedRows.clear();

    if (!this->command.isNull() && this->model != NULL)
    {
        this->sheetConnection = SheetDataResolver::getInstance().connectionFor(this->model->getDeviceName(),
                                                                              this->command->getTemplateName());
    }

    // Declared but unusable is shown, not hidden: a silent box is exactly how a
    // typo in the declaration stays a mystery.
    if (this->sheetConnection.declared && !this->sheetConnection.isValid())
    {
        this->expectedBox->setVisible(true);
        this->expectedHeading->setText(tr("sheetConnection"));
        this->treeExpected->clear();
        this->treeExpected->setVisible(false);
        this->expectedFreshness->clear();
        this->expectedStatus->setText(tr("declared without a \"tab\" \xe2\x80\x94 nothing can be read"));
        pinExpectedBoxHeight();
        emit contentChanged();
        return;
    }

    bool connected = this->sheetConnection.isValid();
    this->expectedBox->setVisible(connected);
    pinExpectedBoxHeight();
    emit contentChanged();
    if (!connected)
        return;

    this->treeExpected->setVisible(this->sheetConnection.hasBlocks());

    this->expectedHeading->setText(this->sheetConnection.hasBlocks()
        ? QString("%1 \xe2\x86\x92 %2").arg(this->sheetConnection.tab, this->sheetConnection.fields().join(", "))
        : this->sheetConnection.tab);
    this->expectedHeading->setToolTip(this->sheetConnection.countsSets()
        ? tr("Sets are found by counting runs of member rows, the way the template's own findTeams() does.")
        : tr("One line: the row whose ID equals the value, else that position. Several lines: "
             "base row (value - 1) x lines, the way the template's own getNumberForRow() does."));

    this->treeExpected->clear();
    this->expectedStatus->setText(tr("reading..."));
    this->expectedOrigin = SheetRowsOrigin();
    this->expectedFreshness->clear();

    requestExpectedRows(forceReload);
}

void InspectorTemplateWidget::requestExpectedRows(bool forceReload)
{
    if (this->command.isNull() || this->model == NULL || !this->sheetConnection.isValid())
        return;

    SheetsProjectRegistry::getInstance().discover();
    SheetsProject project = SheetsProjectRegistry::getInstance().projectForTemplate(this->command->getTemplateName());
    if (!project.isValid())
    {
        this->expectedStatus->setText(tr("no sheet project for this template"));
        return;
    }

    this->expectedRequestId = QString("expected|%1|%2|%3")
        .arg(this->command->getTemplateName(), this->sheetConnection.tab)
        .arg(QDateTime::currentMSecsSinceEpoch());

    this->expectedStatus->setText(tr("reading..."));
    SheetDataResolver::getInstance().fetchRows(project, this->sheetConnection.tab,
                                               this->expectedRequestId, forceReload);
}

// The operator sets the row in the table above; this reads it back from there rather
// than from the command, so an edit that has not been committed still answers.
QString InspectorTemplateWidget::currentTemplateFieldValue(const QString& key) const
{
    QTreeWidgetItem* root = this->treeWidgetTemplateData->invisibleRootItem();
    for (int i = 0; i < root->childCount(); i++)
        if (root->child(i)->text(0) == key)
            return root->child(i)->text(1).trimmed();

    return QString();
}

// functions.js resolves the key as data[key] \xe2\x80\x94 an index into the rows, not a value
// to match on \xe2\x80\x94 so this resolves it the same way, or it would answer confidently
// with the wrong row.
// A field's value becomes a place in the tab the way the template itself does it,
// and every template here does one of three things. The rule is chosen from the
// declaration alone, so nothing has to run.
int InspectorTemplateWidget::expectedBaseRow(const QString& rawStart, int lines, QString* how) const
{
    const QList<SheetRow>& rows = this->expectedRows;

    // Runs of member rows, counted. The value is the run number, 1-based.
    if (this->sheetConnection.countsSets())
    {
        bool numeric = false;
        int wanted = rawStart.toInt(&numeric);
        if (!numeric || wanted < 1)
            return -1;

        auto isMember = [this](const SheetRow& row) {
            QString cell = row.value(this->sheetConnection.setsColumn).trimmed();
            if (cell.isEmpty())
                return false;
            if (!this->sheetConnection.setsDigit)
                return true;
            for (const QChar& c : cell)
                if (c.isDigit())
                    return true;
            return false;
        };

        int seen = 0;
        for (int i = 0; i < rows.count(); i++)
        {
            if (!isMember(rows.at(i)))
                continue;

            int first = i;
            while (i < rows.count() && isMember(rows.at(i)))
                i++;

            if (++seen == wanted)
            {
                // The row above the run is the set's own header, when there is one.
                int header = (first > 0 && !isMember(rows.at(first - 1))) ? first - 1 : first;
                if (how != NULL)
                    *how = QString("set %1 of %2").arg(wanted).arg(seen);
                return header;
            }
        }

        if (how != NULL)
            *how = QString("only %1 set(s) in the tab").arg(seen);
        return -1;
    }

    // Several lines: a stride from a 1-based set number.
    if (lines > 1)
    {
        bool numeric = false;
        int n = rawStart.toInt(&numeric);
        if (!numeric || n < 1)
            return -1;

        if (how != NULL)
            *how = QString("set %1, stride %2").arg(n).arg(lines);
        return (n - 1) * lines;
    }

    // One line: the ID column first, then the value as a 1-based position.
    QString target = rawStart.trimmed();
    for (int i = 0; i < rows.count(); i++)
    {
        if (rows.at(i).value("ID").trimmed() == target)
        {
            if (how != NULL)
                *how = QString("ID %1").arg(target);
            return i;
        }
    }

    bool numeric = false;
    int position = target.toInt(&numeric);
    if (numeric && position >= 1 && position <= rows.count())
    {
        if (how != NULL)
            *how = QString("no ID %1, position %1").arg(target);
        return position - 1;
    }

    if (how != NULL)
        *how = QString("no row with ID %1").arg(target);
    return -1;
}

void InspectorTemplateWidget::renderExpectedRow()
{
    if (this->expectedBox == NULL || !this->sheetConnection.isValid() || this->expectedRows.isEmpty())
        return;

    this->treeExpected->clear();

    // A tab with no selecting field: the template reads all of it, so the count is
    // the whole answer and there is no block to show.
    if (!this->sheetConnection.hasBlocks())
    {
        this->expectedStatus->setText(QString("%1 rows").arg(this->expectedRows.count()));
        pinExpectedBoxHeight();
        emit contentChanged();
        return;
    }

    QStringList summary;
    int shownLines = 0;
    bool anyShort = false;

    int fieldIndex = -1;
    for (const auto& block : this->sheetConnection.blocks)
    {
        fieldIndex++;
        const QString& field = block.first;
        int wanted = block.second;

        // The named field, or the raw f-key standing in for it. The templates read
        // these by position \xe2\x80\x94 lineups take f0 as home and f1 as away \xe2\x80\x94 so the
        // nth declared field falls back to f(n-1), not always to f0.
        QString rawStart = currentTemplateFieldValue(field);
        if (rawStart.isEmpty())
            rawStart = currentTemplateFieldValue(QString("f%1").arg(fieldIndex));

        // The block header earns its row only when it carries something the status
        // line does not: a set of several lines, a short set, or a failure. A single
        // line that resolved cleanly says "row = 1" in the status already.
        auto makeHeader = [this](const QString& text, const QColor& colour) {
            QTreeWidgetItem* header = new QTreeWidgetItem();
            header->setFirstColumnSpanned(true);
            header->setBackground(0, QBrush(QColor(45, 45, 45)));
            header->setForeground(0, QBrush(colour));
            header->setText(0, text);
            this->treeExpected->addTopLevelItem(header);
        };

        if (rawStart.isEmpty())
        {
            makeHeader(QString("%1: not set").arg(field), QColor(150, 180, 215));
            summary.append(QString("%1 = ?").arg(field));
            continue;
        }

        QString how;
        int start = expectedBaseRow(rawStart, wanted, &how);
        if (start < 0 || start >= this->expectedRows.count())
        {
            makeHeader(QString("%1 = %2: %3").arg(field, rawStart, how), QColor(220, 130, 130));
            summary.append(QString("%1 = %2!").arg(field, rawStart));
            continue;
        }

        // A counted set is as long as its run; a declared block is as long as declared.
        int length = wanted;
        if (this->sheetConnection.countsSets())
        {
            length = 1;
            while (start + length < this->expectedRows.count()
                   && !this->expectedRows.at(start + length).value(this->sheetConnection.setsColumn).trimmed().isEmpty())
                length++;
        }

        int available = qMin(length, this->expectedRows.count() - start);
        bool isShort = available < wanted && !this->sheetConnection.countsSets();
        if (isShort)
            anyShort = true;

        // "Eleven names where twelve were asked for is a set the sheet has not
        // finished, and that is worth seeing before air rather than during it."
        if (isShort)
        {
            makeHeader(QString("%1 = %2 \xc2\xb7 %3 \xc2\xb7 %4 of %5 lines")
                           .arg(field, rawStart, how).arg(available).arg(wanted),
                       QColor(230, 180, 80));
        }
        else if (available > 1)
        {
            makeHeader(QString("%1 = %2 \xc2\xb7 %3 \xc2\xb7 %4 lines")
                           .arg(field, rawStart, how).arg(available),
                       QColor(150, 180, 215));
        }
        summary.append(isShort ? QString("%1 = %2 (%3/%4)").arg(field, rawStart).arg(available).arg(wanted)
                               : QString("%1 = %2").arg(field, rawStart));

        for (int line = 0; line < available; line++)
        {
            const SheetRow& row = this->expectedRows.at(start + line);

            if (available > 1)
            {
                QTreeWidgetItem* lineItem = new QTreeWidgetItem();
                lineItem->setFirstColumnSpanned(true);
                lineItem->setText(0, QString("   line %1  (row %2)").arg(line + 1).arg(start + line));
                lineItem->setForeground(0, QBrush(QColor(140, 140, 140)));
                this->treeExpected->addTopLevelItem(lineItem);
                shownLines++;
            }

            // The sheet's own column order, then anything the row has that the
            // header did not name.
            QStringList ordered = this->expectedOrigin.columns;
            foreach (const QString& column, row.keys())
                if (!ordered.contains(column))
                    ordered.append(column);

            foreach (const QString& column, ordered)
            {
                if (!row.contains(column))
                    continue;

                QTreeWidgetItem* item = new QTreeWidgetItem();
                item->setText(0, "      " + column);
                item->setText(1, row.value(column));
                if (row.value(column).isEmpty())
                    item->setForeground(1, QBrush(QColor(140, 140, 140)));
                this->treeExpected->addTopLevelItem(item);
                shownLines++;
            }
        }
    }

    this->treeExpected->resizeColumnToContents(0);
    this->expectedStatus->setText(QString("%1 rows \xc2\xb7 %2").arg(this->expectedRows.count()).arg(summary.join("  ")));
    this->expectedStatus->setStyleSheet(anyShort
        ? "font-size: 10px; color: rgba(230, 180, 80, 230);"
        : "font-size: 10px; color: rgba(140, 140, 140, 200);");

    int rowHeight = this->treeExpected->sizeHintForRow(0);
    if (rowHeight <= 0)
        rowHeight = this->treeExpected->fontMetrics().height() + 6;

    // Count what was actually added: since the block header became lazy, a clean
    // single line has no header row, and counting one left an empty line below.
    int visible = qMin(this->treeExpected->topLevelItemCount(), 18);
    this->treeExpected->setFixedHeight(this->treeExpected->header()->sizeHint().height()
                                       + (visible * rowHeight) + (2 * this->treeExpected->frameWidth()));

    pinExpectedBoxHeight();
    emit contentChanged();
}


// Cache or live, and how old. A cached copy is the normal case and not a problem in
// itself \xe2\x80\x94 it only becomes one when nobody has refreshed it for a while, so the
// colour follows the age rather than the source.
// The box is exactly as tall as what is in it, whatever the cell around it does. A
// Fixed size policy only stops a layout asking the box to grow; when a cell is
// forced taller anyway the box would still be stretched into it and the slack would
// land in the header row. Pinning the height stops that without touching the width.
void InspectorTemplateWidget::pinExpectedBoxHeight()
{
    if (this->expectedBox == NULL || this->expectedBox->layout() == NULL)
        return;

    QLayout* layout = this->expectedBox->layout();
    layout->invalidate();
    layout->activate();
    this->expectedBox->setFixedHeight(layout->sizeHint().height());
}

void InspectorTemplateWidget::renderExpectedFreshness()
{
    if (this->expectedFreshness == NULL)
        return;

    QString source = this->expectedOrigin.describe();

    if (!this->expectedOrigin.cachedAt.isValid())
    {
        // A cache that does not say when it wrote what it is serving. The client's own
        // does; the PHP service does not, and pretending otherwise would be worse.
        this->expectedFreshness->setText(this->expectedOrigin.isCached()
            ? QString("%1 \xc2\xb7 age unknown").arg(source) : source);
        this->expectedFreshness->setStyleSheet("font-size: 10px; color: rgba(140, 140, 140, 200);");
        this->expectedFreshness->setToolTip(this->expectedOrigin.isCached()
            ? tr("This cache does not report when it stored these rows")
            : tr("Read from the sheet just now"));
        return;
    }

    qint64 seconds = this->expectedOrigin.cachedAt.secsTo(QDateTime::currentDateTime());
    if (seconds < 0)
        seconds = 0;

    QString age;
    if (seconds < 60)
        age = tr("just now");
    else if (seconds < 3600)
        age = QString("%1m old").arg(seconds / 60);
    else if (seconds < 86400)
        age = QString("%1h old").arg(seconds / 3600);
    else
        age = QString("%1d old").arg(seconds / 86400);

    // Under five minutes is ordinary, an hour is worth noticing, a day means nothing
    // has refreshed this and the row on screen may be describing last week's match.
    QString colour = "rgba(140, 140, 140, 200)";
    if (seconds >= 86400)
        colour = "rgba(200, 90, 80, 220)";
    else if (seconds >= 3600)
        colour = "rgba(200, 160, 60, 220)";

    this->expectedFreshness->setText(QString("%1 \xc2\xb7 %2").arg(source, age));
    this->expectedFreshness->setStyleSheet(QString("font-size: 10px; color: %1;").arg(colour));
    this->expectedFreshness->setToolTip(QString("Stored %1")
        .arg(this->expectedOrigin.cachedAt.toString("yyyy-MM-dd HH:mm:ss")));
}

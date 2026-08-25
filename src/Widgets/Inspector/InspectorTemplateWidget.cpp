#include "InspectorTemplateWidget.h"
#include "KeyValueDialog.h"
#include "NumericValueDelegate.h"

#include "Global.h"

#include "DatabaseManager.h"
#include "EventManager.h"
#include "Events/StatusbarEvent.h"
#include "Models/DeviceModel.h"
#include "Models/KeyValueModel.h"

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QRegularExpression>
#include <QtCore/QTextStream>

#include <QtGui/QClipboard>
#include <QtGui/QCursor>
#include <QtGui/QKeyEvent>
#include <QtGui/QResizeEvent>

#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
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
    }

    blockAllSignals(false);
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

void InspectorTemplateWidget::updateTemplateDataModels()
{
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
    dialog->move(QPoint(QCursor::pos().x() - dialog->width() + 40, QCursor::pos().y() - dialog->height() - 10));
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
    dialog->move(QPoint(QCursor::pos().x() - dialog->width() + 40, QCursor::pos().y() - dialog->height() - 10));
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

        for (int i = this->treeWidgetTemplateData->invisibleRootItem()->childCount() - 1; i >= 0; i--)
            delete this->treeWidgetTemplateData->invisibleRootItem()->child(i);

        this->fieldCounter = 0;
        while (it.hasNext())
        {
            QRegularExpressionMatch kvMatch = it.next();
            QTreeWidgetItem* treeItem = new QTreeWidgetItem();
            treeItem->setText(0, kvMatch.captured(1));
            treeItem->setText(1, this->checkBoxImportValues->isChecked() ? kvMatch.captured(2) : QString());
            this->treeWidgetTemplateData->invisibleRootItem()->addChild(treeItem);
            this->fieldCounter++;
        }

        if (this->fieldCounter == 0)
        {
            EventManager::getInstance().fireStatusbarEvent(
                StatusbarEvent("window.debugData found but no key-value pairs parsed in: " + filePath));
            return;
        }

        updateTemplateDataModels();

        EventManager::getInstance().fireStatusbarEvent(
            StatusbarEvent(QString("Loaded %1 debug data fields from: %2").arg(this->fieldCounter).arg(filePath)));
    }
    catch (...)
    {
        EventManager::getInstance().fireStatusbarEvent(
            StatusbarEvent("Load debug data: unexpected error occurred"));
    }
}


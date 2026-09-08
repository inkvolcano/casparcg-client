#include "SimpleInspectorWidget.h"

#include "Global.h"

#include "EventManager.h"
#include "PanelHelper.h"
#include "WheelGuard.h"
#include "Commands/TemplateCommand.h"
#include "Events/Inspector/LabelChangedEvent.h"
#include "Events/StatusbarEvent.h"
#include "Events/Rundown/ExecutePlayoutCommandEvent.h"
#include "Inspector/DialogPosition.h"
#include "Inspector/KeyValueDialog.h"
#include "Models/KeyValueModel.h"
#include "Playout.h"

#include <QtCore/QDateTime>

#include <QtGui/QBrush>
#include <QtGui/QColor>

#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>

SimpleInspectorWidget::SimpleInspectorWidget(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 1, 0, 0);
    outerLayout->setSpacing(0);

    this->tabWidget = new QTabWidget(this);
    this->tabWidget->setFocusPolicy(Qt::NoFocus);

    QWidget* tab = new QWidget(this->tabWidget);
    QVBoxLayout* tabLayout = new QVBoxLayout(tab);
    tabLayout->setContentsMargins(4, 4, 4, 4);
    tabLayout->setSpacing(4);

    QFormLayout* form = new QFormLayout();
    form->setContentsMargins(0, 0, 0, 0);

    this->lineEditLabel = new QLineEdit(tab);
    QObject::connect(this->lineEditLabel, &QLineEdit::editingFinished, this, [this]() {
        if (this->command == nullptr || this->loading)
            return;
        EventManager::getInstance().fireLabelChangedEvent(LabelChangedEvent(this->lineEditLabel->text()));
    });
    form->addRow("Label:", this->lineEditLabel);

    QHBoxLayout* channelRow = new QHBoxLayout();
    this->spinBoxChannel = new QSpinBox(tab);
    this->spinBoxChannel->setRange(1, 99);
    QObject::connect(this->spinBoxChannel, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        if (this->command != nullptr && !this->loading)
            this->command->setChannel(value);
    });
    channelRow->addWidget(this->spinBoxChannel, 1);

    this->spinBoxVideolayer = new QSpinBox(tab);
    this->spinBoxVideolayer->setRange(0, 999);
    QObject::connect(this->spinBoxVideolayer, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        if (this->command != nullptr && !this->loading)
            this->command->setVideolayer(value);
    });
    channelRow->addWidget(this->spinBoxVideolayer, 1);
    form->addRow("Ch / Layer:", channelRow);

    // Boxes only react to the mouse wheel after being clicked.
    WheelGuard::apply(this->spinBoxChannel);
    WheelGuard::apply(this->spinBoxVideolayer);

    tabLayout->addLayout(form);

    // Header row above the data table, mirroring the full template inspector:
    // [Update] then the same [+] / [-] tool buttons, right-aligned.
    QHBoxLayout* headerRow = new QHBoxLayout();
    headerRow->setSpacing(2);
    headerRow->addStretch();

    this->buttonUpdate = new QPushButton("Update", tab);
    this->buttonUpdate->setFixedHeight(20);
    this->buttonUpdate->setFocusPolicy(Qt::NoFocus);
    this->buttonUpdate->setToolTip("Send the key/values below to the on-air template");
    this->buttonUpdate->setStyleSheet(
        "QPushButton { background-color: rgba(50, 80, 120, 220); color: white; border-radius: 3px;"
        " font-size: 10px; font-weight: bold; border: 1px solid rgba(80, 110, 150, 200); padding: 2px 10px; }"
        "QPushButton:hover { background-color: rgba(70, 100, 145, 220); }");
    QObject::connect(this->buttonUpdate, &QPushButton::clicked, this, &SimpleInspectorWidget::updateClicked);
    headerRow->addWidget(this->buttonUpdate);

    this->buttonAddKey = new QToolButton(tab);
    this->buttonAddKey->setFixedSize(40, 20);
    this->buttonAddKey->setToolTip("Add a key/value (with edit mode)");
    this->buttonAddKey->setStyleSheet(
        "QToolButton { background-color: rgba(50, 50, 50, 200);"
        " border-radius: 3px; border: 1px solid rgba(70, 70, 70, 200); padding: 0px;"
        " image: url(:/Graphics/Images/Add.png); }"
        " QToolButton:hover { background-color: rgba(70, 70, 70, 200);"
        " image: url(:/Graphics/Images/AddHover.png); }");
    QObject::connect(this->buttonAddKey, &QToolButton::clicked, this, &SimpleInspectorWidget::addKeyClicked);
    headerRow->addWidget(this->buttonAddKey);

    this->buttonRemoveKey = new QToolButton(tab);
    this->buttonRemoveKey->setFixedSize(40, 20);
    this->buttonRemoveKey->setToolTip("Remove selected key");
    this->buttonRemoveKey->setStyleSheet(
        "QToolButton { background-color: rgba(50, 50, 50, 200);"
        " border-radius: 3px; border: 1px solid rgba(70, 70, 70, 200); padding: 0px;"
        " image: url(:/Graphics/Images/Remove.png); }"
        " QToolButton:hover { background-color: rgba(70, 70, 70, 200);"
        " image: url(:/Graphics/Images/RemoveHover.png); }");
    QObject::connect(this->buttonRemoveKey, &QToolButton::clicked, this, &SimpleInspectorWidget::removeKeyClicked);
    headerRow->addWidget(this->buttonRemoveKey);

    tabLayout->addLayout(headerRow);

    // Key/value data (templates only) — values editable inline, rows edited
    // (incl. mode) via double-click, same as the full inspector.
    this->treeWidgetData = new QTreeWidget(tab);
    this->treeWidgetData->setColumnCount(2);
    this->treeWidgetData->setHeaderLabels({"Key", "Value"});
    this->treeWidgetData->setRootIsDecorated(false);
    this->treeWidgetData->header()->setStretchLastSection(true);
    QObject::connect(this->treeWidgetData->model(), &QAbstractItemModel::dataChanged, this, [this]() {
        if (!this->loading)
            syncDataToCommand();
    });
    QObject::connect(this->treeWidgetData, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem*, int) {
        editKeyClicked();
    });
    tabLayout->addWidget(this->treeWidgetData, 1);

    this->tabWidget->addTab(tab, "Inspector");
    outerLayout->addWidget(this->tabWidget);

    setupMenus();

    this->collapsed = PanelHelper::isPanelCollapsed("SimpleInspector");
    if (this->collapsed)
    {
        this->expandCollapseAction->setText("Expand");
        this->tabWidget->widget(0)->setVisible(false);
        this->setFixedHeight(25);
    }
    else
    {
        this->setFixedHeight(Panel::DEFAULT_SIMPLE_INSPECTOR_HEIGHT);
    }

    QObject::connect(&EventManager::getInstance(), SIGNAL(rundownItemSelected(const RundownItemSelectedEvent&)), this, SLOT(rundownItemSelected(const RundownItemSelectedEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(emptyRundown(const EmptyRundownEvent&)), this, SLOT(emptyRundown(const EmptyRundownEvent&)));
}

void SimpleInspectorWidget::setupMenus()
{
    this->dropdownMenu = new QMenu(this);
    this->dropdownMenu->setObjectName("panelMenu");
    PanelHelper::addMoveActions(this->dropdownMenu, "SimpleInspector", this);
    this->dropdownMenu->addSeparator();
    this->expandCollapseAction = this->dropdownMenu->addAction("Collapse", this, SLOT(toggleExpandCollapse()));

    this->menuButton = new QToolButton(this->tabWidget);
    this->menuButton->setText(QString::fromUtf8("\xe2\x89\xa1"));
    this->menuButton->setFixedSize(22, 22);
    this->menuButton->setMenu(this->dropdownMenu);
    this->menuButton->setPopupMode(QToolButton::InstantPopup);
    this->tabWidget->setCornerWidget(this->menuButton);
}

void SimpleInspectorWidget::toggleExpandCollapse()
{
    this->collapsed = !this->collapsed;
    PanelHelper::setPanelCollapsed("SimpleInspector", this->collapsed);

    this->expandCollapseAction->setText(this->collapsed ? "Expand" : "Collapse");
    this->tabWidget->widget(0)->setVisible(!this->collapsed);

    if (this->collapsed)
        this->setFixedHeight(25);
    else
        this->setFixedHeight(Panel::DEFAULT_SIMPLE_INSPECTOR_HEIGHT);
}

void SimpleInspectorWidget::rundownItemSelected(const RundownItemSelectedEvent& event)
{
    this->loading = true;

    this->command = event.getCommand();
    this->model = event.getLibraryModel();
    this->templateCommand = dynamic_cast<TemplateCommand*>(event.getCommand());

    bool hasItem = (this->command != nullptr && this->model != nullptr);
    this->lineEditLabel->setEnabled(hasItem);
    this->spinBoxChannel->setEnabled(hasItem);
    this->spinBoxVideolayer->setEnabled(hasItem);
    this->buttonUpdate->setEnabled(hasItem);

    if (hasItem)
    {
        this->lineEditLabel->setText(this->model->getLabel().split('/').last());
        this->spinBoxChannel->setValue(this->command->getBaseChannel());
        this->spinBoxVideolayer->setValue(this->command->getVideolayer());
    }
    else
    {
        this->lineEditLabel->clear();
    }

    this->treeWidgetData->clear();
    bool isTemplate = (this->templateCommand != nullptr);
    this->treeWidgetData->setVisible(isTemplate);
    this->buttonAddKey->setVisible(isTemplate);
    this->buttonRemoveKey->setVisible(isTemplate);
    if (isTemplate)
    {
        foreach (const KeyValueModel& kv, this->templateCommand->getTemplateDataModels())
        {
            QTreeWidgetItem* item = new QTreeWidgetItem();
            item->setText(0, kv.getKey());
            item->setText(1, kv.getValue());
            item->setData(0, Qt::UserRole, kv.getMode());
            item->setData(0, Qt::UserRole + 1, kv.getCycleValues());
            item->setFlags(item->flags() | Qt::ItemIsEditable);
            this->treeWidgetData->addTopLevelItem(item);
        }
        this->treeWidgetData->resizeColumnToContents(0);
    }


    this->loading = false;
}

void SimpleInspectorWidget::emptyRundown(const EmptyRundownEvent& event)
{
    Q_UNUSED(event);

    this->loading = true;
    this->command = nullptr;
    this->model = nullptr;
    this->templateCommand = nullptr;
    this->lineEditLabel->clear();
    this->lineEditLabel->setEnabled(false);
    this->spinBoxChannel->setEnabled(false);
    this->spinBoxVideolayer->setEnabled(false);
    this->buttonUpdate->setEnabled(false);
    this->treeWidgetData->clear();
    this->loading = false;
}

void SimpleInspectorWidget::syncDataToCommand()
{
    if (this->templateCommand == nullptr)
        return;

    // Rebuild the models from the table; mode/cycle metadata rides in the item data.
    QList<KeyValueModel> models;
    for (int i = 0; i < this->treeWidgetData->topLevelItemCount(); i++)
    {
        QTreeWidgetItem* item = this->treeWidgetData->topLevelItem(i);
        models.append(KeyValueModel(item->text(0), item->text(1),
                                    item->data(0, Qt::UserRole).toInt(),
                                    item->data(0, Qt::UserRole + 1).toString()));
    }

    this->templateCommand->setTemplateDataModels(models);
}

void SimpleInspectorWidget::addKeyClicked()
{
    if (this->templateCommand == nullptr)
        return;

    KeyValueDialog dialog(this);
    DialogPosition::moveNearCursor(&dialog);
    dialog.setTitle("New Template Data");
    dialog.setKey(QString("f%1").arg(this->treeWidgetData->topLevelItemCount()));
    if (dialog.exec() != QDialog::Accepted)
        return;

    QTreeWidgetItem* item = new QTreeWidgetItem();
    item->setText(0, dialog.getKey());
    item->setText(1, dialog.getValue());
    item->setData(0, Qt::UserRole, dialog.getMode());
    item->setData(0, Qt::UserRole + 1, dialog.getCycleValues());
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    this->treeWidgetData->addTopLevelItem(item);
    this->treeWidgetData->setCurrentItem(item);

    syncDataToCommand();
}

void SimpleInspectorWidget::editKeyClicked()
{
    QTreeWidgetItem* item = this->treeWidgetData->currentItem();
    if (this->templateCommand == nullptr || item == nullptr)
        return;

    KeyValueDialog dialog(this);
    DialogPosition::moveNearCursor(&dialog);
    dialog.setTitle("Edit Template Data");
    dialog.setKey(item->text(0));
    dialog.setValue(item->text(1));
    dialog.setMode(item->data(0, Qt::UserRole).toInt());
    dialog.setCycleValues(item->data(0, Qt::UserRole + 1).toString());
    if (dialog.exec() != QDialog::Accepted)
        return;

    item->setText(0, dialog.getKey());
    item->setText(1, dialog.getValue());
    item->setData(0, Qt::UserRole, dialog.getMode());
    item->setData(0, Qt::UserRole + 1, dialog.getCycleValues());

    syncDataToCommand();
}

void SimpleInspectorWidget::removeKeyClicked()
{
    QTreeWidgetItem* item = this->treeWidgetData->currentItem();
    if (this->templateCommand == nullptr || item == nullptr)
        return;

    delete item;
    syncDataToCommand();
}

void SimpleInspectorWidget::updateClicked()
{
    if (this->command == nullptr)
        return;

    syncDataToCommand();

    // Acts on the rundown's current item — which is the selected button's item.
    EventManager::getInstance().fireExecutePlayoutCommandEvent(
        ExecutePlayoutCommandEvent(Playout::PlayoutType::Update));
}



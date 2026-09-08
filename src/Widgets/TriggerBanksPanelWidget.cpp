#include "TriggerBanksPanelWidget.h"

#include "PanelPlacement.h"
#include "ChannelBadge.h"

#include "Global.h"
#include "PanelHelper.h"
#include "DeviceManager.h"
#include "EventManager.h"
#include "RelayClient.h"
#include "SheetCacheServer.h"
#include "TriggerBankRegistry.h"
#include "Timecode.h"
#include "Rundown/AbstractRundownWidget.h"

#include "CasparDevice.h"

#include <algorithm>

#include "DatabaseManager.h"

#include <QtCore/QDateTime>
#include <QtCore/QFileInfo>
#include <QtCore/QSet>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QTreeWidget>

TriggerBanksPanelWidget::TriggerBanksPanelWidget(QWidget* parent)
    : QWidget(parent),
      banksCollapsed(false)
{
    setupUi(this);

    // Grows to fit its content rather than holding a fixed share of the column.
    this->tabWidgetBanks->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    QString val;
    val = DatabaseManager::getInstance().getConfigurationByName("ShowBankIcons").getValue();
    this->showBankIcons = val.isEmpty() || val == "true";
    val = DatabaseManager::getInstance().getConfigurationByName("BigBoldMode").getValue();
    this->bigBoldMode = (val == "true");

    int banksMinH = DatabaseManager::getInstance()
        .getConfigurationByName("MinHeight_TriggerBanks").getValue().toInt();
    if (banksMinH > 0)
        this->tabWidgetBanks->setMinimumHeight(banksMinH);

    setupMenus();
    setupBanksPanel();

    this->bankIconsContainer->setVisible(this->showBankIcons);

    updatePlacement();

    // The layout decides whether any of this runs, so the answer is re-read
    // whenever the layout changes rather than only at startup.
    QObject::connect(&EventManager::getInstance(), &EventManager::rebuildLayout,
                     this, [this]() { updatePlacement(); });

    QObject::connect(&EventManager::getInstance(), SIGNAL(bankAssignmentChanged(const BankAssignmentChangedEvent&)),
                     this, SLOT(bankAssignmentChanged(const BankAssignmentChangedEvent&)));

    // Row size follows the same display setting the Activity panel uses.
    QObject::connect(&EventManager::getInstance(), &EventManager::bigBoldModeChanged,
                     this, [this](bool active) {
        this->bigBoldMode = active;
        rebuildAllBankEntries();
    });
}

// Whether this panel is anywhere in the layout. A panel that is not placed does
// no work at all — not merely no drawing, because a hidden widget still receives
// the signal and the handler still runs.
void TriggerBanksPanelWidget::updatePlacement()
{
    const bool simpleMode = DatabaseManager::getInstance()
        .getConfigurationByName("SimpleMode").getValue() == "true";

    QStringList columns;
    foreach (const QString& key, PanelPlacement::columnKeys(simpleMode))
        columns.append(DatabaseManager::getInstance().getConfigurationByName(key).getValue());

    this->banksPlaced = PanelPlacement::isPlaced("TriggerBanks", columns);

}

void TriggerBanksPanelWidget::setupMenus()
{
    // Banks hamburger menu.
    this->banksMenu = new QMenu(this);
    this->banksMenu->setObjectName("panelMenu");
    PanelHelper::addMoveActions(this->banksMenu, "TriggerBanks", this);
    this->banksMenu->addSeparator();

    // "Minimum Height" submenu with presets.
    {
        QMenu* banksMinMenu = new QMenu("Minimum Height", this);
        QActionGroup* banksMinHGroup = new QActionGroup(banksMinMenu);
        banksMinHGroup->setExclusive(true);
        int currentBanksMinH = DatabaseManager::getInstance()
            .getConfigurationByName("MinHeight_TriggerBanks").getValue().toInt();
        for (int h : {0, 30, 50, 80, 100})
        {
            QAction* a = banksMinMenu->addAction(h == 0 ? "None" : QString("%1 px").arg(h));
            a->setCheckable(true);
            a->setData(h);
            a->setChecked(h == currentBanksMinH);
            banksMinHGroup->addAction(a);
        }
        QObject::connect(banksMinHGroup, &QActionGroup::triggered, this, [this](QAction* action) {
            int h = action->data().toInt();
            DatabaseManager::getInstance().updateConfiguration(
                ConfigurationModel(0, "MinHeight_TriggerBanks", QString::number(h)));
            this->tabWidgetBanks->setMinimumHeight(h);
        });
        this->banksMenu->addMenu(banksMinMenu);
    }

    this->banksMenu->addSeparator();
    this->banksExpandCollapseAction = this->banksMenu->addAction("Collapse", this, &TriggerBanksPanelWidget::toggleBanksCollapse);

    this->banksMenuButton = new QToolButton(this->tabWidgetBanks);
    this->banksMenuButton->setText(QString::fromUtf8("\xe2\x89\xa1"));
    this->banksMenuButton->setFixedSize(22, 22);
    this->banksMenuButton->setMenu(this->banksMenu);
    this->banksMenuButton->setPopupMode(QToolButton::InstantPopup);
    this->tabWidgetBanks->setCornerWidget(this->banksMenuButton);
}


void TriggerBanksPanelWidget::setupBanksPanel()
{
    // Set up Banks content layout.
    QVBoxLayout* banksOuterLayout = new QVBoxLayout(this->widgetBanks);
    banksOuterLayout->setContentsMargins(4, 2, 4, 2);
    banksOuterLayout->setSpacing(4);

    // Create horizontal row of bank icons (B1-B9).
    this->bankIconsContainer = new QWidget(this->widgetBanks);
    QHBoxLayout* iconsLayout = new QHBoxLayout(this->bankIconsContainer);
    iconsLayout->setContentsMargins(0, 4, 0, 0);
    iconsLayout->setSpacing(4);

    for (int i = 0; i < BANK_COUNT; i++)
    {
        this->bankIcons[i] = new QLabel(this->bankIconsContainer);
        this->bankIcons[i]->setText(QString("B%1").arg(i + 1));
        this->bankIcons[i]->setAlignment(Qt::AlignCenter);
        this->bankIcons[i]->setFixedHeight(20);
        this->bankIcons[i]->setStyleSheet(
            "background-color: rgba(60, 60, 60, 200); "
            "color: rgba(100, 100, 100, 200); "
            "border-radius: 3px; "
            "font-size: 10px; "
            "font-weight: bold;");
        iconsLayout->addWidget(this->bankIcons[i], 1);
    }

    banksOuterLayout->addWidget(this->bankIconsContainer);

    // Create layout for bank entries (below the icons).
    this->banksEntriesLayout = new QVBoxLayout();
    this->banksEntriesLayout->setSpacing(2);
    banksOuterLayout->addLayout(this->banksEntriesLayout);
    banksOuterLayout->addStretch();  // Push entries to top
}

void TriggerBanksPanelWidget::toggleBanksCollapse()
{
    this->banksCollapsed = !this->banksCollapsed;
    this->widgetBanks->setVisible(!this->banksCollapsed);
    this->banksExpandCollapseAction->setText(this->banksCollapsed ? "Expand" : "Collapse");

    if (this->banksCollapsed)
    {
        this->tabWidgetBanks->setFixedHeight(Panel::COMPACT_AUDIOLEVELS_HEIGHT);
    }
    else
    {
        int minH = DatabaseManager::getInstance()
            .getConfigurationByName("MinHeight_TriggerBanks").getValue().toInt();
        this->tabWidgetBanks->setMinimumHeight(qMax(minH, 0));
        this->tabWidgetBanks->setMaximumHeight(QWIDGETSIZE_MAX);
    }
}

void TriggerBanksPanelWidget::updateBankIcons()
{
    // Update icons: only banks with actual assignments are lit.
    for (int i = 0; i < BANK_COUNT; i++)
    {
        int bankNum = i + 1;
        bool hasAssignment = (TriggerBankRegistry::getInstance().getItem(bankNum) != nullptr);

        if (hasAssignment)
        {
            // Lit - bank has an assignment
            this->bankIcons[i]->setStyleSheet(
                "background-color: rgba(255, 165, 0, 220); "
                "color: white; "
                "border-radius: 3px; "
                "font-size: 10px; "
                "font-weight: bold;");
        }
        else
        {
            // Greyed out - no assignment
            this->bankIcons[i]->setStyleSheet(
                "background-color: rgba(60, 60, 60, 200); "
                "color: rgba(100, 100, 100, 200); "
                "border-radius: 3px; "
                "font-size: 10px; "
                "font-weight: bold;");
        }
    }
}

void TriggerBanksPanelWidget::bankAssignmentChanged(const BankAssignmentChangedEvent& event)
{
    updateBankDisplay(event.getBankId());
    updateBankIcons();
}

void TriggerBanksPanelWidget::updateBankDisplay(int bankId)
{
    if (bankId < 1 || bankId > BANK_COUNT)
        return;

    QTreeWidgetItem* item = TriggerBankRegistry::getInstance().getItem(bankId);

    // If no item assigned, remove the bank entry if it exists
    if (item == nullptr)
    {
        removeBankEntry(bankId);
        return;
    }

    QTreeWidget* treeWidget = item->treeWidget();
    if (treeWidget == nullptr)
    {
        removeBankEntry(bankId);
        return;
    }

    QWidget* widget = treeWidget->itemWidget(item, 0);
    if (widget == nullptr)
    {
        removeBankEntry(bankId);
        return;
    }

    AbstractRundownWidget* rundownWidget = dynamic_cast<AbstractRundownWidget*>(widget);
    if (rundownWidget == nullptr)
    {
        removeBankEntry(bankId);
        return;
    }

    // Get item label
    QString label = rundownWidget->getLibraryModel()->getLabel();
    if (label.isEmpty())
        label = rundownWidget->getLibraryModel()->getName();
    if (label.isEmpty())
        label = rundownWidget->getLibraryModel()->getType();

    // Get channel info
    int channel = rundownWidget->getCommand()->getChannel();
    int videolayer = rundownWidget->getCommand()->getVideolayer();
    QString channelText = QString("%1-%2").arg(channel).arg(videolayer);

    // Create or update bank entry
    if (!this->bankEntries.contains(bankId))
    {
        BankEntry entry;
        entry.row = new QWidget(this->widgetBanks);
        QHBoxLayout* rowLayout = new QHBoxLayout(entry.row);
        rowLayout->setContentsMargins(0, 1, 0, 1);
        rowLayout->setSpacing(4);

        int bbFS  = this->bigBoldMode ? 13 : 9;
        int bbW   = this->bigBoldMode ? 32 : 24;
        int bbH   = this->bigBoldMode ? 22 : 16;
        int biFS  = this->bigBoldMode ? 14 : 10;
        QSize bcSz = this->bigBoldMode ? QSize(42, 24) : QSize(30, 18);

        entry.labelBank = new QLabel(entry.row);
        entry.labelBank->setText(QString("B%1").arg(bankId));
        entry.labelBank->setStyleSheet(QString("background-color: rgba(255, 165, 0, 200); color: white; border-radius: 3px; font-size: %1px; font-weight: bold; padding: 1px 4px;").arg(bbFS));
        entry.labelBank->setFixedWidth(bbW);
        entry.labelBank->setFixedHeight(bbH);
        entry.labelBank->setAlignment(Qt::AlignCenter);
        rowLayout->addWidget(entry.labelBank, 0);

        entry.labelItem = new QLabel(entry.row);
        entry.labelItem->setStyleSheet(QString("font-size: %1px; color: rgba(200, 200, 200, 200);").arg(biFS));
        entry.labelItem->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        rowLayout->addWidget(entry.labelItem, 1);

        entry.labelChannel = new QLabel(entry.row);
        entry.labelChannel->setAlignment(Qt::AlignCenter);
        entry.labelChannel->setFixedSize(bcSz);
        rowLayout->addWidget(entry.labelChannel, 0);

        // Add to layout in sorted order by bank ID
        int insertIndex = 0;
        for (int i = 0; i < this->banksEntriesLayout->count(); i++)
        {
            QLayoutItem* layoutItem = this->banksEntriesLayout->itemAt(i);
            if (layoutItem == nullptr || layoutItem->widget() == nullptr)
                continue;

            QWidget* w = layoutItem->widget();
            for (auto it = this->bankEntries.begin(); it != this->bankEntries.end(); ++it)
            {
                if (it.value().row == w && it.key() < bankId)
                    insertIndex = i + 1;
            }
        }
        this->banksEntriesLayout->insertWidget(insertIndex, entry.row);
        this->bankEntries[bankId] = entry;
    }

    BankEntry& entry = this->bankEntries[bankId];
    entry.labelItem->setText(label);
    entry.labelChannel->setText(channelText);
    entry.labelChannel->setStyleSheet(ChannelBadge::style(channel, this->bigBoldMode));
}

void TriggerBanksPanelWidget::removeBankEntry(int bankId)
{
    if (!this->bankEntries.contains(bankId))
        return;

    BankEntry& entry = this->bankEntries[bankId];
    this->banksEntriesLayout->removeWidget(entry.row);
    delete entry.row;
    this->bankEntries.remove(bankId);
}

void TriggerBanksPanelWidget::rebuildAllBankEntries()
{
    // Remove all current bank entries and re-add from registry.
    QList<int> ids = this->bankEntries.keys();
    for (int id : ids)
        removeBankEntry(id);

    for (int bankId = 1; bankId <= BANK_COUNT; bankId++)
    {
        if (TriggerBankRegistry::getInstance().getItem(bankId) != nullptr)
            updateBankDisplay(bankId);
    }
    updateBankIcons();
}

#include "TriggerBanksPanelWidget.h"

#include "EventManager.h"
#include "TriggerBankRegistry.h"
#include "Rundown/AbstractRundownWidget.h"

#include <QtCore/QEvent>
#include <QtWidgets/QTreeWidget>

TriggerBanksPanelWidget::TriggerBanksPanelWidget(QWidget* parent)
    : QWidget(parent),
      contentCollapsed(false)
{
    setupUi(this);

    // Load big bold mode from DB.
    QString val = DatabaseManager::getInstance().getConfigurationByName("BigBoldMode").getValue();
    this->bigBoldMode = (val == "true");

    // Style and make header clickable.
    QString headerStyle = "font-weight: bold; font-size: 11px; color: rgba(200, 200, 200, 200); padding: 4px 4px 2px 4px;";
    this->labelHeader->setStyleSheet(headerStyle);
    this->labelHeader->setCursor(Qt::PointingHandCursor);
    this->labelHeader->installEventFilter(this);

    updateHeader();

    // Build bank rows (normal or big bold).
    buildBankRows();

    QObject::connect(&EventManager::getInstance(), SIGNAL(bankAssignmentChanged(const BankAssignmentChangedEvent&)),
                     this, SLOT(bankAssignmentChanged(const BankAssignmentChangedEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(bigBoldModeChanged(bool)),
                     this, SLOT(bigBoldModeChangedSlot(bool)));
}

void TriggerBanksPanelWidget::buildBankRows()
{
    // Clear the existing grid layout contents.
    while (QLayoutItem* item = this->gridLayoutBanks->takeAt(0))
    {
        delete item->widget();
        delete item;
    }

    this->gridLayoutBanks->setHorizontalSpacing(this->bigBoldMode ? 6 : 8);
    this->gridLayoutBanks->setVerticalSpacing(this->bigBoldMode ? 4 : 1);

    int badgeFS  = this->bigBoldMode ? 14 : 10;
    int badgeW   = this->bigBoldMode ? 32 : 26;
    int itemFS   = this->bigBoldMode ? 15 : 11;
    int chanFS   = this->bigBoldMode ? 13 : 11;
    int headerFS = this->bigBoldMode ? 13 : 10;

    if (!this->bigBoldMode)
    {
        // Normal mode: 3 columns, 1 row per bank, with column headers.
        QLabel* hBank = new QLabel("Bank", this);
        QLabel* hItem = new QLabel("Item", this);
        QLabel* hChan = new QLabel("Ch/Layer", this);
        QString hStyle = QString("font-weight: bold; font-size: %1px; color: rgba(200, 200, 200, 180);").arg(headerFS);
        hBank->setStyleSheet(hStyle);
        hItem->setStyleSheet(hStyle);
        hChan->setStyleSheet(hStyle);
        this->gridLayoutBanks->addWidget(hBank, 0, 0);
        this->gridLayoutBanks->addWidget(hItem, 0, 1);
        this->gridLayoutBanks->addWidget(hChan, 0, 2);

        for (int i = 0; i < TriggerBank::BANK_COUNT; i++)
        {
            int bankId = i + 1;
            int row = i + 1;

            this->bankLabels[i] = new QLabel(QString("B%1").arg(bankId), this);
            this->bankLabels[i]->setStyleSheet(QString("background-color: rgba(255, 165, 0, 200); color: white; border-radius: 3px; font-size: %1px; font-weight: bold; padding: 1px 3px;").arg(badgeFS));
            this->bankLabels[i]->setFixedWidth(badgeW);
            this->bankLabels[i]->setAlignment(Qt::AlignCenter);

            this->itemLabels[i] = new QLabel(QString::fromUtf8("\xe2\x80\x94"), this);
            this->itemLabels[i]->setStyleSheet(QString("font-size: %1px; color: rgba(180, 180, 180, 180);").arg(itemFS));
            this->itemLabels[i]->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

            this->channelLabels[i] = new QLabel("", this);
            this->channelLabels[i]->setStyleSheet(QString("font-size: %1px; color: rgba(180, 180, 180, 180);").arg(chanFS));
            this->channelLabels[i]->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

            this->gridLayoutBanks->addWidget(this->bankLabels[i], row, 0);
            this->gridLayoutBanks->addWidget(this->itemLabels[i], row, 1);
            this->gridLayoutBanks->addWidget(this->channelLabels[i], row, 2);
        }

        this->gridLayoutBanks->setColumnStretch(0, 0);
        this->gridLayoutBanks->setColumnStretch(1, 1);
        this->gridLayoutBanks->setColumnStretch(2, 0);
    }
    else
    {
        // Big & Bold mode: 2 rows per bank.
        //   Row 2*i:   [B_badge] [item name]
        //   Row 2*i+1: [empty]   [ch/layer]
        for (int i = 0; i < TriggerBank::BANK_COUNT; i++)
        {
            int bankId = i + 1;
            int r0 = i * 2;
            int r1 = i * 2 + 1;

            this->bankLabels[i] = new QLabel(QString("B%1").arg(bankId), this);
            this->bankLabels[i]->setStyleSheet(QString("background-color: rgba(255, 165, 0, 200); color: white; border-radius: 3px; font-size: %1px; font-weight: bold; padding: 2px 4px;").arg(badgeFS));
            this->bankLabels[i]->setFixedWidth(badgeW);
            this->bankLabels[i]->setAlignment(Qt::AlignCenter);

            this->itemLabels[i] = new QLabel(QString::fromUtf8("\xe2\x80\x94"), this);
            this->itemLabels[i]->setStyleSheet(QString("font-size: %1px; font-weight: bold; color: rgba(180, 180, 180, 180);").arg(itemFS));
            this->itemLabels[i]->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

            this->channelLabels[i] = new QLabel("", this);
            this->channelLabels[i]->setStyleSheet(QString("font-size: %1px; color: rgba(150, 150, 150, 200);").arg(chanFS));
            this->channelLabels[i]->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

            this->gridLayoutBanks->addWidget(this->bankLabels[i], r0, 0);
            this->gridLayoutBanks->addWidget(this->itemLabels[i], r0, 1);
            this->gridLayoutBanks->addWidget(this->channelLabels[i], r1, 1);
        }

        this->gridLayoutBanks->setColumnStretch(0, 0);
        this->gridLayoutBanks->setColumnStretch(1, 1);
    }

    // Refresh content from registry.
    for (int bankId = 1; bankId <= TriggerBank::BANK_COUNT; bankId++)
        updateBankRow(bankId);
}

bool TriggerBanksPanelWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress)
    {
        if (obj == this->labelHeader)
        {
            toggleCollapse();
            return true;
        }
    }

    return QWidget::eventFilter(obj, event);
}

void TriggerBanksPanelWidget::updateHeader()
{
    this->labelHeader->setText(QString("%1 %2")
        .arg(this->contentCollapsed ? QString::fromUtf8("\xe2\x96\xb6") : QString::fromUtf8("\xe2\x96\xbc"))
        .arg("Trigger Banks"));
}

void TriggerBanksPanelWidget::toggleCollapse()
{
    this->contentCollapsed = !this->contentCollapsed;
    this->widgetContent->setVisible(!this->contentCollapsed);
    updateHeader();
}

void TriggerBanksPanelWidget::bankAssignmentChanged(const BankAssignmentChangedEvent& event)
{
    updateBankRow(event.getBankId());
}

void TriggerBanksPanelWidget::updateBankRow(int bankId)
{
    if (bankId < 1 || bankId > TriggerBank::BANK_COUNT)
        return;

    int index = bankId - 1;
    QTreeWidgetItem* item = TriggerBankRegistry::getInstance().getItem(bankId);

    if (item == nullptr)
    {
        this->itemLabels[index]->setText(QString::fromUtf8("\xe2\x80\x94"));
        this->itemLabels[index]->setStyleSheet(QString("font-size: %1px; color: rgba(180, 180, 180, 180);").arg(this->bigBoldMode ? 15 : 11));
        this->channelLabels[index]->setText("");
        return;
    }

    QTreeWidget* treeWidget = item->treeWidget();
    if (treeWidget == nullptr)
        return;

    QWidget* widget = treeWidget->itemWidget(item, 0);
    if (widget == nullptr)
        return;

    AbstractRundownWidget* rundownWidget = dynamic_cast<AbstractRundownWidget*>(widget);
    if (rundownWidget == nullptr)
        return;

    QString label = rundownWidget->getLibraryModel()->getLabel();
    if (label.isEmpty())
        label = rundownWidget->getLibraryModel()->getName();
    if (label.isEmpty())
        label = rundownWidget->getLibraryModel()->getType();

    this->itemLabels[index]->setText(label);
    this->itemLabels[index]->setStyleSheet(QString("font-size: %1px; %2color: white;")
        .arg(this->bigBoldMode ? 15 : 11)
        .arg(this->bigBoldMode ? "font-weight: bold; " : ""));

    if (rundownWidget->isGroup())
    {
        QLabel* badge = widget->findChild<QLabel*>("labelColor");
        if (badge != nullptr)
            this->channelLabels[index]->setText(badge->text());
    }
    else
    {
        int channel = rundownWidget->getCommand()->getChannel();
        int videolayer = rundownWidget->getCommand()->getVideolayer();
        this->channelLabels[index]->setText(QString("%1-%2").arg(channel).arg(videolayer));
    }
}

void TriggerBanksPanelWidget::bigBoldModeChangedSlot(bool active)
{
    this->bigBoldMode = active;
    buildBankRows();
}

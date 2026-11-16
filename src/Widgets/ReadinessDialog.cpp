#include "ReadinessDialog.h"

#include "DeviceManager.h"
#include "SheetDataResolver.h"
#include "SheetsProjectRegistry.h"
#include "Rundown/AbstractRundownWidget.h"
#include "Rundown/MissingMediaScanner.h"
#include "Rundown/RundownTreeBaseWidget.h"

#include "Commands/AbstractCommand.h"
#include "Commands/TemplateCommand.h"
#include "Models/LibraryModel.h"

#include <QtCore/QDateTime>

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>

namespace
{
    const int ITEM_ROLE = Qt::UserRole + 1;
    const int SHEET_WAIT_MS = 20000;

    QString kindText(ReadinessRules::Kind kind)
    {
        switch (kind)
        {
            case ReadinessRules::Kind::Server: return "Server";
            case ReadinessRules::Kind::Locked: return "Locked channel";
            case ReadinessRules::Kind::Media: return "Media";
            default: return "Sheet";
        }
    }
}

void ReadinessDialog::openFor(RundownTreeBaseWidget* tree, QWidget* parent)
{
    ReadinessDialog* dialog = new ReadinessDialog(tree, parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

ReadinessDialog::ReadinessDialog(RundownTreeBaseWidget* tree, QWidget* parent)
    : QDialog(parent), tree(tree)
{
    setWindowTitle("Check Before Going Live");
    resize(760, 420);

    QVBoxLayout* layout = new QVBoxLayout(this);

    this->summaryLabel = new QLabel(this);
    this->summaryLabel->setWordWrap(true);
    QFont font = this->summaryLabel->font();
    font.setBold(true);
    this->summaryLabel->setFont(font);
    layout->addWidget(this->summaryLabel);

    this->list = new QTreeWidget(this);
    this->list->setColumnCount(3);
    this->list->setHeaderLabels(QStringList() << "What" << "Item" << "Why");
    this->list->setRootIsDecorated(false);
    this->list->setWordWrap(true);
    this->list->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    this->list->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    this->list->header()->setStretchLastSection(true);
    QObject::connect(this->list, &QTreeWidget::itemDoubleClicked, this, [this]() { showSelected(); });
    QObject::connect(this->list, &QTreeWidget::currentItemChanged, this, [this]() {
        const QTreeWidgetItem* current = this->list->currentItem();
        this->buttonShow->setEnabled(current != nullptr && current->data(0, ITEM_ROLE).toInt() >= 0);
    });
    layout->addWidget(this->list, 1);

    QHBoxLayout* buttons = new QHBoxLayout();
    this->buttonShow = new QPushButton("Show", this);
    this->buttonShow->setEnabled(false);
    this->buttonShow->setToolTip("Select the item in the rundown");
    QObject::connect(this->buttonShow, &QPushButton::clicked, this, [this]() { showSelected(); });
    buttons->addWidget(this->buttonShow);
    this->buttonCheck = new QPushButton("Check Again", this);
    QObject::connect(this->buttonCheck, &QPushButton::clicked, this, [this]() { runChecks(); });
    buttons->addWidget(this->buttonCheck);
    buttons->addStretch(1);
    QPushButton* close = new QPushButton("Close", this);
    QObject::connect(close, &QPushButton::clicked, this, &QDialog::reject);
    buttons->addWidget(close);
    layout->addLayout(buttons);

    QObject::connect(&SheetDataResolver::getInstance(), &SheetDataResolver::rowsReady, this,
                     [this](const QString& requestId, const QList<SheetRow>&, const SheetRowsOrigin&) { sheetAnswered(requestId, QString()); });
    QObject::connect(&SheetDataResolver::getInstance(), &SheetDataResolver::rowsFailed, this,
                     [this](const QString& requestId, const QString& reason) {
                         sheetAnswered(requestId, reason.isEmpty() ? QString("it did not answer") : reason);
                     });

    this->sheetTimeout.setSingleShot(true);
    QObject::connect(&this->sheetTimeout, &QTimer::timeout, this, [this]() {
        const QStringList waiting = this->pendingSheets.keys();
        for (const QString& requestId : waiting)
            sheetAnswered(requestId, QString("no answer within %1 seconds").arg(SHEET_WAIT_MS / 1000));
    });

    runChecks();
}

void ReadinessDialog::runChecks()
{
    this->rows.clear();
    this->items.clear();
    this->findings.clear();
    this->pendingSheets.clear();
    this->notChecked.clear();

    if (this->tree.isNull())
    {
        this->summaryLabel->setText("The rundown is gone - open one and check again.");
        refresh();
        return;
    }

    // Every row, groups included all the way down.
    QList<QTreeWidgetItem*> pending;
    for (int i = 0; i < this->tree->topLevelItemCount(); i++)
        pending << this->tree->topLevelItem(i);
    while (!pending.isEmpty())
    {
        QTreeWidgetItem* item = pending.takeFirst();
        for (int c = 0; c < item->childCount(); c++)
            pending << item->child(c);

        QWidget* widget = this->tree->itemWidget(item, 0);
        AbstractRundownWidget* rundownItem = dynamic_cast<AbstractRundownWidget*>(widget);
        if (rundownItem == nullptr || rundownItem->getLibraryModel() == nullptr || rundownItem->getCommand() == nullptr)
            continue;

        ReadinessRules::Item record;
        record.id = this->rows.count();
        LibraryModel* model = rundownItem->getLibraryModel();
        record.label = model->getLabel().isEmpty() ? model->getName() : model->getLabel();
        record.device = model->getDeviceName();
        record.channel = rundownItem->getCommand()->getChannel();
        record.disabled = rundownItem->getCommand()->getDisabled();

        this->rows << widget;
        this->items << record;
    }

    // Servers and locked channels.
    this->findings = ReadinessRules::check(this->items, [](const QString& name) {
        ReadinessRules::Server server;
        const QSharedPointer<DeviceModel> model = DeviceManager::getInstance().getDeviceModelByName(name);
        if (model == nullptr)
            return server;
        server.known = true;
        const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(name);
        server.connected = device != nullptr && device->isConnected();
        for (int channel = 1; channel <= qMax(model->getChannels(), 8); channel++)
        {
            if (DeviceManager::getInstance().isChannelLocked(name, channel))
                server.lockedChannels.insert(channel);
        }
        return server;
    });

    // Media and templates: the row warnings' own check.
    if (MissingMediaScanner::isEnabled())
    {
        MissingMediaScanner scanner;
        const MissingMediaScanner::Result result = scanner.scan(this->tree);
        for (const QPair<QWidget*, QString>& missing : result.missingRows)
        {
            int id = -1;
            for (int i = 0; i < this->rows.count(); i++)
            {
                if (this->rows.at(i).data() == missing.first)
                    id = i;
            }
            if (id >= 0 && this->items.at(id).disabled)
                continue;
            this->findings << ReadinessRules::Finding{ReadinessRules::Kind::Media, id, missing.second};
        }
    }
    else
    {
        this->notChecked = "Media and templates were not checked: the missing-media check is off in Settings.";
    }

    checkSheets();
    refresh();
}

void ReadinessDialog::checkSheets()
{
    // One read per sheet tab, however many items use it.
    QMap<QString, QPair<SheetsProject, QString>> reads;
    QMap<QString, PendingSheet> waiting;
    bool discovered = false;

    for (int id = 0; id < this->rows.count(); id++)
    {
        if (this->items.at(id).disabled)
            continue;
        AbstractRundownWidget* rundownItem = dynamic_cast<AbstractRundownWidget*>(this->rows.at(id).data());
        TemplateCommand* command = rundownItem != nullptr ? dynamic_cast<TemplateCommand*>(rundownItem->getCommand()) : nullptr;
        if (command == nullptr || command->getTemplateName().isEmpty())
            continue;

        const TemplateSheetConnection connection =
            SheetDataResolver::getInstance().connectionFor(this->items.at(id).device, command->getTemplateName());
        if (!connection.declared)
            continue;

        if (!connection.isValid())
        {
            this->findings << ReadinessRules::Finding{ReadinessRules::Kind::Sheet, id,
                QString("Its template %1 declares a sheet connection with no tab, so it reads nothing.").arg(command->getTemplateName())};
            continue;
        }

        SheetsProject project = SheetsProjectRegistry::getInstance().projectForTemplate(command->getTemplateName());
        if (!project.isValid() && !discovered)
        {
            SheetsProjectRegistry::getInstance().discover();
            discovered = true;
            project = SheetsProjectRegistry::getInstance().projectForTemplate(command->getTemplateName());
        }
        if (!project.isValid())
        {
            this->findings << ReadinessRules::Finding{ReadinessRules::Kind::Sheet, id,
                QString("Its template %1 reads a sheet, but no sheet project is found for it.").arg(command->getTemplateName())};
            continue;
        }

        const QString key = project.spreadsheetId + "|" + connection.tab;
        if (!reads.contains(key))
        {
            reads.insert(key, qMakePair(project, connection.tab));
            waiting[key].what = QString("tab %1 of %2").arg(connection.tab, project.name);
        }
        waiting[key].itemIds << id;
    }

    const QString stamp = QString::number(QDateTime::currentMSecsSinceEpoch());
    for (auto it = reads.constBegin(); it != reads.constEnd(); ++it)
    {
        const QString requestId = QString("readiness|%1|%2").arg(stamp, it.key());
        this->pendingSheets.insert(requestId, waiting.value(it.key()));
        SheetDataResolver::getInstance().fetchRows(it.value().first, it.value().second, requestId, false);
    }

    if (!this->pendingSheets.isEmpty())
        this->sheetTimeout.start(SHEET_WAIT_MS);
}

void ReadinessDialog::sheetAnswered(const QString& requestId, const QString& failure)
{
    if (!this->pendingSheets.contains(requestId))
        return;

    const PendingSheet sheet = this->pendingSheets.take(requestId);
    if (!failure.isEmpty() && !sheet.itemIds.isEmpty())
    {
        const int count = sheet.itemIds.count();
        this->findings << ReadinessRules::Finding{ReadinessRules::Kind::Sheet, sheet.itemIds.first(),
            QString("The sheet (%1) did not answer: %2. %3.").arg(sheet.what, failure,
                count == 1 ? QString("1 item reads it") : QString("%1 items read it").arg(count))};
    }

    if (this->pendingSheets.isEmpty())
        this->sheetTimeout.stop();
    refresh();
}

void ReadinessDialog::refresh()
{
    this->list->clear();

    for (const ReadinessRules::Finding& finding : this->findings)
    {
        const QString label = finding.item >= 0 && finding.item < this->items.count() ? this->items.at(finding.item).label : QString();
        QTreeWidgetItem* row = new QTreeWidgetItem(this->list, QStringList() << kindText(finding.kind) << label << finding.text);
        row->setData(0, ITEM_ROLE, finding.item);
        row->setToolTip(2, finding.text);
    }

    QString text = this->tree.isNull() ? QString("The rundown is gone.") : ReadinessRules::summary(this->findings);
    if (!this->pendingSheets.isEmpty())
        text += QString(" Still reading %1 sheet source%2...").arg(this->pendingSheets.count()).arg(this->pendingSheets.count() == 1 ? "" : "s");
    if (!this->notChecked.isEmpty())
        text += "\n" + this->notChecked;

    this->summaryLabel->setText(text);
    this->summaryLabel->setStyleSheet(this->findings.isEmpty() && this->pendingSheets.isEmpty()
        ? QString("color: rgb(110, 200, 120);") : QString("color: rgb(235, 170, 80);"));
    this->buttonShow->setEnabled(false);
}

void ReadinessDialog::showSelected()
{
    const QTreeWidgetItem* current = this->list->currentItem();
    if (current == nullptr || this->tree.isNull())
        return;

    const int id = current->data(0, ITEM_ROLE).toInt();
    if (id < 0 || id >= this->rows.count() || this->rows.at(id).isNull())
        return;

    // The row is found again by its widget: the rundown may have changed since.
    QList<QTreeWidgetItem*> pending;
    for (int i = 0; i < this->tree->topLevelItemCount(); i++)
        pending << this->tree->topLevelItem(i);
    while (!pending.isEmpty())
    {
        QTreeWidgetItem* item = pending.takeFirst();
        if (this->tree->itemWidget(item, 0) == this->rows.at(id).data())
        {
            this->tree->clearSelection();
            this->tree->setCurrentItem(item);
            this->tree->scrollToItem(item);
            this->tree->window()->activateWindow();
            return;
        }
        for (int c = 0; c < item->childCount(); c++)
            pending << item->child(c);
    }
}

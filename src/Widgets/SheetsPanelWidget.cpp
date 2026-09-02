#include "SheetsPanelWidget.h"
#include "SheetsProjectRegistry.h"
#include "SheetDataResolver.h"
#include "SheetsActionsDialog.h"

#include "Global.h"
#include "PanelHelper.h"
#include "DatabaseManager.h"
#include "DeviceManager.h"
#include "EventManager.h"
#include "HttpResponseLog.h"
#include "Xml.h"
#include "Events/Rundown/ChannelActivityEvent.h"
#include "Models/ConfigurationModel.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QRegularExpression>
#include <QtCore/QTextStream>
#include <QtCore/QTime>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>

namespace
{
    const int DEFAULT_POLL_SECONDS = 30;
    const int TEMPLATE_FLASHLAYER = 1;   // Flash is history; templates always use layer 1.

    // Build a readable failure description from a finished reply: HTTP status plus
    // Google's own error message (its JSON body says exactly what is wrong), and
    // log the exchange to the Http Log panel with the API key redacted.
    QString describeSheetError(QNetworkReply* reply)
    {
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QString body = QString::fromUtf8(reply->readAll());

        QString redactedUrl = reply->url().toString();
        redactedUrl.replace(QRegularExpression("key=[^&]+"), "key=REDACTED");
        HttpResponseLog::getInstance().logResponse("GET", redactedUrl, status, body);

        QString googleMessage = QJsonDocument::fromJson(body.toUtf8())
            .object()["error"].toObject()["message"].toString();

        if (status > 0 && !googleMessage.isEmpty())
            return QString("HTTP %1: %2").arg(status).arg(googleMessage);
        if (status > 0)
            return QString("HTTP %1: %2").arg(status).arg(reply->errorString());
        return reply->errorString(); // network-level: DNS, TLS, offline...
    }
}

QJsonObject SheetsActionDef::toJson() const
{
    QJsonObject obj;
    obj["label"] = this->label;
    obj["color"] = this->color;
    obj["template"] = this->templateName;
    obj["channel"] = this->channel;
    obj["layer"] = this->videolayer;
    obj["data"] = this->data;
    obj["action"] = this->action;
    return obj;
}

SheetsActionDef SheetsActionDef::fromJson(const QJsonObject& obj)
{
    SheetsActionDef def;
    def.label = obj["label"].toString();
    def.color = obj["color"].toString();
    def.templateName = obj["template"].toString();
    def.channel = obj["channel"].toInt(1);
    def.videolayer = obj["layer"].toInt(20);
    def.data = obj["data"].toString();
    def.action = obj["action"].toString("play");
    return def;
}

SheetsPanelWidget::SheetsPanelWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi(this);
    setupMenus();

    this->networkManager = new QNetworkAccessManager(this);

    this->collapsed = PanelHelper::isPanelCollapsed("Sheets");
    if (this->collapsed)
        this->expandCollapseAction->setText("Expand");

    this->setFixedHeight(Panel::DEFAULT_SHEETS_HEIGHT);

    this->widgetStandalone->setVisible(false);
    this->treeWidgetData->header()->setStretchLastSection(true);

    QObject::connect(this->comboBoxProject, SIGNAL(currentIndexChanged(int)), this, SLOT(projectChanged(int)));
    QObject::connect(this->comboBoxTab, SIGNAL(currentIndexChanged(int)), this, SLOT(tabChanged(int)));
    QObject::connect(this->pushButtonRefresh, SIGNAL(clicked()), this, SLOT(refresh()));

    // Auto-refresh interval selector; 0 = off.
    this->comboBoxPoll->addItem("10s", 10);
    this->comboBoxPoll->addItem("30s", 30);
    this->comboBoxPoll->addItem("1m", 60);
    this->comboBoxPoll->addItem("2m", 120);
    this->comboBoxPoll->addItem("5m", 300);
    this->comboBoxPoll->addItem("Off", 0);

    int pollSeconds = DEFAULT_POLL_SECONDS;
    QString pollValue = DatabaseManager::getInstance().getConfigurationByName("SheetsPollSeconds").getValue();
    if (!pollValue.isEmpty())
        pollSeconds = pollValue.toInt();
    int pollIndex = this->comboBoxPoll->findData(pollSeconds);
    this->comboBoxPoll->setCurrentIndex(pollIndex >= 0 ? pollIndex : 1);

    QObject::connect(&this->pollTimer, &QTimer::timeout, this, [this]() { fetchValues(); });
    QObject::connect(this->comboBoxPoll, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        int seconds = this->comboBoxPoll->currentData().toInt();
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "SheetsPollSeconds", QString::number(seconds)));
        applyPollInterval();
    });
    applyPollInterval();

    discoverProjects();
}

void SheetsPanelWidget::setupMenus()
{
    this->dropdownMenu = new QMenu(this);
    this->dropdownMenu->setObjectName("panelMenu");
    PanelHelper::addMoveActions(this->dropdownMenu, "Sheets", this);
    this->dropdownMenu->addSeparator();
    this->dropdownMenu->addAction("Refresh Projects", this, [this]() { discoverProjects(); });
    this->dropdownMenu->addAction("Edit Actions...", this, SLOT(editActions()));

    // Per-tab column visibility — rebuilt each time the menu opens because the
    // column list belongs to whichever sheet tab is currently selected.
    this->columnsMenu = this->dropdownMenu->addMenu("Columns");
    QObject::connect(this->dropdownMenu, &QMenu::aboutToShow, this, [this]() { buildColumnsMenu(); });
    this->dropdownMenu->addSeparator();
    this->expandCollapseAction = this->dropdownMenu->addAction("Collapse", this, SLOT(toggleExpandCollapse()));

    this->menuButton = new QToolButton(this->tabWidgetSheets);
    this->menuButton->setText(QString::fromUtf8("\xe2\x89\xa1"));
    this->menuButton->setFixedSize(22, 22);
    this->menuButton->setMenu(this->dropdownMenu);
    this->menuButton->setPopupMode(QToolButton::InstantPopup);
    this->tabWidgetSheets->setCornerWidget(this->menuButton);
}

void SheetsPanelWidget::discoverProjects()
{
    // Discovery lives in the registry so the panel and the template data resolver
    // agree on what a project is and where it came from.
    SheetsProjectRegistry::getInstance().discover();

    this->projects = SheetsProjectRegistry::getInstance().projects();

    this->comboBoxProject->blockSignals(true);
    this->comboBoxProject->clear();

    foreach (const SheetsProject& project, this->projects)
        this->comboBoxProject->addItem(project.name);

    // Restore last selected project.
    QString last = DatabaseManager::getInstance().getConfigurationByName("SheetsPanelProject").getValue();
    int lastIndex = this->comboBoxProject->findText(last);
    if (lastIndex >= 0)
        this->comboBoxProject->setCurrentIndex(lastIndex);

    this->comboBoxProject->blockSignals(false);

    if (this->projects.isEmpty())
        setStatus("No project.js found in any device template path");
    else
        projectChanged(this->comboBoxProject->currentIndex());
}

SheetsProject* SheetsPanelWidget::currentProject()
{
    int index = this->comboBoxProject->currentIndex();
    if (index < 0 || index >= this->projects.count())
        return nullptr;
    return &this->projects[index];
}

QString SheetsPanelWidget::currentTab() const
{
    return this->comboBoxTab->currentText();
}

void SheetsPanelWidget::projectChanged(int index)
{
    Q_UNUSED(index);

    SheetsProject* project = currentProject();
    if (project == nullptr)
        return;

    DatabaseManager::getInstance().updateConfiguration(
        ConfigurationModel(0, "SheetsPanelProject", project->name));

    loadExtensions();
    fetchTabs();
}

void SheetsPanelWidget::tabChanged(int index)
{
    Q_UNUSED(index);

    if (currentTab().isEmpty())
        return;

    DatabaseManager::getInstance().updateConfiguration(
        ConfigurationModel(0, "SheetsPanelTab", currentTab()));

    fetchValues();
}

void SheetsPanelWidget::refresh()
{
    fetchTabs();
}

/* ---------------- extensions.json ---------------- */

void SheetsPanelWidget::loadExtensions()
{
    this->tabActions.clear();

    SheetsProject* project = currentProject();
    if (project == nullptr)
        return;

    QFile file(QDir(project->folder).filePath("extensions.json"));
    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    QJsonObject tabs = doc.object()["tabs"].toObject();
    foreach (const QString& tabName, tabs.keys())
    {
        QJsonObject tabObj = tabs[tabName].toObject();
        SheetsTabActions actions;
        foreach (const QJsonValue& value, tabObj["rowButtons"].toArray())
            actions.rowButtons.append(SheetsActionDef::fromJson(value.toObject()));
        foreach (const QJsonValue& value, tabObj["standalone"].toArray())
            actions.standalone.append(SheetsActionDef::fromJson(value.toObject()));
        this->tabActions[tabName] = actions;
    }
}

void SheetsPanelWidget::saveExtensions()
{
    SheetsProject* project = currentProject();
    if (project == nullptr)
        return;

    QJsonObject tabs;
    foreach (const QString& tabName, this->tabActions.keys())
    {
        const SheetsTabActions& actions = this->tabActions[tabName];
        if (actions.rowButtons.isEmpty() && actions.standalone.isEmpty())
            continue;

        QJsonArray rowArray;
        foreach (const SheetsActionDef& def, actions.rowButtons)
            rowArray.append(def.toJson());
        QJsonArray standaloneArray;
        foreach (const SheetsActionDef& def, actions.standalone)
            standaloneArray.append(def.toJson());

        QJsonObject tabObj;
        tabObj["rowButtons"] = rowArray;
        tabObj["standalone"] = standaloneArray;
        tabs[tabName] = tabObj;
    }

    QJsonObject root;
    root["version"] = 1;
    root["tabs"] = tabs;

    QFile file(QDir(project->folder).filePath("extensions.json"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        setStatus("Could not write extensions.json");
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
}

/* ---------------- fetching ---------------- */

void SheetsPanelWidget::fetchTabs()
{
    SheetsProject* project = currentProject();
    if (project == nullptr)
        return;

    setStatus("Fetching tabs...");

    // Straight to the API, so it comes out of the same budget everything else does.
    SheetDataResolver::getInstance().noteClientRead(project->spreadsheetId);

    QString url = QString("https://sheets.googleapis.com/v4/spreadsheets/%1?fields=sheets.properties&key=%2")
        .arg(project->spreadsheetId, project->apiKey);

    QNetworkRequest request((QUrl(url)));
    request.setHeader(QNetworkRequest::UserAgentHeader, "CasparCG-Client");
    QNetworkReply* reply = this->networkManager->get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
        {
            setStatus("Tabs failed \xe2\x80\x94 " + describeSheetError(reply));
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QStringList tabNames;
        foreach (const QJsonValue& sheet, doc.object()["sheets"].toArray())
            tabNames.append(sheet.toObject()["properties"].toObject()["title"].toString());

        this->comboBoxTab->blockSignals(true);
        QString previous = this->comboBoxTab->currentText();
        if (previous.isEmpty())
            previous = DatabaseManager::getInstance().getConfigurationByName("SheetsPanelTab").getValue();
        this->comboBoxTab->clear();
        this->comboBoxTab->addItems(tabNames);
        int previousIndex = this->comboBoxTab->findText(previous);
        if (previousIndex >= 0)
            this->comboBoxTab->setCurrentIndex(previousIndex);
        this->comboBoxTab->blockSignals(false);

        fetchValues();
    });
}

void SheetsPanelWidget::fetchValues()
{
    SheetsProject* project = currentProject();
    if (project == nullptr || currentTab().isEmpty())
        return;

    SheetDataResolver::getInstance().noteClientRead(project->spreadsheetId);

    QString url = QString("https://sheets.googleapis.com/v4/spreadsheets/%1/values/%2?key=%3")
        .arg(project->spreadsheetId, QString(QUrl::toPercentEncoding(currentTab())), project->apiKey);

    // The panel polls, so this is the most regular read the client makes. Keeping it
    // costs nothing extra: the call to Google is already paid for.
    SheetsProject warmProject = *project;
    QString warmTab = currentTab();

    QNetworkRequest request((QUrl(url)));
    request.setHeader(QNetworkRequest::UserAgentHeader, "CasparCG-Client");
    QNetworkReply* reply = this->networkManager->get(request);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, warmProject, warmTab]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
        {
            setStatus("Fetch failed \xe2\x80\x94 " + describeSheetError(reply));
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonArray values = doc.object()["values"].toArray();

        this->headers.clear();
        this->rows.clear();

        for (int i = 0; i < values.count(); i++)
        {
            QStringList row;
            foreach (const QJsonValue& cell, values[i].toArray())
                row.append(cell.toString());

            if (i == 0)
                this->headers = row;
            else
                this->rows.append(row);
        }

        // Shaped the way the cache holds rows, so templates and the inspector get the
        // benefit of a read the panel was making anyway.
        QList<SheetRow> cacheRows;
        foreach (const QStringList& row, this->rows)
        {
            SheetRow entry;
            for (int column = 0; column < this->headers.count(); column++)
                entry.insert(this->headers.at(column), (column < row.count()) ? row.at(column) : QString());

            if (!entry.isEmpty())
                cacheRows.append(entry);
        }

        if (!cacheRows.isEmpty())
            SheetDataResolver::getInstance().warmCache(warmProject, warmTab, cacheRows);

        renderData();
        renderStandaloneButtons();
        setStatus(QString("Fetched %1 \xc2\xb7 %2").arg(currentTab(), QTime::currentTime().toString("HH:mm:ss")));

        applyPollInterval();
    });
}

void SheetsPanelWidget::clearLitTogglesForTarget(const QString& target, const QString& exceptKey)
{
    bool changed = false;
    QList<QString> keys = this->litToggleTargets.keys();
    foreach (const QString& key, keys)
    {
        if (this->litToggleTargets[key] == target && key != exceptKey)
        {
            this->litToggles.remove(key);
            this->litToggleTargets.remove(key);
            changed = true;
        }
    }
    if (changed)
        rebuildButtonsDeferred();
}

void SheetsPanelWidget::rebuildButtonsDeferred()
{
    // Deferred: rebuilding synchronously would delete the button whose clicked()
    // handler we are currently inside.
    QTimer::singleShot(0, this, [this]() {
        renderData();
        renderStandaloneButtons();
    });
}

void SheetsPanelWidget::applyPollInterval()
{
    int seconds = this->comboBoxPoll->currentData().toInt();
    if (seconds > 0)
    {
        this->pollTimer.setInterval(seconds * 1000);
        this->pollTimer.start();
    }
    else
    {
        this->pollTimer.stop();
    }
}

/* ---------------- column visibility ---------------- */

// Hidden columns are stored per project + tab, by column NAME rather than index,
// so inserting a column in the sheet doesn't hide the wrong one.
QString SheetsPanelWidget::hiddenColumnsKey() const
{
    return QString("SheetsHiddenCols_%1|%2").arg(this->comboBoxProject->currentText(), currentTab());
}

QSet<QString> SheetsPanelWidget::hiddenColumns() const
{
    if (currentTab().isEmpty())
        return QSet<QString>();

    QString value = DatabaseManager::getInstance().getConfigurationByName(hiddenColumnsKey()).getValue();
    QStringList names = value.split("\t", Qt::SkipEmptyParts);
    return QSet<QString>(names.begin(), names.end());
}

void SheetsPanelWidget::setColumnHiddenState(const QString& columnName, bool hidden)
{
    QSet<QString> current = hiddenColumns();
    if (hidden)
        current.insert(columnName);
    else
        current.remove(columnName);

    QStringList names(current.begin(), current.end());
    DatabaseManager::getInstance().updateConfiguration(
        ConfigurationModel(0, hiddenColumnsKey(), names.join("\t")));

    renderData();
}

void SheetsPanelWidget::buildColumnsMenu()
{
    if (this->columnsMenu == nullptr)
        return;

    this->columnsMenu->clear();

    if (this->headers.isEmpty())
    {
        QAction* none = this->columnsMenu->addAction("(No columns loaded)");
        none->setEnabled(false);
        return;
    }

    QSet<QString> hidden = hiddenColumns();
    for (const QString& header : this->headers)
    {
        if (header.trimmed().isEmpty())
            continue;

        QAction* action = this->columnsMenu->addAction(header);
        action->setCheckable(true);
        action->setChecked(!hidden.contains(header));
        QObject::connect(action, &QAction::triggered, this, [this, header](bool checked) {
            setColumnHiddenState(header, !checked);
        });
    }

    this->columnsMenu->addSeparator();
    this->columnsMenu->addAction("Show All", this, [this]() {
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, hiddenColumnsKey(), ""));
        renderData();
    });
}

/* ---------------- rendering ---------------- */

void SheetsPanelWidget::renderData()
{
    this->treeWidgetData->clear();

    bool hasRowButtons = !this->tabActions.value(currentTab()).rowButtons.isEmpty();

    QStringList columnLabels = this->headers;
    if (hasRowButtons)
        columnLabels.append("");

    this->treeWidgetData->setColumnCount(columnLabels.count());
    this->treeWidgetData->setHeaderLabels(columnLabels);

    for (int rowIndex = 0; rowIndex < this->rows.count(); rowIndex++)
    {
        QTreeWidgetItem* item = new QTreeWidgetItem();
        for (int col = 0; col < this->rows[rowIndex].count() && col < this->headers.count(); col++)
            item->setText(col, this->rows[rowIndex][col]);
        this->treeWidgetData->addTopLevelItem(item);

        if (hasRowButtons)
        {
            QWidget* buttons = buildRowButtons(rowIndex);
            if (buttons != nullptr)
                this->treeWidgetData->setItemWidget(item, columnLabels.count() - 1, buttons);
        }
    }

    // Apply per-tab column visibility (indices stay aligned with headers, so
    // placeholder resolution and button data are unaffected by hiding).
    QSet<QString> hidden = hiddenColumns();
    for (int col = 0; col < this->headers.count(); col++)
        this->treeWidgetData->setColumnHidden(col, hidden.contains(this->headers[col]));

    for (int col = 0; col < this->headers.count(); col++)
        if (!this->treeWidgetData->isColumnHidden(col))
            this->treeWidgetData->resizeColumnToContents(col);
}

QWidget* SheetsPanelWidget::buildRowButtons(int rowIndex)
{
    const SheetsTabActions actions = this->tabActions.value(currentTab());
    if (actions.rowButtons.isEmpty())
        return nullptr;

    QWidget* container = new QWidget(this->treeWidgetData);
    QHBoxLayout* layout = new QHBoxLayout(container);
    layout->setContentsMargins(2, 1, 2, 1);
    layout->setSpacing(3);

    bool anyButton = false;
    for (int buttonIndex = 0; buttonIndex < actions.rowButtons.count(); buttonIndex++)
    {
        const SheetsActionDef& def = actions.rowButtons[buttonIndex];

        // Skip rows where every referenced placeholder resolves to empty
        // (set-header rows, spacer rows).
        QRegularExpression placeholderRegex("\\{([^}]+)\\}");
        QRegularExpressionMatchIterator it = placeholderRegex.globalMatch(def.data + def.label);
        bool hasPlaceholders = false;
        bool anyResolved = false;
        while (it.hasNext())
        {
            QRegularExpressionMatch match = it.next();
            hasPlaceholders = true;
            int col = this->headers.indexOf(match.captured(1));
            if (col >= 0 && col < this->rows[rowIndex].count() && !this->rows[rowIndex][col].trimmed().isEmpty())
                anyResolved = true;
        }
        if (hasPlaceholders && !anyResolved)
            continue;

        QString toggleKey = QString("%1|%2|row%3|%4").arg(this->comboBoxProject->currentText(), currentTab()).arg(rowIndex).arg(buttonIndex);

        QPushButton* button = new QPushButton(resolvePlaceholders(def.label, rowIndex), container);
        button->setFocusPolicy(Qt::NoFocus);
        styleActionButton(button, def, this->litToggles.contains(toggleKey));
        SheetsActionDef defCopy = def;
        QObject::connect(button, &QPushButton::clicked, this, [this, defCopy, rowIndex, toggleKey, button]() {
            executeAction(defCopy, rowIndex, toggleKey, button);
        });
        layout->addWidget(button);
        anyButton = true;
    }
    layout->addStretch();

    if (!anyButton)
    {
        delete container;
        return nullptr;
    }
    return container;
}

void SheetsPanelWidget::renderStandaloneButtons()
{
    QLayoutItem* child;
    while ((child = this->horizontalLayoutStandalone->takeAt(0)) != nullptr)
    {
        if (child->widget())
            child->widget()->deleteLater();
        delete child;
    }

    const SheetsTabActions actions = this->tabActions.value(currentTab());
    this->widgetStandalone->setVisible(!actions.standalone.isEmpty());

    for (int buttonIndex = 0; buttonIndex < actions.standalone.count(); buttonIndex++)
    {
        const SheetsActionDef& def = actions.standalone[buttonIndex];
        QString toggleKey = QString("%1|%2|standalone|%3").arg(this->comboBoxProject->currentText(), currentTab()).arg(buttonIndex);

        QPushButton* button = new QPushButton(def.label, this->widgetStandalone);
        button->setFocusPolicy(Qt::NoFocus);
        styleActionButton(button, def, this->litToggles.contains(toggleKey));
        SheetsActionDef defCopy = def;
        QObject::connect(button, &QPushButton::clicked, this, [this, defCopy, toggleKey, button]() {
            executeAction(defCopy, -1, toggleKey, button);
        });
        this->horizontalLayoutStandalone->addWidget(button);
    }
    this->horizontalLayoutStandalone->addStretch();
}

void SheetsPanelWidget::styleActionButton(QPushButton* button, const SheetsActionDef& def, bool lit) const
{
    QString background = def.color.isEmpty() ? "rgba(60, 60, 60, 220)" : def.color;
    QString border = lit ? "2px solid #50c878" : "1px solid rgba(90, 90, 90, 200)";
    button->setStyleSheet(QString(
        "QPushButton { background-color: %1; color: white; border: %2; border-radius: 3px;"
        " padding: 2px 8px; font-size: 11px; font-weight: bold; }"
        "QPushButton:hover { border: 2px solid rgba(200, 200, 200, 200); }")
        .arg(background, border));
}

/* ---------------- execution ---------------- */

QString SheetsPanelWidget::resolvePlaceholders(const QString& text, int rowIndex) const
{
    if (rowIndex < 0 || rowIndex >= this->rows.count())
        return text;

    QString result = text;
    for (int col = 0; col < this->headers.count(); col++)
    {
        QString value = (col < this->rows[rowIndex].count()) ? this->rows[rowIndex][col] : QString();
        result.replace("{" + this->headers[col] + "}", value);
    }
    return result;
}

QString SheetsPanelWidget::buildTemplateDataXml(const QString& resolvedData) const
{
    // "key=value,key2=value2" -> componentData XML, built from the same pre-escaped
    // component constant the rundown template items use, so the AMCP message is
    // byte-identical to what a manually fired template item sends.
    QString xml = "<templateData>";
    foreach (const QString& pair, resolvedData.split(",", Qt::SkipEmptyParts))
    {
        int eq = pair.indexOf('=');
        if (eq <= 0)
            continue;

        QString component = TemplateData::DEFAULT_COMPONENT_DATA_XML;
        component.replace("#KEY", pair.left(eq).trimmed());
        component.replace("#VALUE", Xml::encode(pair.mid(eq + 1).trimmed()).replace("\\", "\\\\"));
        xml += component;
    }
    xml += "</templateData>";
    return xml;
}

void SheetsPanelWidget::executeAction(const SheetsActionDef& def, int rowIndex, const QString& toggleKey, QPushButton* button)
{
    SheetsProject* project = currentProject();
    if (project == nullptr)
        return;

    const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(project->deviceName);
    if (device == nullptr || !device->isConnected())
    {
        setStatus("Device not connected: " + project->deviceName);
        return;
    }

    QString resolvedData = resolvePlaceholders(def.data, rowIndex);
    QString dataXml = buildTemplateDataXml(resolvedData);
    QString label = resolvePlaceholders(def.label, rowIndex);
    QString target = QString("%1/%2").arg(def.channel).arg(def.videolayer);

    QString effectiveAction = def.action;
    if (def.action == "toggle")
        effectiveAction = this->litToggles.contains(toggleKey) ? "stop" : "play";

    // Exclusive toggles per channel/layer: pressing a different toggle while one is
    // on air on the same layer stops the current one (out animation plays) instead
    // of overwriting it. Press again to play the new one.
    if (def.action == "toggle" && effectiveAction == "play")
    {
        QString conflictKey;
        for (auto it = this->litToggleTargets.constBegin(); it != this->litToggleTargets.constEnd(); ++it)
        {
            if (it.value() == target && it.key() != toggleKey)
            {
                conflictKey = it.key();
                break;
            }
        }

        if (!conflictKey.isEmpty())
        {
            device->stopTemplate(def.channel, def.videolayer, TEMPLATE_FLASHLAYER);
            this->litToggles.remove(conflictKey);
            this->litToggleTargets.remove(conflictKey);
            EventManager::getInstance().fireChannelActivityEvent(
                ChannelActivityEvent(def.channel, def.videolayer, label, "TEMPLATE", false));
            emit EventManager::getInstance().playoutAction("STOP (Sheets)", label, project->deviceName, def.channel, def.videolayer);
            setStatus(QString("Stopped %1 \xe2\x80\x94 press again to play").arg(target));
            rebuildButtonsDeferred();
            return;
        }
    }

    if (effectiveAction == "play")
    {
        device->playTemplate(def.channel, def.videolayer, TEMPLATE_FLASHLAYER, def.templateName, dataXml);
        EventManager::getInstance().fireChannelActivityEvent(
            ChannelActivityEvent(def.channel, def.videolayer, label, "TEMPLATE", true));
    }
    else if (effectiveAction == "update")
    {
        device->updateTemplate(def.channel, def.videolayer, TEMPLATE_FLASHLAYER, dataXml);
    }
    else if (effectiveAction == "stop")
    {
        device->stopTemplate(def.channel, def.videolayer, TEMPLATE_FLASHLAYER);
        EventManager::getInstance().fireChannelActivityEvent(
            ChannelActivityEvent(def.channel, def.videolayer, label, "TEMPLATE", false));
    }

    if (def.action == "toggle")
    {
        if (effectiveAction == "play")
        {
            this->litToggles.insert(toggleKey);
            this->litToggleTargets[toggleKey] = target;
        }
        else
        {
            this->litToggles.remove(toggleKey);
            this->litToggleTargets.remove(toggleKey);
        }
        styleActionButton(button, def, this->litToggles.contains(toggleKey));
    }

    // A plain play or stop replacing/clearing this layer invalidates any toggle
    // that was lit for it — clear the stale lit state (no extra commands).
    if (def.action != "toggle" && (effectiveAction == "play" || effectiveAction == "stop"))
        clearLitTogglesForTarget(target, QString());

    emit EventManager::getInstance().playoutAction(effectiveAction.toUpper() + " (Sheets)", label, project->deviceName, def.channel, def.videolayer);
}

/* ---------------- editor & chrome ---------------- */

void SheetsPanelWidget::editActions()
{
    if (currentTab().isEmpty())
        return;

    QStringList templateNames;
    foreach (const LibraryModel& model, DatabaseManager::getInstance().getLibraryTemplate())
        templateNames.append(model.getName());
    templateNames.removeDuplicates();
    templateNames.sort(Qt::CaseInsensitive);

    SheetsTabActions actions = this->tabActions.value(currentTab());
    SheetsActionsDialog dialog(currentTab(), this->headers, templateNames, actions, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    this->tabActions[currentTab()] = dialog.getActions();
    saveExtensions();
    renderData();
    renderStandaloneButtons();
}

void SheetsPanelWidget::setStatus(const QString& text)
{
    this->labelStatus->setText(text);
    this->labelStatus->setToolTip(text); // full message hoverable when truncated
}

void SheetsPanelWidget::toggleExpandCollapse()
{
    this->collapsed = !this->collapsed;
    PanelHelper::setPanelCollapsed("Sheets", this->collapsed);

    this->expandCollapseAction->setText(this->collapsed ? "Expand" : "Collapse");
    this->tabWidgetSheets->widget(0)->setVisible(!this->collapsed);

    if (this->collapsed)
        this->setFixedHeight(Panel::COMPACT_SHEETS_HEIGHT);
    else
        this->setFixedHeight(Panel::DEFAULT_SHEETS_HEIGHT);
}

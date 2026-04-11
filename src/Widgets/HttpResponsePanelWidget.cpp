#include "HttpResponsePanelWidget.h"

#include "Global.h"
#include "PanelHelper.h"
#include "HttpResponseLog.h"
#include "DatabaseManager.h"
#include "EventManager.h"
#include "Models/ConfigurationModel.h"

#include <QtCore/QTime>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QToolButton>

HttpResponsePanelWidget::HttpResponsePanelWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi(this);
    setupMenus();

    this->collapsed = PanelHelper::isPanelCollapsed("HttpLog");
    if (this->collapsed)
        this->expandCollapseAction->setText("Expand");

    this->setFixedHeight(Panel::DEFAULT_HTTPLOG_HEIGHT);

    QString val = DatabaseManager::getInstance().getConfigurationByName("HttpLogLastOnly").getValue();
    this->showLastOnly = val.isEmpty() || val == "true";

    QString playoutVal = DatabaseManager::getInstance().getConfigurationByName("LogPlayoutActions").getValue();
    this->logPlayoutActions = playoutVal.isEmpty() || playoutVal == "true";

    QObject::connect(&HttpResponseLog::getInstance(), &HttpResponseLog::responseReceived,
                     this, &HttpResponsePanelWidget::onResponse);
    QObject::connect(&EventManager::getInstance(), &EventManager::playoutAction,
                     this, &HttpResponsePanelWidget::onPlayoutAction);
}

void HttpResponsePanelWidget::setupMenus()
{
    this->dropdownMenu = new QMenu(this);
    this->dropdownMenu->setObjectName("panelMenu");
    PanelHelper::addMoveActions(this->dropdownMenu, "HttpLog", this);
    this->dropdownMenu->addSeparator();
    this->dropdownMenu->addAction("Clear Log", this, &HttpResponsePanelWidget::clearLog);
    QAction* playoutLogAction = this->dropdownMenu->addAction("Log Playout Actions");
    playoutLogAction->setCheckable(true);
    playoutLogAction->setChecked(this->logPlayoutActions);
    QObject::connect(playoutLogAction, &QAction::toggled, this, &HttpResponsePanelWidget::togglePlayoutLog);
    this->dropdownMenu->addSeparator();
    this->expandCollapseAction = this->dropdownMenu->addAction("Collapse", this, &HttpResponsePanelWidget::toggleExpandCollapse);

    this->menuButton = new QToolButton(this->tabWidgetHttpLog);
    this->menuButton->setText(QString::fromUtf8("\xe2\x89\xa1"));
    this->menuButton->setFixedSize(22, 22);
    this->menuButton->setMenu(this->dropdownMenu);
    this->menuButton->setPopupMode(QToolButton::InstantPopup);
    this->tabWidgetHttpLog->setCornerWidget(this->menuButton);
}

void HttpResponsePanelWidget::onResponse(const QString& method, const QString& url, int statusCode, const QString& body)
{
    if (this->showLastOnly)
        this->plainTextEditLog->clear();

    QString timestamp = QTime::currentTime().toString("HH:mm:ss");

    QString entry = QString("[%1] %2 %3 \xe2\x86\x92 %4")
        .arg(timestamp)
        .arg(method)
        .arg(url)
        .arg(statusCode);

    if (!body.isEmpty())
    {
        QString truncated = body.left(500);
        if (body.length() > 500)
            truncated += "...";
        entry += "\n" + truncated;
    }

    this->plainTextEditLog->appendPlainText(entry);

    QScrollBar* scrollBar = this->plainTextEditLog->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void HttpResponsePanelWidget::onPlayoutAction(const QString& action, const QString& label, const QString& device, int channel, int videolayer)
{
    if (!this->logPlayoutActions)
        return;

    QString timestamp = QTime::currentTime().toString("HH:mm:ss");
    QString entry = QString("[%1] %2: %3 on %4 CH%5-L%6")
        .arg(timestamp, action, label, device)
        .arg(channel).arg(videolayer);

    this->plainTextEditLog->appendPlainText(entry);

    QScrollBar* scrollBar = this->plainTextEditLog->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void HttpResponsePanelWidget::togglePlayoutLog(bool enabled)
{
    this->logPlayoutActions = enabled;
    DatabaseManager::getInstance().updateConfiguration(
        ConfigurationModel(0, "LogPlayoutActions", enabled ? "true" : "false"));
}

void HttpResponsePanelWidget::clearLog()
{
    this->plainTextEditLog->clear();
}

void HttpResponsePanelWidget::toggleExpandCollapse()
{
    this->collapsed = !this->collapsed;
    PanelHelper::setPanelCollapsed("HttpLog", this->collapsed);

    this->expandCollapseAction->setText(this->collapsed ? "Expand" : "Collapse");
    this->tabWidgetHttpLog->widget(0)->setVisible(!this->collapsed);

    if (this->collapsed)
        this->setFixedHeight(Panel::COMPACT_HTTPLOG_HEIGHT);
    else
        this->setFixedHeight(Panel::DEFAULT_HTTPLOG_HEIGHT);
}

#include "StatusBarWidget.h"

#include "EventManager.h"

#include <QtCore/QTime>
#include <QtCore/QTimer>
#include <QtWidgets/QHBoxLayout>

StatusBarWidget::StatusBarWidget(QWidget* parent)
    : QWidget(parent),
      expanded(false)
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Top row: current message + expand button.
    QWidget* topRow = new QWidget(this);
    QHBoxLayout* topLayout = new QHBoxLayout(topRow);
    topLayout->setContentsMargins(4, 0, 0, 0);
    topLayout->setSpacing(0);

    this->currentLabel = new QLabel(topRow);
    this->currentLabel->setFixedHeight(20);
    this->currentLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    this->currentLabel->setStyleSheet("font-size: 10px; color: rgba(190, 190, 190, 200); padding: 2px 4px;");
    topLayout->addWidget(this->currentLabel, 1);

    this->expandButton = new QToolButton(topRow);
    this->expandButton->setAutoRaise(true);
    this->expandButton->setFixedSize(20, 20);
    this->expandButton->setArrowType(Qt::UpArrow);
    QObject::connect(this->expandButton, &QToolButton::clicked, this, &StatusBarWidget::toggleExpanded);
    topLayout->addWidget(this->expandButton, 0);

    mainLayout->addWidget(topRow);

    // History container (hidden by default).
    this->historyContainer = new QWidget(this);
    this->historyLayout = new QVBoxLayout(this->historyContainer);
    this->historyLayout->setContentsMargins(4, 0, 4, 4);
    this->historyLayout->setSpacing(1);
    this->historyContainer->hide();
    mainLayout->addWidget(this->historyContainer);

    this->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    // Connect to StatusbarEvent.
    QObject::connect(&EventManager::getInstance(), SIGNAL(statusbar(const StatusbarEvent&)),
                     this, SLOT(statusbar(const StatusbarEvent&)));
}

void StatusBarWidget::statusbar(const StatusbarEvent& event)
{
    // Add to history log.
    LogEntry entry;
    entry.time = QTime::currentTime().toString("HH:mm:ss");
    entry.message = event.getMessage();
    entry.isError = event.getIsError();
    this->messageLog.prepend(entry);
    while (this->messageLog.size() > MAX_MESSAGES)
        this->messageLog.removeLast();

    // Show the message in the collapsed label.
    showLatestMessage();

    if (this->expanded)
    {
        rebuildHistory();
        this->updateGeometry();
    }

    // Clear the current label after timeout (history keeps the message).
    if (event.getTimeout() > 0)
    {
        QTimer::singleShot(event.getTimeout(), this, [this]() {
            this->currentLabel->clear();
            this->currentLabel->setStyleSheet("font-size: 10px; color: rgba(190, 190, 190, 200); padding: 2px 4px;");
        });
    }
}

void StatusBarWidget::showLatestMessage()
{
    if (this->messageLog.isEmpty())
    {
        this->currentLabel->clear();
        return;
    }

    const LogEntry& latest = this->messageLog.first();
    this->currentLabel->setText(QString("<span style='color: rgba(100,100,100,180);'>%1</span>  %2")
        .arg(latest.time, latest.message));
    this->currentLabel->setTextFormat(Qt::RichText);

    if (latest.isError)
        this->currentLabel->setStyleSheet("font-size: 10px; color: #ff4444; font-weight: bold; padding: 2px 4px;");
    else
        this->currentLabel->setStyleSheet("font-size: 10px; color: rgba(190, 190, 190, 200); padding: 2px 4px;");
}

void StatusBarWidget::toggleExpanded()
{
    this->expanded = !this->expanded;
    this->expandButton->setArrowType(this->expanded ? Qt::DownArrow : Qt::UpArrow);
    this->historyContainer->setVisible(this->expanded);
    if (this->expanded)
        rebuildHistory();

    this->updateGeometry();
}

void StatusBarWidget::rebuildHistory()
{
    qDeleteAll(this->historyLabels);
    this->historyLabels.clear();

    for (const LogEntry& entry : this->messageLog)
    {
        QLabel* label = new QLabel(this->historyContainer);
        label->setText(QString("<span style='color: rgba(100,100,100,180);'>%1</span>  %2")
            .arg(entry.time, entry.message));
        label->setTextFormat(Qt::RichText);
        label->setWordWrap(true);
        label->setStyleSheet(entry.isError
            ? "font-size: 10px; color: #ff4444; padding: 1px 4px;"
            : "font-size: 10px; color: rgba(160, 160, 160, 180); padding: 1px 4px;");
        this->historyLayout->addWidget(label);
        this->historyLabels.append(label);
    }
}

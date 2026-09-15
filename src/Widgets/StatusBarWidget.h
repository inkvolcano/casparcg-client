#pragma once

#include "Shared.h"

#include "Events/StatusbarEvent.h"

#include <QtCore/QList>
#include <QtWidgets/QLabel>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

class WIDGETS_EXPORT StatusBarWidget : public QWidget
{
    Q_OBJECT

    public:
        explicit StatusBarWidget(QWidget* parent = 0);

    private:
        static const int MAX_MESSAGES = 10;

        bool expanded;
        QToolButton* expandButton;
        QLabel* currentLabel;

        // Clears the line once the latest message's timeout has passed. One timer,
        // restarted per message: a single-shot per message used to leave the
        // earlier ones running, so a message could be wiped by the timer of the
        // one before it, well before its own three seconds were up.
        class QTimer* clearTimer = nullptr;
        void setLabelStyle(const QString& style);
        QWidget* historyContainer;
        QVBoxLayout* historyLayout;

        struct LogEntry
        {
            QString time;
            QString message;
            bool isError;
        };
        QList<LogEntry> messageLog;
        QList<QLabel*> historyLabels;

        void showLatestMessage();
        void rebuildHistory();

        Q_SLOT void statusbar(const StatusbarEvent&);
        Q_SLOT void toggleExpanded();
};

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

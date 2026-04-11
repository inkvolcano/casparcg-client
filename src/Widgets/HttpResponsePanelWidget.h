#pragma once

#include "Shared.h"
#include "ui_HttpResponsePanelWidget.h"

#include <QtWidgets/QMenu>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QWidget>

class WIDGETS_EXPORT HttpResponsePanelWidget : public QWidget, Ui::HttpResponsePanelWidget
{
    Q_OBJECT

public:
    explicit HttpResponsePanelWidget(QWidget* parent = nullptr);

private:
    void setupMenus();
    Q_SLOT void onResponse(const QString& method, const QString& url, int statusCode, const QString& body);
    Q_SLOT void onPlayoutAction(const QString& action, const QString& label, const QString& device, int channel, int videolayer);
    Q_SLOT void clearLog();
    Q_SLOT void toggleExpandCollapse();
    Q_SLOT void togglePlayoutLog(bool enabled);

    bool collapsed = false;
    bool showLastOnly = true;
    bool logPlayoutActions = true;
    QToolButton* menuButton = nullptr;
    QMenu* dropdownMenu = nullptr;
    QAction* expandCollapseAction = nullptr;
};

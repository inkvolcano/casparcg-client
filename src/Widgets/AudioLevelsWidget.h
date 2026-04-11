#pragma once

#include "Shared.h"
#include "ui_AudioLevelsWidget.h"

#include "CasparDevice.h"

#include <QtGui/QImage>

#include <QtWidgets/QMenu>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QWidget>

class WIDGETS_EXPORT AudioLevelsWidget : public QWidget, Ui::AudioLevelsWidget
{
    Q_OBJECT

    public:
        explicit AudioLevelsWidget(QWidget* parent = 0);

    private:
        bool collapsed = false;

        QToolButton* menuButton = nullptr;
        QMenu* dropdownMenu = nullptr;
        QAction* expandCollapseAction = nullptr;

        void setupMenus();
        void rebuildTabs();

        Q_SLOT void toggleExpandCollapse();
        Q_SLOT void deviceAdded(CasparDevice&);
        Q_SLOT void deviceRemoved();
};

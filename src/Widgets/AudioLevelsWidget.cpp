#include "AudioLevelsWidget.h"
#include "AudioMeterWidget.h"

#include "Global.h"
#include "DeviceManager.h"
#include "PanelHelper.h"

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QToolButton>

AudioLevelsWidget::AudioLevelsWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi(this);
    setupMenus();

    this->collapsed = PanelHelper::isPanelCollapsed("AudioLevels");
    if (this->collapsed)
        this->expandCollapseAction->setText("Expand");

    // Remove the hardcoded tab from .ui — we create tabs dynamically.
    while (this->tabWidgetAudioLevels->count() > 0)
    {
        QWidget* tab = this->tabWidgetAudioLevels->widget(0);
        this->tabWidgetAudioLevels->removeTab(0);
        delete tab;
    }

    QObject::connect(&DeviceManager::getInstance(), SIGNAL(deviceAdded(CasparDevice&)), this, SLOT(deviceAdded(CasparDevice&)));
    QObject::connect(&DeviceManager::getInstance(), SIGNAL(deviceRemoved()), this, SLOT(deviceRemoved()));

    rebuildTabs();
}

void AudioLevelsWidget::setupMenus()
{
    this->dropdownMenu = new QMenu(this);
    this->dropdownMenu->setObjectName("panelMenu");
    PanelHelper::addMoveActions(this->dropdownMenu, "AudioLevels", this);
    this->dropdownMenu->addSeparator();
    this->expandCollapseAction = this->dropdownMenu->addAction("Collapse", this, &AudioLevelsWidget::toggleExpandCollapse);

    this->menuButton = new QToolButton(this->tabWidgetAudioLevels);
    this->menuButton->setText(QString::fromUtf8("\xe2\x89\xa1"));
    this->menuButton->setFixedSize(22, 22);
    this->menuButton->setMenu(this->dropdownMenu);
    this->menuButton->setPopupMode(QToolButton::InstantPopup);
    this->tabWidgetAudioLevels->setCornerWidget(this->menuButton);
}

void AudioLevelsWidget::rebuildTabs()
{
    // Remove all existing tabs.
    while (this->tabWidgetAudioLevels->count() > 0)
    {
        QWidget* tab = this->tabWidgetAudioLevels->widget(0);
        this->tabWidgetAudioLevels->removeTab(0);
        delete tab;
    }

    QList<DeviceModel> models = DeviceManager::getInstance().getDeviceModels();
    bool multipleDevices = (models.size() > 1);

    for (const DeviceModel& model : models)
    {
        for (int ch = 1; ch <= model.getChannels(); ch++)
        {
            QWidget* tabPage = new QWidget();
            QHBoxLayout* layout = new QHBoxLayout(tabPage);
            layout->setContentsMargins(4, 4, 4, 4);
            layout->setSpacing(0);

            layout->addStretch();
            for (int i = 1; i <= 8; i++)
            {
                AudioMeterWidget* meter = new AudioMeterWidget(tabPage);
                meter->setFixedSize(32, 100);
                meter->configureForDevice(i, model.getName(), ch);
                layout->addWidget(meter);
            }
            layout->addStretch();

            QString tabLabel = multipleDevices
                ? QString("%1 CH%2").arg(model.getName()).arg(ch)
                : QString("CH %1").arg(ch);
            this->tabWidgetAudioLevels->addTab(tabPage, tabLabel);
        }
    }

    // If no devices configured, show a placeholder tab.
    if (this->tabWidgetAudioLevels->count() == 0)
    {
        QWidget* emptyTab = new QWidget();
        this->tabWidgetAudioLevels->addTab(emptyTab, "Audio Levels");
    }
}

void AudioLevelsWidget::deviceAdded(CasparDevice&)
{
    rebuildTabs();
}

void AudioLevelsWidget::deviceRemoved()
{
    rebuildTabs();
}

void AudioLevelsWidget::toggleExpandCollapse()
{
    this->collapsed = !this->collapsed;
    PanelHelper::setPanelCollapsed("AudioLevels", this->collapsed);

    this->expandCollapseAction->setText(this->collapsed ? "Expand" : "Collapse");

    if (this->collapsed)
        this->setFixedHeight(Panel::COMPACT_AUDIOLEVELS_HEIGHT);
    else
        PanelHelper::applyExpandedHeight(this, "AudioLevels", Panel::DEFAULT_AUDIOLEVELS_HEIGHT);
}

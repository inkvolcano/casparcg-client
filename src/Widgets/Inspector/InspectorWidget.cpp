#include "InspectorWidget.h"
#include "Inspector/InspectorBlendModeWidget.h"
#include "Inspector/InspectorBrightnessWidget.h"
#include "Inspector/InspectorContrastWidget.h"
#include "Inspector/InspectorClipWidget.h"
#include "Inspector/InspectorFileRecorderWidget.h"
#include "Inspector/InspectorStillWidget.h"
#include "Inspector/InspectorGpiOutputWidget.h"
#include "Inspector/InspectorGridWidget.h"
#include "Inspector/InspectorSaturationWidget.h"
#include "Inspector/InspectorLevelsWidget.h"
#include "Inspector/InspectorMovieWidget.h"
#include "Inspector/InspectorMetadataWidget.h"
#include "Inspector/InspectorOpacityWidget.h"
#include "Inspector/InspectorOutputWidget.h"
#include "Inspector/InspectorTemplateWidget.h"
#include "Inspector/InspectorVolumeWidget.h"
#include "Inspector/InspectorDeckLinkInputWidget.h"
#include "Inspector/InspectorImageScrollerWidget.h"
#include "Inspector/InspectorKeyerWidget.h"
#include "Inspector/InspectorPrintWidget.h"
#include "Inspector/InspectorClearOutputWidget.h"
#include "Inspector/InspectorGroupWidget.h"
#include "Inspector/InspectorSolidColorWidget.h"
#include "Inspector/InspectorFadeToBlackWidget.h"
#include "Inspector/InspectorAudioWidget.h"
#include "Inspector/InspectorStillWidget.h"
#include "Inspector/InspectorCustomCommandWidget.h"
#include "Inspector/InspectorChromaWidget.h"
#include "Inspector/InspectorOscOutputWidget.h"
#include "Inspector/InspectorPlayoutCommandWidget.h"
#include "Inspector/InspectorPerspectiveWidget.h"
#include "Inspector/InspectorRotationWidget.h"
#include "Inspector/InspectorAnchorWidget.h"
#include "Inspector/InspectorFillWidget.h"
#include "Inspector/InspectorCropWidget.h"
#include "Inspector/InspectorHttpGetWidget.h"
#include "Inspector/InspectorHttpPostWidget.h"
#include "Inspector/InspectorHtmlWidget.h"
#include "Inspector/InspectorRouteChannelWidget.h"
#include "Inspector/InspectorRouteVideolayerWidget.h"
#include "Inspector/InspectorGatewayWidget.h"
#include "Inspector/InspectorInvokeWidget.h"
#include "Inspector/InspectorTransformWidget.h"

#include "Global.h"
#include "../PanelHelper.h"

#include "EventManager.h"

#include <QtCore/QTimer>
#include <QtWidgets/QVBoxLayout>
#include "Commands/AbstractCommand.h"
#include "Commands/BlendModeCommand.h"
#include "Commands/BrightnessCommand.h"
#include "Commands/CommitCommand.h"
#include "Commands/ContrastCommand.h"
#include "Commands/SolidColorCommand.h"
#include "Commands/FadeToBlackCommand.h"
#include "Commands/ClipCommand.h"
#include "Commands/FillCommand.h"
#include "Commands/GpiOutputCommand.h"
#include "Commands/GridCommand.h"
#include "Commands/GroupCommand.h"
#include "Commands/KeyerCommand.h"
#include "Commands/LevelsCommand.h"
#include "Commands/MovieCommand.h"
#include "Commands/OpacityCommand.h"
#include "Commands/SaturationCommand.h"
#include "Commands/TemplateCommand.h"
#include "Commands/VolumeCommand.h"
#include "Commands/DeckLinkInputCommand.h"
#include "Commands/ImageScrollerCommand.h"
#include "Commands/PlayoutCommand.h"
#include "Commands/PrintCommand.h"
#include "Commands/CustomCommand.h"
#include "Commands/OscOutputCommand.h"
#include "Commands/PerspectiveCommand.h"
#include "Commands/RotationCommand.h"
#include "Commands/AnchorCommand.h"
#include "Commands/CropCommand.h"
#include "Commands/HttpGetCommand.h"
#include "Commands/HttpPostCommand.h"
#include "Commands/HtmlCommand.h"
#include "Commands/RouteChannelCommand.h"
#include "Commands/RouteVideolayerCommand.h"
#include "Commands/GatewayCommand.h"

InspectorWidget::InspectorWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);

    // Wrap the QToolBox content in a QTabWidget for a consistent panel header.
    this->gridLayout->removeWidget(this->toolBox);
    this->toolBox->hide();

    this->tabWidgetInspector = new QTabWidget(this);
    this->tabWidgetInspector->setObjectName("tabWidgetInspector");
    this->tabWidgetInspector->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    QWidget* tabPage = new QWidget();
    tabPage->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    QVBoxLayout* tabLayout = new QVBoxLayout(tabPage);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->setSpacing(0);
    tabLayout->addWidget(this->scrollArea, 1);
    this->tabWidgetInspector->addTab(tabPage, "Inspector");

    // Replace the .ui grid layout with a QVBoxLayout so the tab widget
    // fills the entire InspectorWidget without leftover row constraints.
    delete this->gridLayout;
    QVBoxLayout* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);
    outerLayout->addWidget(this->tabWidgetInspector, 1);

    // Hamburger menu in tab corner.
    this->dropdownMenu = new QMenu(this);
    this->dropdownMenu->setObjectName("panelMenu");
    PanelHelper::addMoveActions(this->dropdownMenu, "Inspector", this);
    this->dropdownMenu->addSeparator();
    this->expandCollapseAction = this->dropdownMenu->addAction("Collapse", this, &InspectorWidget::toggleExpandCollapse);

    this->menuButton = new QToolButton(this->tabWidgetInspector);
    this->menuButton->setText(QString::fromUtf8("\xe2\x89\xa1"));
    this->menuButton->setFixedSize(22, 22);
    this->menuButton->setMenu(this->dropdownMenu);
    this->menuButton->setPopupMode(QToolButton::InstantPopup);
    this->tabWidgetInspector->setCornerWidget(this->menuButton);

    // Remove the branch indicator area entirely — expand/collapse is handled
    // by toggleExpandItem() on item click, so the arrows are not needed.
    this->treeWidgetInspector->setRootIsDecorated(false);
    this->treeWidgetInspector->setIndentation(0);

    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(0)), 0, new InspectorMetadataWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(1)), 0, new InspectorOutputWidget(this));
    InspectorInvokeWidget* invokeWidget = new InspectorInvokeWidget(this);
    QTreeWidgetItem* invokeChildItem = new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(2));
    this->treeWidgetInspector->setItemWidget(invokeChildItem, 0, invokeWidget);
    QObject::connect(invokeWidget, &InspectorInvokeWidget::contentChanged, this, [=]() {
        QTimer::singleShot(0, this, [=]() {
            invokeChildItem->setSizeHint(0, invokeWidget->sizeHint());
            this->treeWidgetInspector->doItemsLayout();
        });
    });
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(3)), 0, new InspectorTemplateWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(4)), 0, new InspectorMovieWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(5)), 0, new InspectorBlendModeWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(6)), 0, new InspectorBrightnessWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(7)), 0, new InspectorContrastWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(8)), 0, new InspectorClipWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(9)), 0, new InspectorFillWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(10)), 0, new InspectorGridWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(11)), 0, new InspectorLevelsWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(12)), 0, new InspectorOpacityWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(13)), 0, new InspectorSaturationWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(14)), 0, new InspectorVolumeWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(15)), 0, new InspectorDeckLinkInputWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(16)), 0, new InspectorGpiOutputWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(17)), 0, new InspectorImageScrollerWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(18)), 0, new InspectorFileRecorderWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(19)), 0, new InspectorKeyerWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(20)), 0, new InspectorPrintWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(21)), 0, new InspectorClearOutputWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(22)), 0, new InspectorGroupWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(23)), 0, new InspectorSolidColorWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(24)), 0, new InspectorAudioWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(25)), 0, new InspectorStillWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(26)), 0, new InspectorCustomCommandWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(27)), 0, new InspectorChromaWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(28)), 0, new InspectorOscOutputWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(29)), 0, new InspectorPlayoutCommandWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(30)), 0, new InspectorFadeToBlackWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(31)), 0, new InspectorPerspectiveWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(32)), 0, new InspectorRotationWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(33)), 0, new InspectorAnchorWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(34)), 0, new InspectorCropWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(35)), 0, new InspectorHttpGetWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(36)), 0, new InspectorHttpPostWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(37)), 0, new InspectorHtmlWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(38)), 0, new InspectorRouteChannelWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(39)), 0, new InspectorRouteVideolayerWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->treeWidgetInspector->topLevelItem(40)), 0, new InspectorGatewayWidget(this));

    // Dynamically add "Embedded Transform" section (index 41).
    QTreeWidgetItem* transformTopLevel = new QTreeWidgetItem();
    transformTopLevel->setText(0, "Embedded Transform");
    transformTopLevel->setBackground(0, QBrush(QColor(45, 45, 45)));
    transformTopLevel->setForeground(0, QBrush(QColor(255, 255, 255)));
    this->treeWidgetInspector->addTopLevelItem(transformTopLevel);
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(transformTopLevel), 0, new InspectorTransformWidget(this));

    this->treeWidgetInspector->expandAll();

    setDefaultVisibleWidgets();

    QObject::connect(&EventManager::getInstance(), SIGNAL(rundownItemSelected(const RundownItemSelectedEvent &)), this, SLOT(rundownItemSelected(const RundownItemSelectedEvent &)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(libraryItemSelected(const LibraryItemSelectedEvent &)), this, SLOT(libraryItemSelected(const LibraryItemSelectedEvent &)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(emptyRundown(const EmptyRundownEvent &)), this, SLOT(emptyRundown(const EmptyRundownEvent &)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(repositoryRundown(const RepositoryRundownEvent &)), this, SLOT(repositoryRundown(const RepositoryRundownEvent &)));
}

void InspectorWidget::repositoryRundown(const RepositoryRundownEvent &event)
{
    for (int i = 0; i < this->treeWidgetInspector->topLevelItemCount(); i++)
    {
        QTreeWidgetItem *item = this->treeWidgetInspector->topLevelItem(i);
        QWidget *widget = this->treeWidgetInspector->itemWidget(item->child(0), 0);

        widget->setEnabled(!event.getRepositoryRundown());
    }
}

void InspectorWidget::rundownItemSelected(const RundownItemSelectedEvent &event)
{
    setDefaultVisibleWidgets();

    // Show "Embedded Transform" section for all content item types.
    bool isContentItem = dynamic_cast<TemplateCommand*>(event.getCommand()) ||
                         dynamic_cast<MovieCommand*>(event.getCommand()) ||
                         dynamic_cast<StillCommand*>(event.getCommand()) ||
                         dynamic_cast<AudioCommand*>(event.getCommand()) ||
                         dynamic_cast<HtmlCommand*>(event.getCommand()) ||
                         dynamic_cast<ImageScrollerCommand*>(event.getCommand());
    if (isContentItem)
        this->treeWidgetInspector->topLevelItem(41)->setHidden(false);

    if (dynamic_cast<TemplateCommand *>(event.getCommand()))
    {
        this->treeWidgetInspector->topLevelItem(2)->setHidden(false);
        this->treeWidgetInspector->topLevelItem(3)->setHidden(false);
    }
    else if (dynamic_cast<AudioCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(24)->setHidden(false);
    else if (dynamic_cast<StillCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(25)->setHidden(false);
    else if (dynamic_cast<MovieCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(4)->setHidden(false);
    else if (dynamic_cast<BlendModeCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(5)->setHidden(false);
    else if (dynamic_cast<BrightnessCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(6)->setHidden(false);
    else if (dynamic_cast<ContrastCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(7)->setHidden(false);
    else if (dynamic_cast<ClipCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(8)->setHidden(false);
    else if (dynamic_cast<FillCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(9)->setHidden(false);
    else if (dynamic_cast<GridCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(10)->setHidden(false);
    else if (dynamic_cast<KeyerCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(19)->setHidden(false);
    else if (dynamic_cast<CommitCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(1)->setHidden(false);
    else if (dynamic_cast<LevelsCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(11)->setHidden(false);
    else if (dynamic_cast<OpacityCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(12)->setHidden(false);
    else if (dynamic_cast<SaturationCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(13)->setHidden(false);
    else if (dynamic_cast<VolumeCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(14)->setHidden(false);
    else if (dynamic_cast<GpiOutputCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(16)->setHidden(false);
    else if (dynamic_cast<DeckLinkInputCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(15)->setHidden(false);
    else if (dynamic_cast<ImageScrollerCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(17)->setHidden(false);
    else if (dynamic_cast<FileRecorderCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(18)->setHidden(false);
    else if (dynamic_cast<PrintCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(20)->setHidden(false);
    else if (dynamic_cast<ClearOutputCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(21)->setHidden(false);
    else if (dynamic_cast<SolidColorCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(23)->setHidden(false);
    else if (dynamic_cast<GroupCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(22)->setHidden(false);
    else if (dynamic_cast<CustomCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(26)->setHidden(false);
    else if (dynamic_cast<ChromaCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(27)->setHidden(false);
    else if (dynamic_cast<OscOutputCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(28)->setHidden(false);
    else if (dynamic_cast<PlayoutCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(29)->setHidden(false);
    else if (dynamic_cast<FadeToBlackCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(30)->setHidden(false);
    else if (dynamic_cast<PerspectiveCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(31)->setHidden(false);
    else if (dynamic_cast<RotationCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(32)->setHidden(false);
    else if (dynamic_cast<AnchorCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(33)->setHidden(false);
    else if (dynamic_cast<CropCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(34)->setHidden(false);
    else if (dynamic_cast<HttpGetCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(35)->setHidden(false);
    else if (dynamic_cast<HttpPostCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(36)->setHidden(false);
    else if (dynamic_cast<HtmlCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(37)->setHidden(false);
    else if (dynamic_cast<RouteChannelCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(38)->setHidden(false);
    else if (dynamic_cast<RouteVideolayerCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(39)->setHidden(false);
    else if (dynamic_cast<GatewayCommand *>(event.getCommand()))
        this->treeWidgetInspector->topLevelItem(40)->setHidden(false);

    // Force the tree and scroll area to recalculate content size after
    // showing/hiding sections (especially the Embedded Transform section).
    this->treeWidgetInspector->doItemsLayout();
    QTimer::singleShot(0, this, [this]() {
        this->treeWidgetInspector->updateGeometry();
        this->scrollArea->updateGeometry();
    });
}

void InspectorWidget::setDefaultVisibleWidgets()
{
    this->treeWidgetInspector->topLevelItem(0)->setHidden(false);
    this->treeWidgetInspector->topLevelItem(1)->setHidden(false);
    this->treeWidgetInspector->topLevelItem(2)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(3)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(4)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(5)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(6)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(7)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(8)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(9)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(10)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(11)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(12)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(13)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(14)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(15)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(16)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(17)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(18)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(19)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(20)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(21)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(22)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(23)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(24)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(25)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(26)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(27)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(28)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(29)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(30)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(31)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(32)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(33)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(34)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(35)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(36)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(37)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(38)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(39)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(40)->setHidden(true);
    this->treeWidgetInspector->topLevelItem(41)->setHidden(true);
}

void InspectorWidget::emptyRundown(const EmptyRundownEvent &event)
{
    Q_UNUSED(event);

    setDefaultVisibleWidgets();
}

void InspectorWidget::libraryItemSelected(const LibraryItemSelectedEvent &event)
{
    Q_UNUSED(event);

    setDefaultVisibleWidgets();
}

void InspectorWidget::toggleExpandCollapse()
{
    this->collapsed = !this->collapsed;

    this->expandCollapseAction->setText(this->collapsed ? "Expand" : "Collapse");

    if (this->collapsed)
    {
        this->setFixedHeight(Panel::COMPACT_AUDIOLEVELS_HEIGHT);
    }
    else
    {
        PanelHelper::applyExpandedHeight(this, "Inspector", 200);
    }
}

void InspectorWidget::toggleExpandItem(QTreeWidgetItem *item, int index)
{
    Q_UNUSED(index);

    item->setExpanded(!item->isExpanded());
}


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
#include "Inspector/InspectorShellCommandWidget.h"
#include "Inspector/InspectorInvokeWidget.h"
#include "Inspector/InspectorTransformWidget.h"
#include "Inspector/InspectorSimpleModeWidget.h"
#include "../WheelGuard.h"

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
#include "Commands/ShellCommand.h"

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

    // Simple Mode is shown first: it is the section an operator building a button
    // surface reaches for on every item, where the rest are reached for when
    // something specific needs changing.
    //
    // It goes in HERE, before the first sectionItem() call, because sectionItem()
    // maps a declaration index onto a display row by assuming this item already
    // occupies row 0. Inserting it later left that assumption false for the whole
    // of construction, and every section below was handed its neighbour's widget.
    QTreeWidgetItem* simpleModeTopLevel = new QTreeWidgetItem();
    simpleModeTopLevel->setText(0, "Simple Mode");
    simpleModeTopLevel->setBackground(0, QBrush(QColor(45, 45, 45)));
    simpleModeTopLevel->setForeground(0, QBrush(QColor(255, 255, 255)));
    this->treeWidgetInspector->insertTopLevelItem(0, simpleModeTopLevel);
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(simpleModeTopLevel), 0,
                                             new InspectorSimpleModeWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(0)), 0, new InspectorMetadataWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(1)), 0, new InspectorOutputWidget(this));
    InspectorInvokeWidget* invokeWidget = new InspectorInvokeWidget(this);
    QTreeWidgetItem* invokeChildItem = new QTreeWidgetItem(sectionItem(2));
    this->treeWidgetInspector->setItemWidget(invokeChildItem, 0, invokeWidget);
    QObject::connect(invokeWidget, &InspectorInvokeWidget::contentChanged, this, [=]() {
        QTimer::singleShot(0, this, [=]() {
            invokeChildItem->setSizeHint(0, invokeWidget->sizeHint());
            this->treeWidgetInspector->doItemsLayout();
        });
    });
    InspectorTemplateWidget* templateWidget = new InspectorTemplateWidget(this);
    QTreeWidgetItem* templateChildItem = new QTreeWidgetItem(sectionItem(TEMPLATE_SECTION));
    this->treeWidgetInspector->setItemWidget(templateChildItem, 0, templateWidget);

    // The row must be exactly as tall as the widget, and the widget changes height
    // as the table grows and the result box comes and goes. Same arrangement as the
    // Invoke section: re-read sizeHint() on the next turn of the loop, after the
    // layouts inside have settled.
    templateChildItem->setSizeHint(0, templateWidget->sizeHint());
    QObject::connect(templateWidget, &InspectorTemplateWidget::contentChanged, this, [=]() {
        QTimer::singleShot(0, this, [=]() {
            templateChildItem->setSizeHint(0, templateWidget->sizeHint());
            this->treeWidgetInspector->doItemsLayout();
        });
    });

    // Template Settings: the option rows the Template widget lays out in a panel of
    // its own. Inserted here, straight under Template and before the sections
    // declared after it are built, so sectionItem() is right for all of them.
    this->templateSettingsTopLevel = new QTreeWidgetItem();
    this->templateSettingsTopLevel->setText(0, "Template Settings");
    this->templateSettingsTopLevel->setBackground(0, QBrush(QColor(45, 45, 45)));
    this->templateSettingsTopLevel->setForeground(0, QBrush(QColor(255, 255, 255)));
    this->treeWidgetInspector->insertTopLevelItem(
        this->treeWidgetInspector->indexOfTopLevelItem(sectionItem(TEMPLATE_SECTION)) + 1,
        this->templateSettingsTopLevel);
    QTreeWidgetItem* settingsChildItem = new QTreeWidgetItem(this->templateSettingsTopLevel);
    this->treeWidgetInspector->setItemWidget(settingsChildItem, 0, templateWidget->templateSettingsPanel());
    settingsChildItem->setSizeHint(0, templateWidget->templateSettingsPanel()->sizeHint());
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(4)), 0, new InspectorMovieWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(5)), 0, new InspectorBlendModeWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(6)), 0, new InspectorBrightnessWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(7)), 0, new InspectorContrastWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(8)), 0, new InspectorClipWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(9)), 0, new InspectorFillWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(10)), 0, new InspectorGridWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(11)), 0, new InspectorLevelsWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(12)), 0, new InspectorOpacityWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(13)), 0, new InspectorSaturationWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(14)), 0, new InspectorVolumeWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(15)), 0, new InspectorDeckLinkInputWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(16)), 0, new InspectorGpiOutputWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(17)), 0, new InspectorImageScrollerWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(18)), 0, new InspectorFileRecorderWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(19)), 0, new InspectorKeyerWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(20)), 0, new InspectorPrintWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(21)), 0, new InspectorClearOutputWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(22)), 0, new InspectorGroupWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(23)), 0, new InspectorSolidColorWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(24)), 0, new InspectorAudioWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(25)), 0, new InspectorStillWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(26)), 0, new InspectorCustomCommandWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(27)), 0, new InspectorChromaWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(28)), 0, new InspectorOscOutputWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(29)), 0, new InspectorPlayoutCommandWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(30)), 0, new InspectorFadeToBlackWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(31)), 0, new InspectorPerspectiveWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(32)), 0, new InspectorRotationWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(33)), 0, new InspectorAnchorWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(34)), 0, new InspectorCropWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(35)), 0, new InspectorHttpGetWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(36)), 0, new InspectorHttpPostWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(37)), 0, new InspectorHtmlWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(38)), 0, new InspectorRouteChannelWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(39)), 0, new InspectorRouteVideolayerWidget(this));
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(sectionItem(40)), 0, new InspectorGatewayWidget(this));

    // Dynamically add "Embedded Transform" section (index 41).
    QTreeWidgetItem* transformTopLevel = new QTreeWidgetItem();
    transformTopLevel->setText(0, "Embedded Transform");
    transformTopLevel->setBackground(0, QBrush(QColor(45, 45, 45)));
    transformTopLevel->setForeground(0, QBrush(QColor(255, 255, 255)));
    this->treeWidgetInspector->addTopLevelItem(transformTopLevel);
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(transformTopLevel), 0, new InspectorTransformWidget(this));

    // "Shell Command" is appended after Embedded Transform and reached through
    // this pointer rather than sectionItem(), because sectionItem() maps declared
    // indices onto rows by counting and a new row in the middle would hand every
    // section below it its neighbour's widget.
    this->shellCommandTopLevel = new QTreeWidgetItem();
    this->shellCommandTopLevel->setText(0, "Shell Command");
    this->shellCommandTopLevel->setBackground(0, QBrush(QColor(45, 45, 45)));
    this->shellCommandTopLevel->setForeground(0, QBrush(QColor(255, 255, 255)));
    this->treeWidgetInspector->addTopLevelItem(this->shellCommandTopLevel);
    this->treeWidgetInspector->setItemWidget(new QTreeWidgetItem(this->shellCommandTopLevel), 0,
                                             new InspectorShellCommandWidget(this));

    this->treeWidgetInspector->expandAll();

    // Embedded Transform and Simple Mode are rarely needed — start them collapsed;
    // clicking a header still toggles it, and the toggle sticks for the session.
    transformTopLevel->setExpanded(false);
    simpleModeTopLevel->setExpanded(false);
    this->templateSettingsTopLevel->setExpanded(false);

    setDefaultVisibleWidgets();

    QObject::connect(&EventManager::getInstance(), SIGNAL(rundownItemSelected(const RundownItemSelectedEvent &)), this, SLOT(rundownItemSelected(const RundownItemSelectedEvent &)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(libraryItemSelected(const LibraryItemSelectedEvent &)), this, SLOT(libraryItemSelected(const LibraryItemSelectedEvent &)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(emptyRundown(const EmptyRundownEvent &)), this, SLOT(emptyRundown(const EmptyRundownEvent &)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(repositoryRundown(const RepositoryRundownEvent &)), this, SLOT(repositoryRundown(const RepositoryRundownEvent &)));

    // Wheel-guard every input in every section: boxes only react to the mouse
    // wheel after being clicked; otherwise the wheel scrolls the inspector page.
    WheelGuard::applyToInputs(this);
}

// Sections are referred to throughout this file by the order they are declared in,
// which is no longer the order they appear in: Simple Mode is declared last and shown
// first. Rather than renumber a hundred and twenty call sites \xe2\x80\x94 where a single
// missed one would quietly hide the wrong section \xe2\x80\x94 the difference is resolved here.
QTreeWidgetItem* InspectorWidget::sectionItem(int declaredIndex) const
{
    if (declaredIndex == SIMPLE_MODE_SECTION)
        return this->treeWidgetInspector->topLevelItem(0);

    // Simple Mode occupies row 0, so everything declared sits one row down. Template
    // Settings is inserted straight after Template, so once it exists everything
    // declared after Template sits one further down again.
    int row = declaredIndex + 1;
    if (this->templateSettingsTopLevel != NULL && declaredIndex > TEMPLATE_SECTION)
        row += 1;

    return this->treeWidgetInspector->topLevelItem(row);
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

    // Every rundown item can become a Simple Mode button, so that section always shows.
    sectionItem(42)->setHidden(false);

    // Show "Embedded Transform" section for all content item types.
    bool isContentItem = dynamic_cast<TemplateCommand*>(event.getCommand()) ||
                         dynamic_cast<MovieCommand*>(event.getCommand()) ||
                         dynamic_cast<StillCommand*>(event.getCommand()) ||
                         dynamic_cast<AudioCommand*>(event.getCommand()) ||
                         dynamic_cast<HtmlCommand*>(event.getCommand()) ||
                         dynamic_cast<ImageScrollerCommand*>(event.getCommand());
    if (isContentItem)
        sectionItem(41)->setHidden(false);

    if (dynamic_cast<TemplateCommand *>(event.getCommand()))
    {
        sectionItem(2)->setHidden(false);
        sectionItem(3)->setHidden(false);
        this->templateSettingsTopLevel->setHidden(false);
    }
    else if (dynamic_cast<AudioCommand *>(event.getCommand()))
        sectionItem(24)->setHidden(false);
    else if (dynamic_cast<StillCommand *>(event.getCommand()))
        sectionItem(25)->setHidden(false);
    else if (dynamic_cast<MovieCommand *>(event.getCommand()))
        sectionItem(4)->setHidden(false);
    else if (dynamic_cast<BlendModeCommand *>(event.getCommand()))
        sectionItem(5)->setHidden(false);
    else if (dynamic_cast<BrightnessCommand *>(event.getCommand()))
        sectionItem(6)->setHidden(false);
    else if (dynamic_cast<ContrastCommand *>(event.getCommand()))
        sectionItem(7)->setHidden(false);
    else if (dynamic_cast<ClipCommand *>(event.getCommand()))
        sectionItem(8)->setHidden(false);
    else if (dynamic_cast<FillCommand *>(event.getCommand()))
        sectionItem(9)->setHidden(false);
    else if (dynamic_cast<GridCommand *>(event.getCommand()))
        sectionItem(10)->setHidden(false);
    else if (dynamic_cast<KeyerCommand *>(event.getCommand()))
        sectionItem(19)->setHidden(false);
    else if (dynamic_cast<CommitCommand *>(event.getCommand()))
        sectionItem(1)->setHidden(false);
    else if (dynamic_cast<LevelsCommand *>(event.getCommand()))
        sectionItem(11)->setHidden(false);
    else if (dynamic_cast<OpacityCommand *>(event.getCommand()))
        sectionItem(12)->setHidden(false);
    else if (dynamic_cast<SaturationCommand *>(event.getCommand()))
        sectionItem(13)->setHidden(false);
    else if (dynamic_cast<VolumeCommand *>(event.getCommand()))
        sectionItem(14)->setHidden(false);
    else if (dynamic_cast<GpiOutputCommand *>(event.getCommand()))
        sectionItem(16)->setHidden(false);
    else if (dynamic_cast<DeckLinkInputCommand *>(event.getCommand()))
        sectionItem(15)->setHidden(false);
    else if (dynamic_cast<ImageScrollerCommand *>(event.getCommand()))
        sectionItem(17)->setHidden(false);
    else if (dynamic_cast<FileRecorderCommand *>(event.getCommand()))
        sectionItem(18)->setHidden(false);
    else if (dynamic_cast<PrintCommand *>(event.getCommand()))
        sectionItem(20)->setHidden(false);
    else if (dynamic_cast<ClearOutputCommand *>(event.getCommand()))
        sectionItem(21)->setHidden(false);
    else if (dynamic_cast<SolidColorCommand *>(event.getCommand()))
        sectionItem(23)->setHidden(false);
    else if (dynamic_cast<GroupCommand *>(event.getCommand()))
        sectionItem(22)->setHidden(false);
    else if (dynamic_cast<CustomCommand *>(event.getCommand()))
        sectionItem(26)->setHidden(false);
    else if (dynamic_cast<ChromaCommand *>(event.getCommand()))
        sectionItem(27)->setHidden(false);
    else if (dynamic_cast<OscOutputCommand *>(event.getCommand()))
        sectionItem(28)->setHidden(false);
    else if (dynamic_cast<PlayoutCommand *>(event.getCommand()))
        sectionItem(29)->setHidden(false);
    else if (dynamic_cast<FadeToBlackCommand *>(event.getCommand()))
        sectionItem(30)->setHidden(false);
    else if (dynamic_cast<PerspectiveCommand *>(event.getCommand()))
        sectionItem(31)->setHidden(false);
    else if (dynamic_cast<RotationCommand *>(event.getCommand()))
        sectionItem(32)->setHidden(false);
    else if (dynamic_cast<AnchorCommand *>(event.getCommand()))
        sectionItem(33)->setHidden(false);
    else if (dynamic_cast<CropCommand *>(event.getCommand()))
        sectionItem(34)->setHidden(false);
    else if (dynamic_cast<HttpGetCommand *>(event.getCommand()))
        sectionItem(35)->setHidden(false);
    else if (dynamic_cast<HttpPostCommand *>(event.getCommand()))
        sectionItem(36)->setHidden(false);
    else if (dynamic_cast<HtmlCommand *>(event.getCommand()))
        sectionItem(37)->setHidden(false);
    else if (dynamic_cast<RouteChannelCommand *>(event.getCommand()))
        sectionItem(38)->setHidden(false);
    else if (dynamic_cast<RouteVideolayerCommand *>(event.getCommand()))
        sectionItem(39)->setHidden(false);
    else if (dynamic_cast<GatewayCommand *>(event.getCommand()))
        sectionItem(40)->setHidden(false);
    else if (dynamic_cast<ShellCommand *>(event.getCommand()))
        this->shellCommandTopLevel->setHidden(false);

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
    sectionItem(0)->setHidden(false);
    sectionItem(1)->setHidden(false);
    sectionItem(2)->setHidden(true);
    sectionItem(3)->setHidden(true);
    this->templateSettingsTopLevel->setHidden(true);
    this->shellCommandTopLevel->setHidden(true);
    sectionItem(4)->setHidden(true);
    sectionItem(5)->setHidden(true);
    sectionItem(6)->setHidden(true);
    sectionItem(7)->setHidden(true);
    sectionItem(8)->setHidden(true);
    sectionItem(9)->setHidden(true);
    sectionItem(10)->setHidden(true);
    sectionItem(11)->setHidden(true);
    sectionItem(12)->setHidden(true);
    sectionItem(13)->setHidden(true);
    sectionItem(14)->setHidden(true);
    sectionItem(15)->setHidden(true);
    sectionItem(16)->setHidden(true);
    sectionItem(17)->setHidden(true);
    sectionItem(18)->setHidden(true);
    sectionItem(19)->setHidden(true);
    sectionItem(20)->setHidden(true);
    sectionItem(21)->setHidden(true);
    sectionItem(22)->setHidden(true);
    sectionItem(23)->setHidden(true);
    sectionItem(24)->setHidden(true);
    sectionItem(25)->setHidden(true);
    sectionItem(26)->setHidden(true);
    sectionItem(27)->setHidden(true);
    sectionItem(28)->setHidden(true);
    sectionItem(29)->setHidden(true);
    sectionItem(30)->setHidden(true);
    sectionItem(31)->setHidden(true);
    sectionItem(32)->setHidden(true);
    sectionItem(33)->setHidden(true);
    sectionItem(34)->setHidden(true);
    sectionItem(35)->setHidden(true);
    sectionItem(36)->setHidden(true);
    sectionItem(37)->setHidden(true);
    sectionItem(38)->setHidden(true);
    sectionItem(39)->setHidden(true);
    sectionItem(40)->setHidden(true);
    sectionItem(41)->setHidden(true);
    sectionItem(42)->setHidden(true);
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


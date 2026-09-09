#include "RundownTreeWidget.h"
#include "RundownBlendModeWidget.h"
#include "RundownBrightnessWidget.h"
#include "RundownCommitWidget.h"
#include "RundownContrastWidget.h"
#include "RundownClipWidget.h"
#include "RundownDeckLinkInputWidget.h"
#include "RundownFileRecorderWidget.h"
#include "RundownImageScrollerWidget.h"
#include "RundownFillWidget.h"
#include "RundownGpiOutputWidget.h"
#include "RundownGridWidget.h"
#include "RundownGroupWidget.h"
#include "RundownKeyerWidget.h"
#include "RundownLevelsWidget.h"
#include "RundownMovieWidget.h"
#include "RundownOpacityWidget.h"
#include "RundownSaturationWidget.h"
#include "RundownTemplateWidget.h"
#include "RundownVolumeWidget.h"
#include "RundownSeparatorWidget.h"
#include "RundownPrintWidget.h"
#include "RundownClearOutputWidget.h"
#include "RundownSolidColorWidget.h"
#include "RundownAudioWidget.h"
#include "RundownStillWidget.h"
#include "RundownAutoPlayGatewayWidget.h"
#include "RundownFocusGatewayWidget.h"
#include "RundownCommandGatewayWidget.h"
#include "RundownItemFactory.h"
#include "RundownWidgetHelper.h"
#include "PresetDialog.h"

#include "Commands/TransformData.h"
#include "Commands/TemplateCommand.h"
#include "Commands/MovieCommand.h"
#include "Commands/StillCommand.h"
#include "Commands/AudioCommand.h"
#include "Commands/HtmlCommand.h"
#include "Commands/ImageScrollerCommand.h"
#include "Commands/FillCommand.h"
#include "Commands/OpacityCommand.h"
#include "Commands/ClipCommand.h"
#include "Commands/CropCommand.h"
#include "Commands/AnchorCommand.h"
#include "Commands/PerspectiveCommand.h"
#include "Commands/RotationCommand.h"
#include "Commands/BrightnessCommand.h"
#include "Commands/ContrastCommand.h"
#include "Commands/SaturationCommand.h"
#include "Commands/VolumeCommand.h"
#include "Commands/LevelsCommand.h"
#include "Commands/BlendModeCommand.h"
#include "Commands/KeyerCommand.h"

#include "GpiManager.h"
#include "DatabaseManager.h"
#include "EventManager.h"
#include "DeviceManager.h"
#include "CasparDevice.h"
#include "../SheetDataResolver.h"
#include "CloneGroupRegistry.h"
#include "TriggerBankRegistry.h"
#include "Events/PresetChangedEvent.h"
#include "Events/StatusbarEvent.h"
#include "Events/Rundown/ActiveRundownChangedEvent.h"
#include "Events/Rundown/EmptyRundownEvent.h"
#include "Events/Rundown/OpenRundownEvent.h"
#include "Events/Rundown/RundownItemSelectedEvent.h"
#include "Events/Rundown/SaveMenuEvent.h"
#include "Events/Rundown/SaveAsMenuEvent.h"
#include "Events/Rundown/ReloadRundownMenuEvent.h"
#include "Models/RundownModel.h"
#include "Library/LibraryWidget.h"
#include "MissingMediaScanner.h"
#include "SimpleModeMarker.h"

#include "AutoSaveNaming.h"

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QPoint>
#include <QtCore5Compat/QTextCodec>
#include <QtCore/QElapsedTimer>
#include <QtCore/QUuid>
#include <QtCore/QTextStream>
#include <QtCore/QCryptographicHash>
#include <QtCore/QSet>

#include <functional>

#include <QtGui/QClipboard>
#include <QtGui/QIcon>
#include <QtGui/QKeyEvent>

#include <QtGui/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QFrame>
#include <QtWidgets/QLabel>
#include <QtWidgets/QTreeWidgetItem>

RundownTreeWidget::RundownTreeWidget(QWidget* parent)
    : QWidget(parent),
      active(false), enterPressed(false), allowRemoteRundownTriggering(false), repositoryRundown(false),
      activeRundown(Rundown::DEFAULT_NAME), currentAutoPlayWidget(NULL), copyItem(NULL), currentPlayingAutoStepItem(NULL), currentAutostepHighlightItem(nullptr),
      upControlSubscription(NULL), downControlSubscription(NULL),
      playNowIfChannelControlSubscription(NULL), stopControlSubscription(NULL), playControlSubscription(NULL), playNowControlSubscription(NULL),
      loadControlSubscription(NULL), pauseControlSubscription(NULL), nextControlSubscription(NULL), updateControlSubscription(NULL), invokeControlSubscription(NULL), previewControlSubscription(NULL),
      clearControlSubscription(NULL), clearVideolayerControlSubscription(NULL), clearChannelControlSubscription(NULL), repositoryDevice(NULL)
{
    setupUi(this);
    setupMenus();

    QObject::connect(this->treeWidgetRundown, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(customContextMenuRequested(const QPoint &)));

    // TODO: Specific Gpi device.
    QObject::connect(GpiManager::getInstance().getGpiDevice().data(), SIGNAL(gpiTriggered(int, GpiDevice*)), this, SLOT(gpiPortTriggered(int, GpiDevice*)));

    QObject::connect(&EventManager::getInstance(), SIGNAL(clearDelayedCommands()), this, SLOT(clearDelayedCommands()));
    QObject::connect(&EventManager::getInstance(), SIGNAL(saveAsPreset(const SaveAsPresetEvent&)), this, SLOT(saveAsPreset(const SaveAsPresetEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(addPresetItem(const AddPresetItemEvent&)), this, SLOT(addPresetItem(const AddPresetItemEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(addRudnownItem(const AddRudnownItemEvent&)), this, SLOT(addRudnownItem(const AddRudnownItemEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(compactView(const CompactViewEvent&)), this, SLOT(compactView(const CompactViewEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(executeRundownItem(const ExecuteRundownItemEvent&)), this, SLOT(executeRundownItem(const ExecuteRundownItemEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(allowRemoteTriggering(const AllowRemoteTriggeringEvent&)), this, SLOT(allowRemoteTriggering(const AllowRemoteTriggeringEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(autoPlayRundownItem(const AutoPlayRundownItemEvent&)), this, SLOT(autoPlayRundownItem(const AutoPlayRundownItemEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(autoPlayChanged(const AutoPlayChangedEvent&)), this, SLOT(autoPlayChanged(const AutoPlayChangedEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(autoPlayNextRundownItem(const AutoPlayNextRundownItemEvent&)), this, SLOT(autoPlayNextRundownItem(const AutoPlayNextRundownItemEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(executePlayoutCommand(const ExecutePlayoutCommandEvent&)), this, SLOT(executePlayoutCommand(const ExecutePlayoutCommandEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(removeItemFromAutoPlayQueue(const RemoveItemFromAutoPlayQueueEvent&)), this, SLOT(removeItemFromAutoPlayQueue(const RemoveItemFromAutoPlayQueueEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(copyItemProperties(const CopyItemPropertiesEvent&)), this, SLOT(copyItemProperties(const CopyItemPropertiesEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(pasteItemProperties(const PasteItemPropertiesEvent&)), this, SLOT(pasteItemProperties(const PasteItemPropertiesEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(insertRepositoryChanges(const InsertRepositoryChangesEvent&)), this, SLOT(insertRepositoryChanges(const InsertRepositoryChangesEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(clearCurrentPlayingItem(const ClearCurrentPlayingItemEvent&)), this, SLOT(clearCurrentPlayingItem(const ClearCurrentPlayingItemEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(currentItemChanged(const CurrentItemChangedEvent&)), this, SLOT(currentItemChanged(const CurrentItemChangedEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(assignBank(const AssignBankEvent&)), this, SLOT(assignBank(const AssignBankEvent&)));
    QObject::connect(&TriggerBankRegistry::getInstance(), SIGNAL(bankTriggered(int)), this, SLOT(bankTriggered(int)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(autostepModeChanged(bool)), this, SLOT(autostepModeChanged(bool)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(unitSettingsChanged()), this, SLOT(refreshUnitLabels()));
    QObject::connect(&EventManager::getInstance(), &EventManager::previewModifierHeld,
                     this, [this](bool held) { updatePreviewChannelBadgeForSelection(held); });

    // Direct signal connections for drag-and-drop from library/preset.
    // These bypass the EventManager so drops work into any pane, not just the focused one.
    QObject::connect(this->treeWidgetRundown, &RundownTreeBaseWidget::libraryItemDropped, this, &RundownTreeWidget::insertRundownItem);
    QObject::connect(this->treeWidgetRundown, &RundownTreeBaseWidget::presetItemDropped, this, &RundownTreeWidget::insertPresetItem);
    QObject::connect(this->treeWidgetRundown, &RundownTreeBaseWidget::itemsChanged, this, &RundownTreeWidget::wireAllGatewayWidgets);

    foreach (const GpiPortModel& port, DatabaseManager::getInstance().getGpiPorts())
        gpiBindingChanged(port.getPort(), port.getAction());

    this->treeWidgetRundown->checkEmptyRundown();
}

void RundownTreeWidget::setupMenus()
{
    this->contextMenuMixer = new QMenu(this);
    this->contextMenuMixer->setObjectName("contextMenuMixer");
    this->contextMenuMixer->setTitle("Mixer");
    //this->contextMenuMixer->setIcon(QIcon(":/Graphics/Images/Mixer.png"));
    this->contextMenuMixer->addAction(QIcon(":/Graphics/Images/AnchorSmall.png"), "Anchor Point", this, SLOT(addAnchorItem()));
    this->contextMenuMixer->addAction(QIcon(":/Graphics/Images/BlendModeSmall.png"), "Blend Mode", this, SLOT(addBlendModeItem()));
    this->contextMenuMixer->addAction(QIcon(":/Graphics/Images/BrightnessSmall.png"), "Brightness", this, SLOT(addBrightnessItem()));
    this->contextMenuMixer->addAction(QIcon(":/Graphics/Images/ChromaSmall.png"), "Chroma Key", this, SLOT(addChromaKeyItem()));
    this->contextMenuMixer->addAction(QIcon(":/Graphics/Images/ClipSmall.png"), "Clipping", this, SLOT(addClipItem()));
    this->contextMenuMixer->addAction(QIcon(":/Graphics/Images/CommitSmall.png"), "Commit", this, SLOT(addCommitItem()));
    this->contextMenuMixer->addAction(QIcon(":/Graphics/Images/ContrastSmall.png"), "Contrast", this, SLOT(addContrastItem()));
    this->contextMenuMixer->addAction(QIcon(":/Graphics/Images/CropSmall.png"), "Crop", this, SLOT(addCropItem()));
    this->contextMenuMixer->addAction(QIcon(":/Graphics/Images/PerspectiveSmall.png"), "Distort", this, SLOT(addPerspectiveItem()));
    this->contextMenuMixer->addAction(QIcon(":/Graphics/Images/GridSmall.png"), "Grid", this, SLOT(addGridItem()));
    this->contextMenuMixer->addAction(QIcon(":/Graphics/Images/LevelsSmall.png"), "Levels", this, SLOT(addLevelsItem()));
    this->contextMenuMixer->addAction(QIcon(":/Graphics/Images/KeyerSmall.png"), "Mask", this, SLOT(addKeyerItem())); 
    this->contextMenuMixer->addAction(QIcon(":/Graphics/Images/OpacitySmall.png"), "Opacity", this, SLOT(addOpacityItem()));
    this->contextMenuMixer->addAction(QIcon(":/Graphics/Images/ResetSmall.png"), "Reset", this, SLOT(addResetItem()));
    this->contextMenuMixer->addAction(QIcon(":/Graphics/Images/RotationSmall.png"), "Rotation", this, SLOT(addRotationItem()));
    this->contextMenuMixer->addAction(QIcon(":/Graphics/Images/SaturationSmall.png"), "Saturation", this, SLOT(addSaturationItem()));
    this->contextMenuMixer->addAction(QIcon(":/Graphics/Images/FillSmall.png"), "Transform", this, SLOT(addFillItem()));
    this->contextMenuMixer->addAction(QIcon(":/Graphics/Images/VolumeSmall.png"), "Volume", this, SLOT(addVolumeItem()));

    this->contextMenuLibrary = new QMenu(this);
    this->contextMenuLibrary->setObjectName("contextMenuLibrary");
    this->contextMenuLibrary->setTitle("Library");
    //this->contextMenuLibrary->setIcon(QIcon(":/Graphics/Images/Library.png"));
    this->contextMenuLibrary->addAction(QIcon(":/Graphics/Images/AudioSmall.png"), "Audio", this, SLOT(addAudioItem()));
    this->contextMenuLibrary->addAction(QIcon(":/Graphics/Images/StillSmall.png"), "Image", this, SLOT(addImageItem()));
    this->contextMenuLibrary->addAction(QIcon(":/Graphics/Images/ImageScrollerSmall.png"), "Image Scroller", this, SLOT(addImageScrollerItem()));
    this->contextMenuLibrary->addAction(QIcon(":/Graphics/Images/TemplateSmall.png"), "Template", this, SLOT(addTemplateItem()));
    this->contextMenuLibrary->addAction(QIcon(":/Graphics/Images/MovieSmall.png"), "Video", this, SLOT(addVideoItem()));

    this->contextMenuOther = new QMenu(this);
    this->contextMenuOther->setObjectName("contextMenuOther");
    this->contextMenuOther->setTitle("Other");
    //this->contextMenuOther->setIcon(QIcon(":/Graphics/Images/Other.png"));
    this->contextMenuOther->addAction(QIcon(":/Graphics/Images/SeparatorSmall.png"), "Autoplay Gateway", this, SLOT(addAutoPlayGatewayItem()));
    this->contextMenuOther->addAction(QIcon(":/Graphics/Images/SeparatorSmall.png"), "Autoplay Gateway Exit", this, SLOT(addAutoPlayGatewayExitItem()));
    this->contextMenuOther->addAction(QIcon(":/Graphics/Images/SeparatorSmall.png"), "Command Gateway", this, SLOT(addCommandGatewayItem()));
    this->contextMenuOther->addAction(QIcon(":/Graphics/Images/SeparatorSmall.png"), "Command Gateway Exit", this, SLOT(addCommandGatewayExitItem()));
    this->contextMenuOther->addAction(QIcon(":/Graphics/Images/SeparatorSmall.png"), "Gateway", this, SLOT(addFocusGatewayItem()));
    this->contextMenuOther->addAction(QIcon(":/Graphics/Images/SeparatorSmall.png"), "Gateway Exit", this, SLOT(addFocusGatewayExitItem()));
    this->contextMenuOther->addAction(QIcon(":/Graphics/Images/SnapshotSmall.png"), "Channel Snapshot", this, SLOT(addPrintItem()));
    this->contextMenuOther->addAction(QIcon(":/Graphics/Images/ClearSmall.png"), "Clear Output", this, SLOT(addClearOutputItem()));
    this->contextMenuOther->addAction(QIcon(":/Graphics/Images/CustomCommandSmall.png"), "Custom Command", this, SLOT(addCustomCommandItem()));
    this->contextMenuOther->addAction(QIcon(":/Graphics/Images/DeckLinkProducerSmall.png"), "DeckLink Input", this, SLOT(addDeckLinkInputItem()));
    this->contextMenuOther->addAction(QIcon(":/Graphics/Images/SolidColorSmall.png"), "Fade Out", this, SLOT(addFadeToBlackItem()));
    this->contextMenuOther->addAction(QIcon(":/Graphics/Images/FileRecorderSmall.png"), "File Recorder", this, SLOT(addFileRecorderItem()));
    this->contextMenuOther->addAction(QIcon(":/Graphics/Images/GpiOutputSmall.png"), "GPI Output", this, SLOT(addGpiOutputItem()));
    this->contextMenuOther->addAction(QIcon(":/Graphics/Images/HtmlSmall.png"), "HTML Page", this, SLOT(addHtmlItem()));
    this->contextMenuOther->addAction(QIcon(":/Graphics/Images/HttpGetSmall.png"), "HTTP GET Request", this, SLOT(addHttpGetItem()));
    this->contextMenuOther->addAction(QIcon(":/Graphics/Images/HttpGetSmall.png"), "HTTP POST Request", this, SLOT(addHttpPostItem()));
    this->contextMenuOther->addAction(QIcon(":/Graphics/Images/CustomCommandSmall.png"), "Shell Command", this, SLOT(addShellCommandItem()));
    this->contextMenuOther->addAction(QIcon(":/Graphics/Images/OscOutputSmall.png"), "OSC Output", this, SLOT(addOscOutputItem()));
    this->contextMenuOther->addAction(QIcon(":/Graphics/Images/PlayoutCommandSmall.png"), "Playout Command", this, SLOT(addPlayoutCommandItem()));
    this->contextMenuOther->addAction(QIcon(":/Graphics/Images/RouteChannelSmall.png"), "Route Channel", this, SLOT(addRouteChannelItem()));
    this->contextMenuOther->addAction(QIcon(":/Graphics/Images/RouteVideolayerSmall.png"), "Route Video Layer", this, SLOT(addRouteVideolayerItem()));
    this->contextMenuOther->addAction(QIcon(":/Graphics/Images/SeparatorSmall.png"), "Separator", this, SLOT(addSeparatorItem()));
    this->contextMenuOther->addAction(QIcon(":/Graphics/Images/ClearSmall.png"), "Stop All Auto-Loops", this, SLOT(addStopAutoLoopsItem()));
    this->contextMenuOther->addAction(QIcon(":/Graphics/Images/SolidColorSmall.png"), "Solid Color", this, SLOT(addSolidColorItem()));

    this->contextMenuTools = new QMenu(this);
    this->contextMenuTools->setTitle("Tools");
    //this->contextMenuTools->setIcon(QIcon(":/Graphics/Images/New.png"));
    this->contextMenuTools->addMenu(this->contextMenuLibrary);
    this->contextMenuTools->addMenu(this->contextMenuMixer);
    this->contextMenuTools->addMenu(this->contextMenuOther);

    this->contextMenuMark = new QMenu(this);
    this->contextMenuMark->setTitle("Mark Item");
    this->contextMenuMark->addAction(/*QIcon(":/Graphics/Images/RenameRundown.png"),*/ "As Used", this, SLOT(markItemAsUsed()));
    this->contextMenuMark->addAction(/*QIcon(":/Graphics/Images/RenameRundown.png"),*/ "As Unused", this, SLOT(markItemAsUnused()));
    this->contextMenuMark->addAction(/*QIcon(":/Graphics/Images/RenameRundown.png"),*/ "All as Used", this, SLOT(markAllItemsAsUsed()));
    this->contextMenuMark->addAction(/*QIcon(":/Graphics/Images/RenameRundown.png"),*/ "All as Unused", this, SLOT(markAllItemsAsUnused()));

    this->contextMenuColor = new QMenu(this);
    this->contextMenuColor->setTitle("Colorize Item");
    //this->contextMenuColor->setIcon(QIcon(":/Graphics/Images/Color.png"));
    // Reds
    this->contextMenuColor->addAction("Bright Red");
    this->contextMenuColor->addAction("Dark Red");
    this->contextMenuColor->addAction("Soft Red");
    this->contextMenuColor->addAction("Rose");
    this->contextMenuColor->addSeparator();
    // Oranges
    this->contextMenuColor->addAction("Orange");
    this->contextMenuColor->addAction("Burnt Orange");
    this->contextMenuColor->addAction("Peach");
    this->contextMenuColor->addSeparator();
    // Yellows
    this->contextMenuColor->addAction("Gold");
    this->contextMenuColor->addAction("Yellow");
    this->contextMenuColor->addAction("Amber");
    this->contextMenuColor->addSeparator();
    // Greens
    this->contextMenuColor->addAction("Green");
    this->contextMenuColor->addAction("Dark Green");
    this->contextMenuColor->addAction("Lime");
    this->contextMenuColor->addAction("Olive");
    this->contextMenuColor->addAction("Teal");
    this->contextMenuColor->addAction("Mint");
    this->contextMenuColor->addSeparator();
    // Blues
    this->contextMenuColor->addAction("Blue");
    this->contextMenuColor->addAction("Dark Blue");
    this->contextMenuColor->addAction("Light Blue");
    this->contextMenuColor->addAction("Sky Blue");
    this->contextMenuColor->addAction("Navy");
    this->contextMenuColor->addAction("Cyan");
    this->contextMenuColor->addSeparator();
    // Purples
    this->contextMenuColor->addAction("Purple");
    this->contextMenuColor->addAction("Dark Purple");
    this->contextMenuColor->addAction("Lavender");
    this->contextMenuColor->addAction("Magenta");
    this->contextMenuColor->addSeparator();
    // Neutrals & Browns
    this->contextMenuColor->addAction("Brown");
    this->contextMenuColor->addAction("Warm Gray");
    this->contextMenuColor->addAction("Cool Gray");
    this->contextMenuColor->addAction("Charcoal");
    this->contextMenuColor->addSeparator();
    this->contextMenuColor->addAction("Reset");

    foreach (QAction* action, this->contextMenuColor->actions())
    {
        action->setCheckable(true);
        action->setChecked(false);
        action->font().setItalic(true);
    }

    this->contextMenuRundown = new QMenu(this);
    this->contextMenuRundown->addMenu(this->contextMenuTools);
    this->contextMenuRundown->addSeparator();
    this->contextMenuRundown->addAction(/*QIcon(":/Graphics/Images/GroupSmall.png"),*/ "Group");
    this->contextMenuRundown->addAction(/*QIcon(":/Graphics/Images/UngroupSmall.png"),*/ "Ungroup");
    this->contextMenuRundown->addSeparator();
    this->contextMenuRundown->addMenu(this->contextMenuMark);
    this->contextMenuRundown->addSeparator();
    this->contextMenuRundown->addAction("Copy Properties", this, SLOT(copyItemProperties()));
    this->contextMenuRundown->addAction("Paste Properties", this, SLOT(pasteItemProperties()));
    this->contextMenuRundown->addAction("Paste Properties (No Data)", this, SLOT(pasteItemPropertiesNoData()));
    this->contextMenuRundown->addSeparator();
    this->contextMenuRundown->addAction("Create Linked Clone", this, SLOT(createLinkedClone()));
    this->contextMenuRundown->addAction("Unlink Clone", this, SLOT(unlinkClone()));
    this->contextMenuRundown->addSeparator();
    this->contextMenuRundown->addAction("Absorb Transforms", this, SLOT(absorbTransforms()));
    this->contextMenuRundown->addAction("Extract Transforms", this, SLOT(extractTransforms()));
    this->contextMenuRundown->addSeparator();
    this->addGatewayExitAction = this->contextMenuRundown->addAction("Add Gateway Exit", this, SLOT(addGatewayExit()));
    this->contextMenuRundown->addSeparator();
    this->contextMenuRundown->addMenu(this->contextMenuColor);
    this->contextMenuRundown->addSeparator();

    // Auto-Loop submenu (visible only for Movie/Still/Template; see customContextMenuRequested).
    this->contextMenuAutoLoop = new QMenu(tr("Auto-Loop"), this);
    this->actionAutoLoopEnable = this->contextMenuAutoLoop->addAction(tr("Enable Auto-Loop"));
    this->actionAutoLoopEnable->setCheckable(true);
    QObject::connect(this->actionAutoLoopEnable, SIGNAL(triggered()), this, SLOT(autoLoopEnableTriggered()));
    this->contextMenuAutoLoop->addSeparator();
    this->actionAutoLoopDelay5 = this->contextMenuAutoLoop->addAction(tr("Every 5 seconds"));
    this->actionAutoLoopDelay10 = this->contextMenuAutoLoop->addAction(tr("Every 10 seconds"));
    this->actionAutoLoopDelay30 = this->contextMenuAutoLoop->addAction(tr("Every 30 seconds"));
    this->actionAutoLoopDelay60 = this->contextMenuAutoLoop->addAction(tr("Every 60 seconds"));
    this->actionAutoLoopDelay5->setCheckable(true);
    this->actionAutoLoopDelay10->setCheckable(true);
    this->actionAutoLoopDelay30->setCheckable(true);
    this->actionAutoLoopDelay60->setCheckable(true);
    QObject::connect(this->actionAutoLoopDelay5, &QAction::triggered, this, [this]() { autoLoopDelayPresetTriggered(5); });
    QObject::connect(this->actionAutoLoopDelay10, &QAction::triggered, this, [this]() { autoLoopDelayPresetTriggered(10); });
    QObject::connect(this->actionAutoLoopDelay30, &QAction::triggered, this, [this]() { autoLoopDelayPresetTriggered(30); });
    QObject::connect(this->actionAutoLoopDelay60, &QAction::triggered, this, [this]() { autoLoopDelayPresetTriggered(60); });
    this->contextMenuAutoLoop->addSeparator();
    this->actionAutoLoopDelayCustom = this->contextMenuAutoLoop->addAction(tr("Custom Delay..."));
    QObject::connect(this->actionAutoLoopDelayCustom, SIGNAL(triggered()), this, SLOT(autoLoopDelayCustomTriggered()));
    this->contextMenuRundown->addMenu(this->contextMenuAutoLoop);
    this->contextMenuRundown->addSeparator();

    this->contextMenuRundown->addAction(/*QIcon(":/Graphics/Images/PresetSmall.png"),*/ "Save as Preset...", this, SLOT(saveAsPreset()));
    this->contextMenuRundown->addSeparator();
    this->contextMenuRundown->addAction(/*QIcon(":/Graphics/Images/Remove.png"),*/ "Remove", this, SLOT(removeSelectedItems()));

    QObject::connect(this->contextMenuTools, SIGNAL(triggered(QAction*)), this, SLOT(contextMenuNewTriggered(QAction*)));
    QObject::connect(this->contextMenuColor, SIGNAL(triggered(QAction*)), this, SLOT(contextMenuColorTriggered(QAction*)));
    QObject::connect(this->contextMenuRundown, SIGNAL(triggered(QAction*)), this, SLOT(contextMenuRundownTriggered(QAction*)));
}

void RundownTreeWidget::executePlayoutCommand(const ExecutePlayoutCommandEvent& event)
{
    if (!this->active)
        return;

    Playout::PlayoutType type;

    if (event.getHasPlayoutType())
    {
        type = event.getPlayoutType();
    }
    else
    {
        // Legacy key-based dispatch (kept for backward compatibility).
        if (event.getKey() == Qt::Key_F1)
            type = Playout::PlayoutType::Stop;
        else if (event.getKey() == Qt::Key_F2 && event.getModifiers() == Qt::ShiftModifier)
            type = Playout::PlayoutType::PlayNow;
        else if (event.getKey() == Qt::Key_F2)
            type = Playout::PlayoutType::Play;
        else if (event.getKey() == Qt::Key_F3)
            type = Playout::PlayoutType::Load;
        else if (event.getKey() == Qt::Key_F4)
            type = Playout::PlayoutType::PauseResume;
        else if (event.getKey() == Qt::Key_F5)
            type = Playout::PlayoutType::Next;
        else if (event.getKey() == Qt::Key_F6)
            type = Playout::PlayoutType::Update;
        else if (event.getKey() == Qt::Key_F7)
            type = Playout::PlayoutType::Invoke;
        else if (event.getKey() == Qt::Key_F8)
            type = Playout::PlayoutType::Preview;
        else if (event.getKey() == Qt::Key_F10)
            type = Playout::PlayoutType::Clear;
        else if (event.getKey() == Qt::Key_F11)
            type = Playout::PlayoutType::ClearVideoLayer;
        else if (event.getKey() == Qt::Key_F12)
            type = Playout::PlayoutType::ClearChannel;
        else
            return;
    }

    executeCommand(type, Action::ActionType::KeyPress, nullptr, true);
}

void RundownTreeWidget::saveAsPreset(const SaveAsPresetEvent& event)
{
    Q_UNUSED(event);

    if (!this->active)
        return;

    saveAsPreset();
}

void RundownTreeWidget::copyItemProperties()
{
    EventManager::getInstance().fireCopyItemPropertiesEvent(CopyItemPropertiesEvent());
}

void RundownTreeWidget::pasteItemProperties()
{
    EventManager::getInstance().firePasteItemPropertiesEvent(PasteItemPropertiesEvent());
}

void RundownTreeWidget::pasteItemPropertiesNoData()
{
    EventManager::getInstance().firePasteItemPropertiesEvent(PasteItemPropertiesEvent(true));
}

void RundownTreeWidget::copyItemProperties(const CopyItemPropertiesEvent& event)
{
    Q_UNUSED(event);

    if (!this->active)
        return;

    if (this->treeWidgetRundown->currentItem() == NULL)
        return;

    this->treeWidgetRundown->copyItemProperties();
}

void RundownTreeWidget::pasteItemProperties(const PasteItemPropertiesEvent& event)
{
    if (!this->active)
        return;

    if (event.getNoData())
        this->treeWidgetRundown->pasteItemPropertiesNoData();
    else
        this->treeWidgetRundown->pasteItemProperties();
}

bool RundownTreeWidget::removeSelectedItems()
{
    this->treeWidgetRundown->removeSelectedItems();

    return true;
}

void RundownTreeWidget::markItemAsUsed()
{
    EventManager::getInstance().fireMarkItemAsUsedEvent(MarkItemAsUsedEvent());
}

void RundownTreeWidget::markItemAsUnused()
{
    EventManager::getInstance().fireMarkItemAsUnusedEvent(MarkItemAsUnusedEvent());
}

void RundownTreeWidget::markAllItemsAsUsed()
{
    EventManager::getInstance().fireMarkAllItemsAsUsedEvent(MarkAllItemsAsUsedEvent());
}

void RundownTreeWidget::markAllItemsAsUnused()
{
    EventManager::getInstance().fireMarkAllItemsAsUnusedEvent(MarkAllItemsAsUnusedEvent());
}

void RundownTreeWidget::addPresetItem(const AddPresetItemEvent& event)
{
    if (!this->active)
        return;

    insertPresetItem(event.getPreset());
}

void RundownTreeWidget::insertPresetItem(const QString& preset)
{
    qApp->clipboard()->setText(preset);
    pasteSelectedItems();
    this->treeWidgetRundown->selectItemBelow();
}

void RundownTreeWidget::compactView(const CompactViewEvent& event)
{
    Q_UNUSED(event);

    if (!this->active)
        return;

    if (this->treeWidgetRundown->invisibleRootItem()->childCount() == 0)
        return;

    this->treeWidgetRundown->setUpdatesEnabled(false);

    for (int i = 0; i < this->treeWidgetRundown->invisibleRootItem()->childCount(); i++)
    {
        QTreeWidgetItem* item = this->treeWidgetRundown->invisibleRootItem()->child(i);
        QWidget* widget = dynamic_cast<QWidget*>(this->treeWidgetRundown->itemWidget(item, 0));

        dynamic_cast<AbstractRundownWidget*>(widget)->setCompactView(!this->treeWidgetRundown->getCompactView());
        if (this->treeWidgetRundown->getCompactView())
            widget->setFixedHeight(Rundown::DEFAULT_ITEM_HEIGHT);
        else
            widget->setFixedHeight(Rundown::COMPACT_ITEM_HEIGHT);

        for (int j = 0; j < item->childCount(); j++)
        {
            QTreeWidgetItem* child = item->child(j);
            QWidget* widget = dynamic_cast<QWidget*>(this->treeWidgetRundown->itemWidget(child, 0));

            dynamic_cast<AbstractRundownWidget*>(widget)->setCompactView(!this->treeWidgetRundown->getCompactView());
            if (this->treeWidgetRundown->getCompactView())
                widget->setFixedHeight(Rundown::DEFAULT_ITEM_HEIGHT);
            else
                widget->setFixedHeight(Rundown::COMPACT_ITEM_HEIGHT);
        }
    }

    this->treeWidgetRundown->doItemsLayout(); // Refresh
    this->treeWidgetRundown->setCompactView(!this->treeWidgetRundown->getCompactView());

    this->treeWidgetRundown->setUpdatesEnabled(true);
}

void RundownTreeWidget::executeRundownItem(const ExecuteRundownItemEvent& event)
{
    if (event.getItem()->treeWidget() == this->treeWidgetRundown)
        executeCommand(event.getType(), Action::ActionType::KeyPress, event.getItem(), true);
}

void RundownTreeWidget::clearDelayedCommands()
{
    if (!this->active)
        return;

    if (this->currentPlayingAutoStepItem != nullptr)
    {
        for (int i = 0; i < this->currentPlayingAutoStepItem->childCount(); i++)
        {
            QWidget* childWidget = this->treeWidgetRundown->itemWidget(this->currentPlayingAutoStepItem->child(i), 0);
            dynamic_cast<AbstractRundownWidget*>(childWidget)->clearDelayedCommands();
        }
    }
}

void RundownTreeWidget::allowRemoteTriggering(const AllowRemoteTriggeringEvent& event)
{
    if (!this->active)
        return;

    this->allowRemoteRundownTriggering = event.getEnabled();

    (this->allowRemoteRundownTriggering == true) ? configureOscSubscriptions() : resetOscSubscriptions();
}

void RundownTreeWidget::addRudnownItem(const AddRudnownItemEvent& event)
{
    if (!this->active)
        return;

    insertRundownItem(event.getLibraryModel());
}

void RundownTreeWidget::insertRundownItem(const LibraryModel& model)
{
    // Paired gateway insert helper (shared by AutoPlay Gateway, Focus Gateway, and Command Gateway).
    if (model.getType() == Rundown::AUTOPLAYGATEWAY || model.getType() == Rundown::FOCUSGATEWAY || model.getType() == Rundown::COMMANDGATEWAY)
    {
        // Find the next unique gateway number by scanning existing items of the same type.
        int maxNumber = 0;
        QString targetType = model.getType();
        QTreeWidgetItem* root = this->treeWidgetRundown->invisibleRootItem();
        for (int i = 0; i < root->childCount(); i++)
        {
            QTreeWidgetItem* topItem = root->child(i);
            QWidget* w = this->treeWidgetRundown->itemWidget(topItem, 0);
            AbstractRundownWidget* rw = dynamic_cast<AbstractRundownWidget*>(w);
            if (rw != nullptr && rw->getLibraryModel()->getType() == targetType)
            {
                bool ok;
                int num = rw->getLibraryModel()->getLabel().split(' ').last().toInt(&ok);
                if (ok && num > maxNumber) maxNumber = num;
            }
            if (rw != nullptr && rw->isGroup())
            {
                for (int j = 0; j < topItem->childCount(); j++)
                {
                    QTreeWidgetItem* childItem = topItem->child(j);
                    QWidget* cw = this->treeWidgetRundown->itemWidget(childItem, 0);
                    AbstractRundownWidget* crw = dynamic_cast<AbstractRundownWidget*>(cw);
                    if (crw != nullptr && crw->getLibraryModel()->getType() == targetType)
                    {
                        bool ok;
                        int num = crw->getLibraryModel()->getLabel().split(' ').last().toInt(&ok);
                        if (ok && num > maxNumber) maxNumber = num;
                    }
                    // Also scan inside inner groups.
                    if (crw != nullptr && crw->isGroup())
                    {
                        for (int k = 0; k < childItem->childCount(); k++)
                        {
                            QWidget* gcw = this->treeWidgetRundown->itemWidget(childItem->child(k), 0);
                            AbstractRundownWidget* gcrw = dynamic_cast<AbstractRundownWidget*>(gcw);
                            if (gcrw != nullptr && gcrw->getLibraryModel()->getType() == targetType)
                            {
                                bool ok;
                                int num = gcrw->getLibraryModel()->getLabel().split(' ').last().toInt(&ok);
                                if (ok && num > maxNumber) maxNumber = num;
                            }
                        }
                    }
                }
            }
        }
        int gatewayNumber = maxNumber + 1;
        QString typeName = (targetType == Rundown::FOCUSGATEWAY) ? "Gateway"
                         : (targetType == Rundown::COMMANDGATEWAY) ? "Command Gateway"
                         : "Autoplay Gateway";
        QString gatewayLabel = QString("%1 %2").arg(typeName).arg(gatewayNumber);

        // Create model with the numbered label.
        LibraryModel numberedModel(model.getId(), gatewayLabel, model.getName(), model.getDeviceName(), model.getType(), model.getThumbnailId(), model.getTimecode());

        QString gatewayId = QUuid::createUuid().toString();
        bool compact = this->treeWidgetRundown->getCompactView();
        int height = compact ? Rundown::COMPACT_ITEM_HEIGHT : Rundown::DEFAULT_ITEM_HEIGHT;

        auto insertOneWidget = [&](AbstractRundownWidget* w) -> QTreeWidgetItem* {
            w->setCompactView(compact);
            QTreeWidgetItem* treeItem = new QTreeWidgetItem();
            if (this->treeWidgetRundown->currentItem() == NULL)
                this->treeWidgetRundown->invisibleRootItem()->addChild(treeItem);
            else if (this->treeWidgetRundown->currentItem()->parent() == NULL)
                this->treeWidgetRundown->invisibleRootItem()->insertChild(this->treeWidgetRundown->currentIndex().row() + 1, treeItem);
            else
            {
                this->treeWidgetRundown->currentItem()->parent()->insertChild(this->treeWidgetRundown->currentIndex().row() + 1, treeItem);
                w->setInGroup(true);
            }
            this->treeWidgetRundown->setItemWidget(treeItem, 0, dynamic_cast<QWidget*>(w));
            this->treeWidgetRundown->setCurrentItem(treeItem);
            dynamic_cast<QWidget*>(w)->setFixedHeight(height);
            return treeItem;
        };

        AbstractRundownWidget* entranceWidget = RundownItemFactory::getInstance().createWidget(numberedModel);
        if (entranceWidget == NULL) return;
        GatewayCommand* entranceCmd = dynamic_cast<GatewayCommand*>(entranceWidget->getCommand());
        if (entranceCmd == nullptr) return;
        entranceCmd->setGatewayId(gatewayId);
        QTreeWidgetItem* entranceTreeItem = insertOneWidget(entranceWidget);

        AbstractRundownWidget* exitWidget = RundownItemFactory::getInstance().createWidget(numberedModel);
        if (exitWidget == NULL) return;
        GatewayCommand* exitCmd = dynamic_cast<GatewayCommand*>(exitWidget->getCommand());
        if (exitCmd == nullptr) return;
        exitCmd->setGatewayId(gatewayId);
        exitCmd->setIsExit(true);
        exitCmd->setExitLabel("Exit A");
        // Set the entrance's selected exit to match the first exit.
        entranceCmd->setSelectedExitLabel("Exit A");
        if (auto* apw = dynamic_cast<RundownAutoPlayGatewayWidget*>(exitWidget))
            apw->updateVisuals();
        else if (auto* fgw = dynamic_cast<RundownFocusGatewayWidget*>(exitWidget))
            fgw->updateVisuals();
        else if (auto* cgw = dynamic_cast<RundownCommandGatewayWidget*>(exitWidget))
            cgw->updateVisuals();
        QTreeWidgetItem* exitTreeItem = insertOneWidget(exitWidget);

        // Wire gateway widgets with treeItem references and signal connections.
        wireGatewayWidget(entranceWidget, entranceTreeItem);
        wireGatewayWidget(exitWidget, exitTreeItem);
        EventManager::getInstance().fireGatewayExitsChangedEvent(gatewayId);

        if (this->active)
            this->treeWidgetRundown->setFocus();
        else
            this->treeWidgetRundown->selectionModel()->clearSelection();

        if (this->treeWidgetRundown->updatesEnabled())
        {
            this->treeWidgetRundown->doItemsLayout();
            this->treeWidgetRundown->repaint();
            this->treeWidgetRundown->checkEmptyRundown();
        }
        return;
    }

    AbstractRundownWidget* widget = RundownItemFactory::getInstance().createWidget(model);
    if (widget == NULL)
        return;

    // Apply the Library panel's channel/layer selector values.
    if (widget->getCommand() != nullptr)
    {
        widget->getCommand()->setChannel(LibraryWidget::dropChannel());
        widget->getCommand()->setVideolayer(LibraryWidget::dropVideolayer());
    }

    widget->setCompactView(this->treeWidgetRundown->getCompactView());

    QTreeWidgetItem* item = new QTreeWidgetItem();
    if (this->treeWidgetRundown->currentItem() == NULL) // There is no item selected.
        this->treeWidgetRundown->invisibleRootItem()->addChild(item); // Add item to the bottom of the rundown.
    else if (this->treeWidgetRundown->currentItem()->parent() == NULL) // Top level item.
        this->treeWidgetRundown->invisibleRootItem()->insertChild(this->treeWidgetRundown->currentIndex().row() + 1, item); // Insert item below.
    else if (this->treeWidgetRundown->currentItem()->parent() != NULL) // Goup item.
    {
        this->treeWidgetRundown->currentItem()->parent()->insertChild(this->treeWidgetRundown->currentIndex().row() + 1, item); // Insert item below.
        widget->setInGroup(true);
    }

    this->treeWidgetRundown->setItemWidget(item, 0, dynamic_cast<QWidget*>(widget));
    this->treeWidgetRundown->setCurrentItem(item);

    // Only take focus and show selection if this pane is active.
    if (this->active)
        this->treeWidgetRundown->setFocus();
    else
        this->treeWidgetRundown->selectionModel()->clearSelection();

    if (this->treeWidgetRundown->getCompactView())
        dynamic_cast<QWidget*>(widget)->setFixedHeight(Rundown::COMPACT_ITEM_HEIGHT);
    else
        dynamic_cast<QWidget*>(widget)->setFixedHeight(Rundown::DEFAULT_ITEM_HEIGHT);

    if (this->treeWidgetRundown->updatesEnabled())
    {
        this->treeWidgetRundown->doItemsLayout();
        this->treeWidgetRundown->repaint();
        this->treeWidgetRundown->checkEmptyRundown();
    }
}

void RundownTreeWidget::autoPlayChanged(const AutoPlayChangedEvent& event)
{
    if (!this->active)
        return;

    QTreeWidgetItem* current = this->treeWidgetRundown->currentItem();
    if (current == nullptr)
        return;

    for (int i = 0; i < current->childCount(); i++)
    {
        QTreeWidgetItem* childTreeItem = current->child(i);
        QWidget* childWidget = this->treeWidgetRundown->itemWidget(childTreeItem, 0);
        AbstractRundownWidget* childRundownWidget = dynamic_cast<AbstractRundownWidget*>(childWidget);
        if (childRundownWidget == nullptr)
            continue;

        if (dynamic_cast<MovieCommand*>(childRundownWidget->getCommand()))
            dynamic_cast<MovieCommand*>(childRundownWidget->getCommand())->setAutoPlay(event.getAutoPlay());
        else if (dynamic_cast<StillCommand*>(childRundownWidget->getCommand()))
            dynamic_cast<StillCommand*>(childRundownWidget->getCommand())->setAutoPlay(event.getAutoPlay());
        else if (dynamic_cast<TemplateCommand*>(childRundownWidget->getCommand()))
            dynamic_cast<TemplateCommand*>(childRundownWidget->getCommand())->setAutoPlay(event.getAutoPlay());

        // Also sync inner group children.
        if (childRundownWidget->isGroup())
        {
            for (int j = 0; j < childTreeItem->childCount(); j++)
            {
                QWidget* gcw = this->treeWidgetRundown->itemWidget(childTreeItem->child(j), 0);
                AbstractRundownWidget* gcRundown = dynamic_cast<AbstractRundownWidget*>(gcw);
                if (gcRundown == nullptr)
                    continue;
                if (MovieCommand* mc = dynamic_cast<MovieCommand*>(gcRundown->getCommand()))
                    mc->setAutoPlay(event.getAutoPlay());
                else if (StillCommand* sc = dynamic_cast<StillCommand*>(gcRundown->getCommand()))
                    sc->setAutoPlay(event.getAutoPlay());
                else if (TemplateCommand* tc = dynamic_cast<TemplateCommand*>(gcRundown->getCommand()))
                    tc->setAutoPlay(event.getAutoPlay());
            }
        }
    }
}

void RundownTreeWidget::autoPlayRundownItem(const AutoPlayRundownItemEvent& event)
{
    if (!this->active)
        return;

    QWidget* sourceWidget = event.getSource();
    if (sourceWidget == nullptr)
        return;

    AbstractRundownWidget* rundownWidget = dynamic_cast<AbstractRundownWidget*>(sourceWidget);
    if (rundownWidget == nullptr)
        return;

    // Helper to fire channel activity event for the status panel.
    auto fireAutoPlayActivity = [](AbstractRundownWidget* widget) {
        if (widget == nullptr || widget->getCommand() == nullptr)
            return;

        int ch = widget->getCommand()->getChannel();
        int vl = widget->getCommand()->getVideolayer();
        QString label = widget->getLibraryModel()->getLabel();
        if (label.isEmpty()) label = widget->getLibraryModel()->getName();
        if (label.isEmpty()) label = widget->getLibraryModel()->getType();
        QString itemType = widget->getLibraryModel()->getType();

        // Skip activity if file not in library.
        if (itemType == Rundown::MOVIE || itemType == Rundown::STILL ||
            itemType == Rundown::AUDIO || itemType == Rundown::IMAGESCROLLER ||
            itemType == Rundown::TEMPLATE)
        {
            QString name = widget->getLibraryModel()->getName();
            QString deviceName = widget->getLibraryModel()->getDeviceName();
            const QSharedPointer<DeviceModel> dm = DeviceManager::getInstance().getDeviceModelByName(deviceName);
            if (dm == nullptr || DatabaseManager::getInstance().getLibraryByNameAndDeviceId(name, dm->getId()).isEmpty())
                return;
        }

        EventManager::getInstance().fireChannelActivityEvent(
            ChannelActivityEvent(ch, vl, label, itemType, true));
    };

    for (int i = 0; i < this->autoPlayQueues.count(); i++)
    {
        AutoPlayQueueInfo& queueInfo = this->autoPlayQueues[i];
        if (!queueInfo.queue->contains(rundownWidget))
            continue;

        // Only process events from the currently-playing item (position 0).
        // Stale events from widgets elsewhere in the queue must be ignored.
        if (queueInfo.queue->indexOf(rundownWidget) != 0)
            break;

        bool previousIsMovie = dynamic_cast<MovieCommand*>(rundownWidget->getCommand()) != nullptr;
        bool previousIsStill = dynamic_cast<StillCommand*>(rundownWidget->getCommand()) != nullptr;

        // For video->still transitions: Movies fire this event early (first OSC frame)
        // for LOADBG AUTO preloading. But stills would play immediately, overwriting the
        // video. Instead, defer until the video reaches end-of-clip.
        if (previousIsMovie && !queueInfo.pendingEndOfClip)
        {
            // Determine the next item to play (either next in queue, or first in loop).
            AbstractRundownWidget* peekNextWidget = nullptr;
            if (queueInfo.queue->count() > 1)
            {
                peekNextWidget = queueInfo.queue->at(1);
            }
            else if (queueInfo.groupItem != nullptr)
            {
                // Video is the last item. Check if the loop's first item is a still.
                QWidget* groupWidget = this->treeWidgetRundown->itemWidget(queueInfo.groupItem, 0);
                AbstractRundownWidget* groupRundownWidget = dynamic_cast<AbstractRundownWidget*>(groupWidget);
                if (groupRundownWidget != nullptr && groupRundownWidget->isGroup())
                {
                    GroupCommand* groupCommand = dynamic_cast<GroupCommand*>(groupRundownWidget->getCommand());
                    if (groupCommand != nullptr && groupCommand->getLoop())
                    {
                        for (int j = 0; j < queueInfo.groupItem->childCount() && peekNextWidget == nullptr; j++)
                        {
                            QTreeWidgetItem* childTreeItem = queueInfo.groupItem->child(j);
                            QWidget* childWidget = this->treeWidgetRundown->itemWidget(childTreeItem, 0);
                            AbstractRundownWidget* child = dynamic_cast<AbstractRundownWidget*>(childWidget);

                            if (child->isGroup())
                            {
                                if (childTreeItem->isExpanded())
                                {
                                    // Expanded inner group: peek at its first queueable child.
                                    for (int k = 0; k < childTreeItem->childCount(); k++)
                                    {
                                        QWidget* gcw = this->treeWidgetRundown->itemWidget(childTreeItem->child(k), 0);
                                        AbstractRundownWidget* gc = dynamic_cast<AbstractRundownWidget*>(gcw);
                                        bool gcShouldQueue = false;
                                        if (MovieCommand* mc = dynamic_cast<MovieCommand*>(gc->getCommand()))
                                            gcShouldQueue = mc->getAutoPlay();
                                        else if (StillCommand* sc = dynamic_cast<StillCommand*>(gc->getCommand()))
                                            gcShouldQueue = sc->getAutoPlay() && sc->getDuration() > 0;
                                        else if (TemplateCommand* tc = dynamic_cast<TemplateCommand*>(gc->getCommand()))
                                            gcShouldQueue = tc->getAutoPlay() && tc->getDuration() > 0;
                                        if (gcShouldQueue) { peekNextWidget = gc; break; }
                                    }
                                }
                                else
                                {
                                    // Collapsed inner group: treat as batch (triggers deferral).
                                    peekNextWidget = child;
                                }
                                continue;
                            }

                            bool childShouldQueue = false;
                            if (MovieCommand* mc = dynamic_cast<MovieCommand*>(child->getCommand()))
                                childShouldQueue = mc->getAutoPlay();
                            else if (StillCommand* sc = dynamic_cast<StillCommand*>(child->getCommand()))
                                childShouldQueue = sc->getAutoPlay() && sc->getDuration() > 0;
                            else if (TemplateCommand* tc = dynamic_cast<TemplateCommand*>(child->getCommand()))
                                childShouldQueue = tc->getAutoPlay() && tc->getDuration() > 0;
                            if (childShouldQueue)
                                peekNextWidget = child;
                        }
                    }
                }
            }

            // For top-level chains, peek at next top-level item across chain boundary.
            // We need to defer to end-of-clip for ALL cross-chain transitions (not just
            // video→still), because we can't use LOADBG AUTO across top-level items.
            bool crossChainPeek = false;
            if (peekNextWidget == nullptr && queueInfo.isTopLevelChain)
            {
                QTreeWidgetItem* topLevelItem = queueInfo.groupItem;
                if (topLevelItem == nullptr)
                    topLevelItem = findTopLevelTreeItem(rundownWidget);

                if (topLevelItem != nullptr)
                {
                    int idx = this->treeWidgetRundown->invisibleRootItem()->indexOfChild(topLevelItem);
                    if (idx >= 0 && idx + 1 < this->treeWidgetRundown->invisibleRootItem()->childCount())
                    {
                        QTreeWidgetItem* nextTopItem = this->treeWidgetRundown->invisibleRootItem()->child(idx + 1);
                        QWidget* nextTopWidget = this->treeWidgetRundown->itemWidget(nextTopItem, 0);
                        AbstractRundownWidget* nextRundown = dynamic_cast<AbstractRundownWidget*>(nextTopWidget);

                        if (nextRundown != nullptr)
                        {
                            bool nextHasAutoPlay = false;
                            if (nextRundown->isGroup())
                            {
                                if (GroupCommand* gc = dynamic_cast<GroupCommand*>(nextRundown->getCommand()))
                                    nextHasAutoPlay = gc->getAutoPlay();
                            }
                            else if (MovieCommand* mc = dynamic_cast<MovieCommand*>(nextRundown->getCommand()))
                                nextHasAutoPlay = mc->getAutoPlay();
                            else if (StillCommand* sc = dynamic_cast<StillCommand*>(nextRundown->getCommand()))
                                nextHasAutoPlay = sc->getAutoPlay() && sc->getDuration() > 0;
                            else if (TemplateCommand* tc = dynamic_cast<TemplateCommand*>(nextRundown->getCommand()))
                                nextHasAutoPlay = tc->getAutoPlay() && tc->getDuration() > 0;

                            if (nextHasAutoPlay)
                            {
                                peekNextWidget = nextRundown;
                                crossChainPeek = true;
                            }
                        }
                    }
                }
            }

            if (peekNextWidget != nullptr)
            {
                bool nextIsStill = dynamic_cast<StillCommand*>(peekNextWidget->getCommand()) != nullptr;
                bool nextIsGroup = peekNextWidget->isGroup();
                // Defer for video→still, video→collapsed-inner-group (can't LOADBG AUTO a batch),
                // and ALL cross-chain transitions since we can't use LOADBG AUTO.
                if (nextIsStill || nextIsGroup || crossChainPeek)
                {
                    // Defer: request the movie to fire again at end-of-clip.
                    RundownMovieWidget* movieWidget = dynamic_cast<RundownMovieWidget*>(event.getSource());
                    if (movieWidget != nullptr)
                    {
                        movieWidget->requestEndOfClipAutoPlay();
                        queueInfo.pendingEndOfClip = true;
                    }
                    break; // Don't advance the queue yet.
                }
            }
        }

        // Clear the pending flag if this is the end-of-clip callback.
        queueInfo.pendingEndOfClip = false;

        // Remove currently playing item.
        queueInfo.queue->removeAt(0);

        // Process any gateway items at the front of the queue.
        while (!queueInfo.queue->isEmpty())
        {
            AbstractRundownWidget* frontWidget = queueInfo.queue->at(0);
            if (frontWidget->getLibraryModel()->getType() != Rundown::AUTOPLAYGATEWAY)
                break; // Not a gateway, proceed to normal play.

            GatewayCommand* tCmd = dynamic_cast<GatewayCommand*>(frontWidget->getCommand());
            if (tCmd->getIsExit())
            {
                // Exit: passive marker, just skip.
                queueInfo.queue->removeAt(0);
                continue;
            }

            // Gateway entrance: find matching exit (by effectiveExitLabel) and rebuild queue from after exit.
            QString gatewayId = tCmd->getGatewayId();
            QString selectedExit = tCmd->getEffectiveExitLabel();

            QTreeWidgetItem* container = queueInfo.groupItem;
            QTreeWidgetItem* root = (container != nullptr) ? container : this->treeWidgetRundown->invisibleRootItem();
            int childCount = root->childCount();
            int exitIdx = -1, entranceIdx = -1;
            int firstExitIdx = -1;
            for (int j = 0; j < childCount; j++)
            {
                QWidget* cw = this->treeWidgetRundown->itemWidget(root->child(j), 0);
                AbstractRundownWidget* crw = dynamic_cast<AbstractRundownWidget*>(cw);
                if (crw == nullptr)
                    continue;
                if (crw == frontWidget)
                    entranceIdx = j;
                GatewayCommand* ct = dynamic_cast<GatewayCommand*>(crw->getCommand());
                if (ct != nullptr && ct->getIsExit() && ct->getGatewayId() == gatewayId)
                {
                    if (firstExitIdx < 0)
                        firstExitIdx = j;
                    if (ct->getExitLabel() == selectedExit)
                        exitIdx = j;
                }
            }
            // Fallback to first exit if selected label not found.
            if (exitIdx < 0)
                exitIdx = firstExitIdx;

            if (exitIdx < 0)
            {
                // No local exit: try cross-tab gateway.
                setAutoPlayHighlight(this->currentAutoPlayWidget, false);
                this->currentAutoPlayWidget = nullptr;
                delete queueInfo.queue;
                this->autoPlayQueues.removeAt(i);
                emit requestCrossTabGateway(gatewayId, selectedExit);
                return;
            }

            // Safety: if exit is before entrance with no playable items between, skip (prevents infinite loop).
            if (exitIdx < entranceIdx)
            {
                bool hasPlayable = false;
                for (int j = exitIdx + 1; j < entranceIdx; j++)
                {
                    QWidget* cw = this->treeWidgetRundown->itemWidget(root->child(j), 0);
                    AbstractRundownWidget* crw = dynamic_cast<AbstractRundownWidget*>(cw);
                    if (crw == nullptr)
                        continue;
                    if (MovieCommand* mc = dynamic_cast<MovieCommand*>(crw->getCommand()))
                    {
                        if (mc->getAutoPlay()) { hasPlayable = true; break; }
                    }
                    else if (StillCommand* sc = dynamic_cast<StillCommand*>(crw->getCommand()))
                    {
                        if (sc->getAutoPlay() && sc->getDuration() > 0) { hasPlayable = true; break; }
                    }
                    else if (TemplateCommand* tc = dynamic_cast<TemplateCommand*>(crw->getCommand()))
                    {
                        if (tc->getAutoPlay() && tc->getDuration() > 0) { hasPlayable = true; break; }
                    }
                }
                if (!hasPlayable)
                {
                    queueInfo.queue->removeAt(0);
                    continue;
                }
            }

            // Rebuild queue from exit+1 onwards, handling inner groups.
            queueInfo.queue->clear();
            for (int j = exitIdx + 1; j < childCount; j++)
            {
                QTreeWidgetItem* childTreeItem = root->child(j);
                QWidget* cw = this->treeWidgetRundown->itemWidget(childTreeItem, 0);
                AbstractRundownWidget* crw = dynamic_cast<AbstractRundownWidget*>(cw);

                if (crw->isGroup())
                {
                    if (childTreeItem->isExpanded())
                    {
                        // Expanded inner group: flatten children into queue.
                        for (int k = 0; k < childTreeItem->childCount(); k++)
                        {
                            QWidget* gcw = this->treeWidgetRundown->itemWidget(childTreeItem->child(k), 0);
                            AbstractRundownWidget* gcrw = dynamic_cast<AbstractRundownWidget*>(gcw);
                            bool gsq = false;
                            if (MovieCommand* mc = dynamic_cast<MovieCommand*>(gcrw->getCommand()))
                                gsq = mc->getAutoPlay();
                            else if (StillCommand* sc = dynamic_cast<StillCommand*>(gcrw->getCommand()))
                                gsq = sc->getAutoPlay() && sc->getDuration() > 0;
                            else if (TemplateCommand* tc = dynamic_cast<TemplateCommand*>(gcrw->getCommand()))
                                gsq = tc->getAutoPlay() && tc->getDuration() > 0;
                            else if (gcrw->getLibraryModel()->getType() == Rundown::AUTOPLAYGATEWAY)
                                gsq = true;
                            if (gsq)
                                queueInfo.queue->push_back(gcrw);
                        }
                    }
                    else
                    {
                        // Collapsed inner group: add as batch placeholder.
                        queueInfo.queue->push_back(crw);
                    }
                    continue;
                }

                bool sq = false;
                if (MovieCommand* mc = dynamic_cast<MovieCommand*>(crw->getCommand()))
                    sq = mc->getAutoPlay();
                else if (StillCommand* sc = dynamic_cast<StillCommand*>(crw->getCommand()))
                    sq = sc->getAutoPlay() && sc->getDuration() > 0;
                else if (TemplateCommand* tc = dynamic_cast<TemplateCommand*>(crw->getCommand()))
                    sq = tc->getAutoPlay() && tc->getDuration() > 0;
                else if (crw->getLibraryModel()->getType() == Rundown::AUTOPLAYGATEWAY)
                    sq = true;
                if (sq)
                    queueInfo.queue->push_back(crw);
            }
            // Loop continues to check if the new front is also a gateway.
        }

        // Handle collapsed inner groups at the front of the queue.
        // These are group widgets added as placeholders; batch-execute their children and skip.
        while (!queueInfo.queue->isEmpty() && queueInfo.queue->at(0)->isGroup())
        {
            AbstractRundownWidget* frontGroupWidget = queueInfo.queue->at(0);

            // Find the inner group's tree item within the outer group.
            QTreeWidgetItem* innerTreeItem = nullptr;
            for (int j = 0; j < queueInfo.groupItem->childCount(); j++)
            {
                QWidget* cw = this->treeWidgetRundown->itemWidget(queueInfo.groupItem->child(j), 0);
                if (dynamic_cast<AbstractRundownWidget*>(cw) == frontGroupWidget)
                {
                    innerTreeItem = queueInfo.groupItem->child(j);
                    break;
                }
            }

            // Batch-execute all children of the collapsed inner group.
            if (innerTreeItem != nullptr)
            {
                for (int j = 0; j < innerTreeItem->childCount(); j++)
                {
                    QWidget* gcw = this->treeWidgetRundown->itemWidget(innerTreeItem->child(j), 0);
                    AbstractRundownWidget* gcRundown = dynamic_cast<AbstractRundownWidget*>(gcw);
                    dynamic_cast<AbstractPlayoutCommand*>(gcRundown)->executeCommand(Playout::PlayoutType::Play);
                    fireAutoPlayActivity(gcRundown);
                }
            }

            queueInfo.queue->removeAt(0);
            // After a batch, force direct PLAY for the next item (no LOADBG AUTO).
            previousIsMovie = false;
            previousIsStill = true;
        }

        // Have more in queue, play them...
        if (!queueInfo.queue->isEmpty())
        {
            AbstractRundownWidget* rundownQueueWidget = dynamic_cast<AbstractRundownWidget*>(queueInfo.queue->at(0));
            bool nextIsMovie = dynamic_cast<MovieCommand*>(rundownQueueWidget->getCommand()) != nullptr;

            // When the previous item is a still (or a batch), the next movie must be played
            // directly (PLAY, not LOADBG AUTO). Stills are infinite producers on CasparCG
            // and never "end", so LOADBG AUTO would never trigger.
            if (previousIsStill && nextIsMovie)
                dynamic_cast<AbstractPlayoutCommand*>(rundownQueueWidget)->executeCommand(Playout::PlayoutType::Next);
            else
                dynamic_cast<AbstractPlayoutCommand*>(rundownQueueWidget)->executeCommand(Playout::PlayoutType::Play);

            setAutoPlayHighlight(this->currentAutoPlayWidget, false);
            this->currentAutoPlayWidget = rundownQueueWidget;
            setAutoPlayHighlight(this->currentAutoPlayWidget, true);
            fireAutoPlayActivity(rundownQueueWidget);
        }
        else
        {
            // Queue is empty. Check if we should loop.
            QWidget* groupWidget = this->treeWidgetRundown->itemWidget(queueInfo.groupItem, 0);
            AbstractRundownWidget* groupRundownWidget = dynamic_cast<AbstractRundownWidget*>(groupWidget);
            if (groupRundownWidget != nullptr && groupRundownWidget->isGroup())
            {
                GroupCommand* groupCommand = dynamic_cast<GroupCommand*>(groupRundownWidget->getCommand());
                if (groupCommand != nullptr && groupCommand->getLoop())
                {
                    // Loop is enabled. Repopulate queue and play first item.
                    // Build flat candidate list (same as initial queue build).
                    struct LoopCandidate { AbstractRundownWidget* widget; bool isCollapsedInnerGroup; QTreeWidgetItem* treeItem; };
                    QList<LoopCandidate> loopCandidates;
                    for (int j = 0; j < queueInfo.groupItem->childCount(); j++)
                    {
                        QTreeWidgetItem* childTreeItem = queueInfo.groupItem->child(j);
                        QWidget* childWidget = this->treeWidgetRundown->itemWidget(childTreeItem, 0);
                        AbstractRundownWidget* childRundown = dynamic_cast<AbstractRundownWidget*>(childWidget);

                        if (childRundown->isGroup())
                        {
                            if (childTreeItem->isExpanded())
                            {
                                for (int k = 0; k < childTreeItem->childCount(); k++)
                                {
                                    QTreeWidgetItem* gcTree = childTreeItem->child(k);
                                    QWidget* gcw = this->treeWidgetRundown->itemWidget(gcTree, 0);
                                    loopCandidates.append({dynamic_cast<AbstractRundownWidget*>(gcw), false, gcTree});
                                }
                            }
                            else
                            {
                                loopCandidates.append({childRundown, true, childTreeItem});
                            }
                        }
                        else
                        {
                            loopCandidates.append({childRundown, false, childTreeItem});
                        }
                    }

                    bool isFirstChild = true;
                    for (const LoopCandidate& lc : loopCandidates)
                    {
                        AbstractRundownWidget* rundownChildWidget = lc.widget;

                        if (lc.isCollapsedInnerGroup)
                        {
                            if (isFirstChild)
                            {
                                // Batch-execute all children.
                                for (int k = 0; k < lc.treeItem->childCount(); k++)
                                {
                                    QWidget* gcw = this->treeWidgetRundown->itemWidget(lc.treeItem->child(k), 0);
                                    AbstractRundownWidget* gcRundown = dynamic_cast<AbstractRundownWidget*>(gcw);
                                    dynamic_cast<AbstractPlayoutCommand*>(gcRundown)->executeCommand(Playout::PlayoutType::Play);
                                    fireAutoPlayActivity(gcRundown);
                                }

                                // Find trigger child for queue tracking.
                                AbstractRundownWidget* triggerChild = nullptr;
                                for (int k = 0; k < lc.treeItem->childCount(); k++)
                                {
                                    QWidget* gcw = this->treeWidgetRundown->itemWidget(lc.treeItem->child(k), 0);
                                    AbstractRundownWidget* gcRundown = dynamic_cast<AbstractRundownWidget*>(gcw);
                                    if (MovieCommand* mc = dynamic_cast<MovieCommand*>(gcRundown->getCommand()))
                                    {
                                        if (mc->getAutoPlay()) { triggerChild = gcRundown; break; }
                                    }
                                    else if (!triggerChild)
                                    {
                                        if (StillCommand* sc = dynamic_cast<StillCommand*>(gcRundown->getCommand()))
                                        {
                                            if (sc->getAutoPlay() && sc->getDuration() > 0)
                                                triggerChild = gcRundown;
                                        }
                                        else if (TemplateCommand* tc = dynamic_cast<TemplateCommand*>(gcRundown->getCommand()))
                                        {
                                            if (tc->getAutoPlay() && tc->getDuration() > 0)
                                                triggerChild = gcRundown;
                                        }
                                    }
                                }

                                if (triggerChild != nullptr)
                                {
                                    queueInfo.queue->push_back(triggerChild);
                                    setAutoPlayHighlight(this->currentAutoPlayWidget, false);
                                    this->currentAutoPlayWidget = triggerChild;
                                    setAutoPlayHighlight(this->currentAutoPlayWidget, true);
                                }
                            }
                            else
                            {
                                queueInfo.queue->push_back(rundownChildWidget);
                            }
                            isFirstChild = false;
                            continue;
                        }

                        // Check if it's a movie with AutoPlay, or a still with AutoPlay and Duration.
                        bool shouldQueue = false;
                        bool isGateway = (rundownChildWidget->getLibraryModel()->getType() == Rundown::AUTOPLAYGATEWAY);
                        if (dynamic_cast<MovieCommand*>(rundownChildWidget->getCommand()))
                        {
                            if (dynamic_cast<MovieCommand*>(rundownChildWidget->getCommand())->getAutoPlay())
                                shouldQueue = true;
                        }
                        else if (dynamic_cast<StillCommand*>(rundownChildWidget->getCommand()))
                        {
                            StillCommand* stillCmd = dynamic_cast<StillCommand*>(rundownChildWidget->getCommand());
                            if (stillCmd->getAutoPlay() && stillCmd->getDuration() > 0)
                                shouldQueue = true;
                        }

                        if (shouldQueue)
                        {
                            queueInfo.queue->push_back(rundownChildWidget);

                            // Play the first item in the queue.
                            if (isFirstChild)
                            {
                                bool firstIsMovie = dynamic_cast<MovieCommand*>(rundownChildWidget->getCommand()) != nullptr;
                                if (firstIsMovie)
                                    dynamic_cast<AbstractPlayoutCommand*>(rundownChildWidget)->executeCommand(Playout::PlayoutType::Next);
                                else
                                    dynamic_cast<AbstractPlayoutCommand*>(rundownChildWidget)->executeCommand(Playout::PlayoutType::Play);
                                setAutoPlayHighlight(this->currentAutoPlayWidget, false);
                                this->currentAutoPlayWidget = rundownChildWidget;
                                setAutoPlayHighlight(this->currentAutoPlayWidget, true);
                                fireAutoPlayActivity(rundownChildWidget);
                                isFirstChild = false;
                            }
                        }
                        else if (isGateway && !isFirstChild)
                        {
                            // Add gateway items to loop queue (only after first playable item).
                            queueInfo.queue->push_back(rundownChildWidget);
                        }
                    }

                    // If we added items to the queue, continue (don't remove queue).
                    if (!queueInfo.queue->isEmpty())
                        break;
                }
            }

            // No loop or no items to loop.
            setAutoPlayHighlight(this->currentAutoPlayWidget, false);
            if (queueInfo.isTopLevelChain)
            {
                chainToNextTopLevelItem(i, rundownWidget);
            }
            else
            {
                delete queueInfo.queue;
                this->autoPlayQueues.removeAt(i);
            }
        }

        break;
    }
}

void RundownTreeWidget::autoPlayNextRundownItem(const AutoPlayNextRundownItemEvent& event)
{
    if (!this->active)
        return;

    AbstractRundownWidget* rundownWidget = dynamic_cast<AbstractRundownWidget*>(event.getSource());

    for (int i = 0; i < this->autoPlayQueues.count(); i++)
    {
        AutoPlayQueueInfo& queueInfo = this->autoPlayQueues[i];
        if (queueInfo.queue->contains(rundownWidget))
        {
            // Have more in queue, play them...
            if (!queueInfo.queue->isEmpty())
            {
                AbstractRundownWidget* rundownQueueWidget = dynamic_cast<AbstractRundownWidget*>(queueInfo.queue->at(0));
                dynamic_cast<AbstractPlayoutCommand*>(rundownQueueWidget)->executeCommand(Playout::PlayoutType::Next);

                setAutoPlayHighlight(this->currentAutoPlayWidget, false);
                this->currentAutoPlayWidget = rundownQueueWidget;
                setAutoPlayHighlight(this->currentAutoPlayWidget, true);
            }

            break;
        }
    }
}

void RundownTreeWidget::setActive(bool active)
{
    this->active = active;

    if (this->active)
    {
        EventManager::getInstance().fireAllowRemoteTriggeringEvent(AllowRemoteTriggeringEvent(this->allowRemoteRundownTriggering));
        EventManager::getInstance().fireRepositoryRundownEvent(RepositoryRundownEvent(this->repositoryRundown));
        EventManager::getInstance().fireActiveRundownChangedEvent(ActiveRundownChangedEvent(this->activeRundown));
    }

    QTreeWidgetItem* currentItem = this->treeWidgetRundown->currentItem();
    QWidget* currentItemWidget = this->treeWidgetRundown->itemWidget(currentItem, 0);

    QTreeWidgetItem* currentItemParent = NULL;
    if (currentItem != NULL)
        currentItemParent = this->treeWidgetRundown->currentItem()->parent();

    QWidget* currentItemWidgetParent = NULL;
    if (currentItemParent != NULL)
        currentItemWidgetParent = this->treeWidgetRundown->itemWidget(currentItemParent, 0);

    if (currentItem != NULL && currentItemWidget != NULL)
    {
        dynamic_cast<AbstractRundownWidget*>(currentItemWidget)->setSelected(this->active);

        // Set/clear green selection highlight on the widget (left badge border + right strip).
        // Skip items that are currently playing — they keep their channel color.
        if (!this->currentPlayingItems.values().contains(currentItem))
            RundownWidgetHelper::setSelectionHighlight(currentItemWidget, this->active);

        // Manage Qt selection visual (green border from CSS ::item:selected).
        // Clear it when deactivating so only the focused pane shows the selector.
        if (!this->active)
            this->treeWidgetRundown->selectionModel()->clearSelection();
        else
            currentItem->setSelected(true);

        AbstractCommand* command = dynamic_cast<AbstractRundownWidget*>(currentItemWidget)->getCommand();
        LibraryModel* model = dynamic_cast<AbstractRundownWidget*>(currentItemWidget)->getLibraryModel();

        EventManager::getInstance().fireRundownItemSelectedEvent(RundownItemSelectedEvent(command, model, currentItemWidget, currentItemWidgetParent));
        EventManager::getInstance().fireSaveAsPresetMenuEvent(SaveAsPresetMenuEvent(true));
    }
    else // Empty rundown.
    {
        if (!this->active)
            this->treeWidgetRundown->selectionModel()->clearSelection();

        EventManager::getInstance().fireEmptyRundownEvent(EmptyRundownEvent());
        EventManager::getInstance().fireSaveAsPresetMenuEvent(SaveAsPresetMenuEvent(false));
    }
}

void RundownTreeWidget::openRundown(const QString& path)
{
    QElapsedTimer time;
    time.start();

    EventManager::getInstance().fireStatusbarEvent(StatusbarEvent("Opening rundown..."));

    qDebug("Open rundown %s", qPrintable(path));

    QFile file(path);
    if (file.open(QFile::ReadOnly | QIODevice::Text))
    {
        this->activeRundown = path;

        QTextStream stream(&file);
        stream.setEncoding(QStringConverter::Utf8);

        // Save the latest value stored in the clipboard.
        QString latest = qApp->clipboard()->text();
        QString data = stream.readAll();

        this->hexHash = QString(QCryptographicHash::hash(data.toUtf8(), QCryptographicHash::Md5).toHex());
        qDebug("Hash is %s", qPrintable(this->hexHash));

        qApp->clipboard()->setText(data);
        this->treeWidgetRundown->pasteSelectedItems(false, true); // preserveCloneLinks = true for file load.
        wireAllGatewayWidgets();

        // Set previous stored clipboard value.
        qApp->clipboard()->setText(latest);

        qDebug("Parsing rundown completed in %lld msec", time.elapsed());

        file.close();

        if (this->treeWidgetRundown->invisibleRootItem()->childCount() > 0)
            this->treeWidgetRundown->setCurrentItem(this->treeWidgetRundown->invisibleRootItem()->child(0));

        this->treeWidgetRundown->setFocus();

        if (!this->suppressOpenRecent)
            DatabaseManager::getInstance().insertOpenRecent(path);

        // The rundown is on screen now, so anything pointing at media that is
        // not there can be marked before it is played rather than after.
        checkMissingMedia();
        markSimpleModeItems();

        qDebug("RundownTreeWidget::openRundown %lld msec (%d items)", time.elapsed(), this->treeWidgetRundown->invisibleRootItem()->childCount());
    }

    EventManager::getInstance().fireStatusbarEvent(StatusbarEvent(""));
}

void RundownTreeWidget::openRundownFromUrl(const QString& url)
{
    EventManager::getInstance().fireStatusbarEvent(StatusbarEvent("Opening rundown..."));
    EventManager::getInstance().fireReloadRundownMenuEvent(ReloadRundownMenuEvent(false));

    qDebug("Open rundown from url %s", qPrintable(url));

    this->activeRundown = url;

    this->repositoryDevice = QSharedPointer<RepositoryDevice>(new RepositoryDevice(QUrl(url).host()));
    QObject::connect(this->repositoryDevice.data(), SIGNAL(connectionStateChanged(RepositoryDevice&)), this, SLOT(repositoryConnectionStateChanged(RepositoryDevice&)));
    QObject::connect(this->repositoryDevice.data(), SIGNAL(repositoryChanged(const RepositoryChangeModel&, RepositoryDevice&)), this, SLOT(repositoryChanged(const RepositoryChangeModel&, RepositoryDevice&)));
    this->repositoryDevice->connectDevice();

    this->networkManager = new QNetworkAccessManager(this);
    QObject::connect(this->networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(doOpenRundownFromUrl(QNetworkReply*)));
    this->networkManager->get(QNetworkRequest(QUrl(url)));
}

void RundownTreeWidget::doOpenRundownFromUrl(QNetworkReply* reply)
{
    this->repositoryRundown = true;

    // Save the latest value stored in the clipboard.
    QString latest = qApp->clipboard()->text();
    QString data = QString::fromUtf8(reply->readAll());

    this->hexHash = QString(QCryptographicHash::hash(data.toUtf8(), QCryptographicHash::Md5).toHex());
    qDebug("Hash is %s", qPrintable(this->hexHash));

    qApp->clipboard()->setText(data);
    this->treeWidgetRundown->pasteSelectedItems(false, true); // preserveCloneLinks = true for URL load.
    wireAllGatewayWidgets();

    // Set previous stored clipboard value.
    qApp->clipboard()->setText(latest);

    if (this->treeWidgetRundown->invisibleRootItem()->childCount() > 0)
        this->treeWidgetRundown->setCurrentItem(this->treeWidgetRundown->invisibleRootItem()->child(0));

    this->treeWidgetRundown->setFocus();

    EventManager::getInstance().fireSaveMenuEvent(SaveMenuEvent(false));
    EventManager::getInstance().fireSaveAsMenuEvent(SaveAsMenuEvent(false));
    EventManager::getInstance().fireReloadRundownMenuEvent(ReloadRundownMenuEvent(true));
    EventManager::getInstance().fireStatusbarEvent(StatusbarEvent(""));

    reply->deleteLater();
    this->networkManager->deleteLater();
}

void RundownTreeWidget::repositoryConnectionStateChanged(RepositoryDevice& device)
{
    qDebug("RundownTreeWidget::repositoryConnectionStateChanged %s (%s)", qPrintable(device.getAddress()), qPrintable((device.isConnected() == true) ? "connected" : "disconnected"));

    QStringList repositoryUrl = this->activeRundown.split("/");
    QString rundown = repositoryUrl.takeLast();
    QString profile = repositoryUrl.takeLast();

    device.subscribe(rundown, profile);
}

void RundownTreeWidget::repositoryChanged(const RepositoryChangeModel& model, RepositoryDevice& device)
{
    Q_UNUSED(device);

    this->treeWidgetRundown->addRepositoryChange(model);
    this->treeWidgetRundown->applyRepositoryChanges();
}

void RundownTreeWidget::insertRepositoryChanges(const InsertRepositoryChangesEvent& event)
{
    Q_UNUSED(event);

    if (!this->active)
        return;

    this->treeWidgetRundown->applyRepositoryChanges();
    this->treeWidgetRundown->checRepositoryChanges();
}

void RundownTreeWidget::reloadRundown()
{
    if (this->activeRundown == Rundown::DEFAULT_NAME)
        return;

    if (this->copyItem != NULL)
        this->copyItem = NULL;

    this->currentPlayingItems.clear();

    if (this->currentPlayingAutoStepItem != NULL)
        this->currentPlayingAutoStepItem = NULL;

    if (this->currentAutoPlayWidget != NULL)
    {
        setAutoPlayHighlight(this->currentAutoPlayWidget, false);
        this->currentAutoPlayWidget = NULL;
    }

    setAutostepHighlight(this->currentAutostepHighlightItem, false);
    this->currentAutostepHighlightItem = nullptr;

    this->treeWidgetRundown->removeAllItems();

    if (this->repositoryRundown)
        openRundownFromUrl(this->activeRundown);
    else
        openRundown(this->activeRundown);

    if (this->treeWidgetRundown->invisibleRootItem()->childCount() > 0)
        this->treeWidgetRundown->setCurrentItem(this->treeWidgetRundown->invisibleRootItem()->child(0));
}

void RundownTreeWidget::saveRundown(bool saveAs)
{
    if (this->treeWidgetRundown->invisibleRootItem()->childCount() == 0)
        return;

    QString path;
    if (saveAs) {
        if (this->activeRundown == Rundown::DEFAULT_NAME) {
            path = QDir::homePath();
        }
        else {
            QFileInfo fi(this->activeRundown);
            path = fi.absolutePath();
        }
        path = QFileDialog::getSaveFileName(this, "Save Rundown", path, "Rundown (*.xml)");
    }
    else
        path = (this->activeRundown == Rundown::DEFAULT_NAME) ? QFileDialog::getSaveFileName(this, "Save Rundown", QDir::homePath(), "Rundown (*.xml)") : this->activeRundown;

    if (!path.isEmpty())
    {
        // Make sure we have an extension. On *nix system it will not be appended by default.
        if (!path.toLower().endsWith(".xml"))
            path.append(".xml");
        EventManager::getInstance().fireStatusbarEvent(StatusbarEvent("Saving rundown..."));

        QFile file(path);
        if (file.exists())
            file.remove();

        if (file.open(QFile::WriteOnly))
        {
            QByteArray data = serialiseRundown();

            this->hexHash = QString(QCryptographicHash::hash(data, QCryptographicHash::Md5).toHex());
            qDebug("Hash is %s", qPrintable(this->hexHash));

            file.write(data);
            file.close();

            DatabaseManager::getInstance().insertOpenRecent(path);

            qDebug("Saved rundown to %s", qPrintable(path));
        }

        this->activeRundown = path;
        EventManager::getInstance().fireActiveRundownChangedEvent(ActiveRundownChangedEvent(this->activeRundown));
        EventManager::getInstance().fireStatusbarEvent(StatusbarEvent(""));
    }
}

QByteArray RundownTreeWidget::serialiseRundown() const
{
    QByteArray data;
    QXmlStreamWriter writer(&data);

    writer.setAutoFormatting(XmlFormatting::ENABLE_FORMATTING);
    writer.setAutoFormattingIndent(XmlFormatting::NUMBER_OF_SPACES);

    writer.writeStartDocument();
    writer.writeStartElement("items");
    writer.writeTextElement("allowremotetriggering", (this->allowRemoteRundownTriggering == true) ? "true" : "false");

    for (int i = 0; i < this->treeWidgetRundown->invisibleRootItem()->childCount(); i++)
        this->treeWidgetRundown->writeProperties(this->treeWidgetRundown->invisibleRootItem()->child(i), writer);

    writer.writeEndElement();
    writer.writeEndDocument();

    return data;
}

const QString& RundownTreeWidget::getActiveRundown() const
{
    return this->activeRundown;
}

bool RundownTreeWidget::openAutoSaveCopy(const QString& autoSavePath, const QString& originalPath)
{
    QFile source(autoSavePath);
    if (!source.open(QFile::ReadOnly))
        return false;

    QByteArray content = source.readAll();
    source.close();

    // Drop the marker line the auto-save writer put in front of the rundown; what
    // follows is ordinary rundown XML and has to reach the parser as such.
    if (content.startsWith(AutoSaveNaming::marker()))
    {
        int newline = content.indexOf('\n');
        if (newline < 0)
            return false;

        content = content.mid(newline + 1);
    }

    // openRundown() reads from a path, so the stripped copy needs one. It goes
    // beside the recovery file rather than in the user's way, and is removed
    // whether or not the load worked.
    QString scratchPath = autoSavePath + ".restoring";

    QFile scratch(scratchPath);
    if (!scratch.open(QFile::WriteOnly | QFile::Truncate))
        return false;

    scratch.write(content);
    scratch.close();

    this->suppressOpenRecent = true;
    openRundown(scratchPath);
    this->suppressOpenRecent = false;

    QFile::remove(scratchPath);

    // openRundown() left this pointing at the scratch file and recorded its hash
    // as the saved state. Both are wrong for a recovered rundown: it belongs to
    // the file it was recovered for, and it is emphatically unsaved. Clearing the
    // hash is what makes Ctrl+S and the quit prompt treat it as such.
    this->activeRundown = originalPath.isEmpty() ? Rundown::DEFAULT_NAME : originalPath;
    this->hexHash.clear();

    EventManager::getInstance().fireActiveRundownChangedEvent(ActiveRundownChangedEvent(this->activeRundown));

    return true;
}

// Which rows have a key on the Simple Mode grid. Deferred for the same reason as
// the media sweep: on load the item widgets are still being built.
void RundownTreeWidget::markSimpleModeItems()
{
    if (this->simpleModeMarker == nullptr)
        this->simpleModeMarker = new SimpleModeMarker(this);

    QTimer::singleShot(0, this, [this]() {
        this->simpleModeMarker->sweep(this->treeWidgetRundown);
    });
}

void RundownTreeWidget::checkMissingMedia()
{
    if (!MissingMediaScanner::isEnabled())
        return;

    // Deferred by a turn of the event loop: on load the item widgets are still
    // being built, and a sweep now would walk a tree that is not finished.
    QTimer::singleShot(0, this, [this]() {
        MissingMediaScanner scanner;
        MissingMediaScanner::Result result = scanner.scan(this->treeWidgetRundown);

        if (result.missing == 0)
            return;

        EventManager::getInstance().fireStatusbarEvent(
            StatusbarEvent(QString("%1 of %2 item%3 point at media that was not found - hover a warning mark to see why")
                .arg(result.missing).arg(result.checked).arg(result.checked == 1 ? "" : "s")));
    });
}

bool RundownTreeWidget::checkForSave() const
{
    // Don't save empty rundowns.
    if (this->treeWidgetRundown->invisibleRootItem()->childCount() == 0)
        return false;

    // We can't save repository rundowns.
    if (this->repositoryRundown)
        return false;

    QString hexHash = QString(QCryptographicHash::hash(serialiseRundown(), QCryptographicHash::Md5).toHex());

    if (hexHash != this->hexHash)
        return true;

    return false;
}

QString RundownTreeWidget::autoSaveStemFor(const QString& activeRundown)
{
    // Rundown::DEFAULT_NAME is this class's idea of "no file yet"; the naming
    // rules themselves live in the header so they can be tested on their own.
    if (activeRundown == Rundown::DEFAULT_NAME)
        return AutoSaveNaming::stemFor(QString());

    return AutoSaveNaming::stemFor(activeRundown);
}

bool RundownTreeWidget::writeAutoSaveCopy(const QString& directory) const
{
    // Only rundowns with something in them and something changed. checkForSave()
    // already rules out empty and repository rundowns, so a copy is only ever made
    // of work that would otherwise be lost.
    if (!checkForSave())
        return false;

    QDir().mkpath(directory);

    QString safeStem = autoSaveStemFor(this->activeRundown);

    // The original path travels inside the file rather than in its name, so a
    // restore knows where the rundown belongs without encoding a path in a
    // filename. The rundown itself is written unchanged after this line.
    QByteArray payload;
    payload.append(AutoSaveNaming::marker());
    payload.append(this->activeRundown.toUtf8().toPercentEncoding());
    payload.append(" -->\n");
    payload.append(serialiseRundown());

    // Written to a temporary name and renamed into place, so a recovery file is
    // never a half-written one: the crash this protects against can land here.
    QString finalPath = QString("%1/%2.xml").arg(directory, safeStem);
    QString partPath = finalPath + ".part";

    QFile part(partPath);
    if (!part.open(QFile::WriteOnly | QFile::Truncate))
        return false;

    if (part.write(payload) != payload.size())
    {
        part.close();
        part.remove();
        return false;
    }

    part.close();

    QFile::remove(finalPath);
    if (!QFile::rename(partPath, finalPath))
    {
        QFile::remove(partPath);
        return false;
    }

    return true;
}

void RundownTreeWidget::colorizeItems(const QString& color)
{
    if (this->treeWidgetRundown->selectedItems().count() == 0)
        return;

    foreach (QTreeWidgetItem* item, this->treeWidgetRundown->selectedItems())
        dynamic_cast<AbstractRundownWidget*>(this->treeWidgetRundown->itemWidget(item, 0))->setColor(color); // Colorize current selected item.
}

void RundownTreeWidget::gpiPortTriggered(int gpiPort, GpiDevice* device)
{
    Q_UNUSED(device);

    executeCommand(gpiBindings[gpiPort], Action::ActionType::GpiPulse, nullptr, true);
}

void RundownTreeWidget::gpiBindingChanged(int gpiPort, Playout::PlayoutType binding)
{
    gpiBindings[gpiPort] = binding;
}

namespace
{
    // Apply a functor to autoLoop-capable commands of every selected item.
    template <typename F>
    void forEachSelectedAutoLoopCommand(QTreeWidget* tree, F fn)
    {
        for (QTreeWidgetItem* item : tree->selectedItems())
        {
            QWidget* widget = tree->itemWidget(item, 0);
            AbstractRundownWidget* rw = dynamic_cast<AbstractRundownWidget*>(widget);
            if (rw == nullptr) continue;

            AbstractCommand* cmd = rw->getCommand();
            if (auto* mc = dynamic_cast<MovieCommand*>(cmd))          fn(mc);
            else if (auto* sc = dynamic_cast<StillCommand*>(cmd))     fn(sc);
            else if (auto* tc = dynamic_cast<TemplateCommand*>(cmd))  fn(tc);
            else if (auto* gc = dynamic_cast<GroupCommand*>(cmd))     fn(gc);
        }
    }
}

void RundownTreeWidget::customContextMenuRequested(const QPoint& point)
{
    foreach (QAction* action, this->contextMenuRundown->actions())
        action->setEnabled(true);

    bool isGroup = false;
    bool isTopItem = false;
    bool isGroupItem = false;
    bool hasDepth2 = false;
    bool hasGroupAtDepth1 = false;
    bool mixedDepth = false;
    int firstDepth = -1;

    foreach (QTreeWidgetItem* item, this->treeWidgetRundown->selectedItems())
    {
        QWidget* widget = this->treeWidgetRundown->itemWidget(item, 0);
        AbstractRundownWidget* rw = dynamic_cast<AbstractRundownWidget*>(widget);
        int depth = RundownTreeBaseWidget::getItemDepth(item);

        if (firstDepth < 0)
            firstDepth = depth;
        else if (depth != firstDepth)
            mixedDepth = true;

        if (depth >= 2)
            hasDepth2 = true;

        if (rw->isGroup() && depth >= 1)
            hasGroupAtDepth1 = true;

        if (item->parent() != NULL)
            isGroupItem = true;
        else if (rw->isGroup())
            isGroup = true;
        else
            isTopItem = true;
    }

    // Group action: disable when depth >= 2, mixed depths, or groups at depth 1.
    bool canGroup = !hasDepth2 && !mixedDepth && !hasGroupAtDepth1
                    && (isTopItem || isGroup || isGroupItem);
    if (!canGroup)
        this->contextMenuRundown->actions().at(2)->setEnabled(false);

    // Ungroup action: disable when no group to ungroup.
    // Allow when a group is selected (any depth) or when items inside a group are selected.
    bool canUngroup = false;
    if (isGroup && !isTopItem && !isGroupItem) canUngroup = true; // Only groups selected.
    if (isGroupItem && !isTopItem && !isGroup) canUngroup = true; // Only group items selected.
    // Also allow ungrouping inner groups (isGroupItem is true for them since they have a parent).
    if (!canUngroup)
        this->contextMenuRundown->actions().at(3)->setEnabled(false);

    if (!isTopItem && !isGroup && !isGroupItem)
    {
        this->contextMenuRundown->actions().at(2)->setEnabled(false); // Group.
        this->contextMenuRundown->actions().at(3)->setEnabled(false); // Ungroup.
        this->contextMenuRundown->actions().at(5)->setEnabled(false); // Mark Item.
        this->contextMenuRundown->actions().at(7)->setEnabled(false); // Copy Properties.
        this->contextMenuRundown->actions().at(8)->setEnabled(false); // Paste Properties.
        this->contextMenuRundown->actions().at(9)->setEnabled(false); // Paste Properties (No Data).
        this->contextMenuRundown->actions().at(11)->setEnabled(false); // Create Linked Clone.
        this->contextMenuRundown->actions().at(12)->setEnabled(false); // Unlink Clone.
        this->contextMenuRundown->actions().at(16)->setEnabled(false); // Colorize Item.
        this->contextMenuRundown->actions().at(18)->setEnabled(false); // Save as Preset.
        this->contextMenuRundown->actions().at(20)->setEnabled(false); // Remove.
    }

    if (this->repositoryRundown)
    {
        this->contextMenuRundown->actions().at(0)->setEnabled(false); // Tools.
        this->contextMenuRundown->actions().at(2)->setEnabled(false); // Group.
        this->contextMenuRundown->actions().at(3)->setEnabled(false); // Ungroup.
        this->contextMenuRundown->actions().at(8)->setEnabled(false); // Paste Properties.
        this->contextMenuRundown->actions().at(9)->setEnabled(false); // Paste Properties (No Data).
        this->contextMenuRundown->actions().at(11)->setEnabled(false); // Create Linked Clone.
        this->contextMenuRundown->actions().at(12)->setEnabled(false); // Unlink Clone.
        this->contextMenuRundown->actions().at(16)->setEnabled(false); // Colorize Item.
        this->contextMenuRundown->actions().at(18)->setEnabled(false); // Save as Preset.
        this->contextMenuRundown->actions().at(20)->setEnabled(false); // Remove.
    }

    // Linked clone enable/disable logic.
    if (this->treeWidgetRundown->selectedItems().count() == 1)
    {
        QTreeWidgetItem* item = this->treeWidgetRundown->currentItem();
        QWidget* w = this->treeWidgetRundown->itemWidget(item, 0);
        AbstractRundownWidget* rw = dynamic_cast<AbstractRundownWidget*>(w);
        if (rw != NULL)
        {
            if (rw->isGroup())
                this->contextMenuRundown->actions().at(11)->setEnabled(false); // No cloning groups.

            if (rw->getCommand()->getCloneGroupId().isEmpty())
                this->contextMenuRundown->actions().at(12)->setEnabled(false); // Nothing to unlink.
        }
    }
    else
    {
        // Multiple selection: disable clone actions.
        this->contextMenuRundown->actions().at(11)->setEnabled(false);
        this->contextMenuRundown->actions().at(12)->setEnabled(false);
    }

    if (this->treeWidgetRundown->selectedItems().count() > 0)
    {
        if (this->treeWidgetRundown->selectedItems().count() == 1)
        {
            QTreeWidgetItem* currentItem = this->treeWidgetRundown->currentItem();
            QWidget* currentItemWidget = this->treeWidgetRundown->itemWidget(currentItem, 0);
            if (dynamic_cast<AbstractRundownWidget*>(currentItemWidget) != NULL)
            {
                QString color = dynamic_cast<AbstractRundownWidget*>(currentItemWidget)->getColor();
                QString name = colorLookup(color, true);

                foreach (QAction* action, this->contextMenuColor->actions())
                {
                    if (action->text() == name)
                        action->setChecked(true);
                    else
                        action->setChecked(false);
                }
            }
        }
        else
        {
            foreach (QAction* action, this->contextMenuColor->actions())
                action->setChecked(false);
        }
    }

    // Gateway-specific context menu logic.
    bool showAddGatewayExit = false;
    bool isGatewayItem = false;
    if (this->treeWidgetRundown->selectedItems().count() == 1)
    {
        QTreeWidgetItem* selItem = this->treeWidgetRundown->currentItem();
        QWidget* selWidget = this->treeWidgetRundown->itemWidget(selItem, 0);
        AbstractRundownWidget* selRw = dynamic_cast<AbstractRundownWidget*>(selWidget);
        if (selRw != nullptr)
        {
            QString selType = selRw->getLibraryModel()->getType();
            isGatewayItem = (selType == Rundown::AUTOPLAYGATEWAY || selType == Rundown::FOCUSGATEWAY || selType == Rundown::COMMANDGATEWAY);
            if (isGatewayItem)
                showAddGatewayExit = true;
        }
    }
    this->addGatewayExitAction->setVisible(showAddGatewayExit);

    // Hide clone actions for gateway items.
    for (QAction* action : this->contextMenuRundown->actions())
    {
        if (action->text() == "Create Linked Clone" || action->text() == "Unlink Clone")
            action->setVisible(!isGatewayItem);
    }

    // Auto-Loop submenu: visible only if at least one Movie/Still/Template item is selected.
    bool hasAutoLoopTarget = false;
    bool firstAutoLoopEnabled = false;
    int firstAutoLoopDelay = 5;
    bool tookFirst = false;
    forEachSelectedAutoLoopCommand(this->treeWidgetRundown, [&](auto* cmd) {
        hasAutoLoopTarget = true;
        if (!tookFirst)
        {
            firstAutoLoopEnabled = cmd->getAutoLoop();
            firstAutoLoopDelay = cmd->getAutoLoopDelay();
            tookFirst = true;
        }
    });
    this->contextMenuAutoLoop->menuAction()->setVisible(hasAutoLoopTarget);
    if (hasAutoLoopTarget)
    {
        this->actionAutoLoopEnable->setChecked(firstAutoLoopEnabled);
        this->actionAutoLoopDelay5->setChecked(firstAutoLoopDelay == 5);
        this->actionAutoLoopDelay10->setChecked(firstAutoLoopDelay == 10);
        this->actionAutoLoopDelay30->setChecked(firstAutoLoopDelay == 30);
        this->actionAutoLoopDelay60->setChecked(firstAutoLoopDelay == 60);
    }

    this->contextMenuRundown->exec(this->treeWidgetRundown->mapToGlobal(point));
}

void RundownTreeWidget::contextMenuNewTriggered(QAction* action)
{
    Q_UNUSED(action);
}

QString RundownTreeWidget::colorLookup(const QString& color, bool reverse) const
{
    if (reverse)
    {
        // Reds
        if (color == Color::BRIGHT_RED_COLOR) return "Bright Red";
        else if (color == Color::DARK_RED_COLOR) return "Dark Red";
        else if (color == Color::SOFT_RED_COLOR) return "Soft Red";
        else if (color == Color::ROSE_COLOR) return "Rose";
        // Oranges
        else if (color == Color::ORANGE_COLOR) return "Orange";
        else if (color == Color::BURNT_ORANGE_COLOR) return "Burnt Orange";
        else if (color == Color::PEACH_COLOR) return "Peach";
        // Yellows
        else if (color == Color::GOLD_COLOR) return "Gold";
        else if (color == Color::YELLOW_COLOR) return "Yellow";
        else if (color == Color::AMBER_COLOR) return "Amber";
        // Greens
        else if (color == Color::GREEN_COLOR) return "Green";
        else if (color == Color::DARK_GREEN_COLOR) return "Dark Green";
        else if (color == Color::LIME_COLOR) return "Lime";
        else if (color == Color::OLIVE_COLOR) return "Olive";
        else if (color == Color::TEAL_COLOR) return "Teal";
        else if (color == Color::MINT_COLOR) return "Mint";
        // Blues
        else if (color == Color::BLUE_COLOR) return "Blue";
        else if (color == Color::DARK_BLUE_COLOR) return "Dark Blue";
        else if (color == Color::LIGHT_BLUE_COLOR) return "Light Blue";
        else if (color == Color::SKY_BLUE_COLOR) return "Sky Blue";
        else if (color == Color::NAVY_COLOR) return "Navy";
        else if (color == Color::CYAN_COLOR) return "Cyan";
        // Purples
        else if (color == Color::PURPLE_COLOR) return "Purple";
        else if (color == Color::DARK_PURPLE_COLOR) return "Dark Purple";
        else if (color == Color::LAVENDER_COLOR) return "Lavender";
        else if (color == Color::MAGENTA_COLOR) return "Magenta";
        // Neutrals & Browns
        else if (color == Color::BROWN_COLOR) return "Brown";
        else if (color == Color::WARM_GRAY_COLOR) return "Warm Gray";
        else if (color == Color::COOL_GRAY_COLOR) return "Cool Gray";
        else if (color == Color::CHARCOAL_COLOR) return "Charcoal";
    }
    else
    {
        // Reds
        if (color == "Bright Red") return Color::BRIGHT_RED_COLOR;
        else if (color == "Dark Red") return Color::DARK_RED_COLOR;
        else if (color == "Soft Red") return Color::SOFT_RED_COLOR;
        else if (color == "Rose") return Color::ROSE_COLOR;
        // Oranges
        else if (color == "Orange") return Color::ORANGE_COLOR;
        else if (color == "Burnt Orange") return Color::BURNT_ORANGE_COLOR;
        else if (color == "Peach") return Color::PEACH_COLOR;
        // Yellows
        else if (color == "Gold") return Color::GOLD_COLOR;
        else if (color == "Yellow") return Color::YELLOW_COLOR;
        else if (color == "Amber") return Color::AMBER_COLOR;
        // Greens
        else if (color == "Green") return Color::GREEN_COLOR;
        else if (color == "Dark Green") return Color::DARK_GREEN_COLOR;
        else if (color == "Lime") return Color::LIME_COLOR;
        else if (color == "Olive") return Color::OLIVE_COLOR;
        else if (color == "Teal") return Color::TEAL_COLOR;
        else if (color == "Mint") return Color::MINT_COLOR;
        // Blues
        else if (color == "Blue") return Color::BLUE_COLOR;
        else if (color == "Dark Blue") return Color::DARK_BLUE_COLOR;
        else if (color == "Light Blue") return Color::LIGHT_BLUE_COLOR;
        else if (color == "Sky Blue") return Color::SKY_BLUE_COLOR;
        else if (color == "Navy") return Color::NAVY_COLOR;
        else if (color == "Cyan") return Color::CYAN_COLOR;
        // Purples
        else if (color == "Purple") return Color::PURPLE_COLOR;
        else if (color == "Dark Purple") return Color::DARK_PURPLE_COLOR;
        else if (color == "Lavender") return Color::LAVENDER_COLOR;
        else if (color == "Magenta") return Color::MAGENTA_COLOR;
        // Neutrals & Browns
        else if (color == "Brown") return Color::BROWN_COLOR;
        else if (color == "Warm Gray") return Color::WARM_GRAY_COLOR;
        else if (color == "Cool Gray") return Color::COOL_GRAY_COLOR;
        else if (color == "Charcoal") return Color::CHARCOAL_COLOR;
    }

    return ""; // Reset
}

void RundownTreeWidget::contextMenuColorTriggered(QAction* action)
{
    colorizeItems(colorLookup(action->text(), false));
}

void RundownTreeWidget::contextMenuRundownTriggered(QAction* action)
{
    if (action->text() == "Group")
        this->treeWidgetRundown->groupItems();
    else if (action->text() == "Ungroup")
        this->treeWidgetRundown->ungroupItems();
}

void RundownTreeWidget::autoLoopEnableTriggered()
{
    bool enable = this->actionAutoLoopEnable->isChecked();
    forEachSelectedAutoLoopCommand(this->treeWidgetRundown, [enable](auto* cmd) {
        cmd->setAutoLoop(enable);
    });
}

void RundownTreeWidget::autoLoopDelayPresetTriggered(int seconds)
{
    forEachSelectedAutoLoopCommand(this->treeWidgetRundown, [seconds](auto* cmd) {
        cmd->setAutoLoopDelay(seconds);
    });
}

void RundownTreeWidget::autoLoopDelayCustomTriggered()
{
    // Seed with the delay of the first selected supported item.
    int seed = 5;
    bool taken = false;
    forEachSelectedAutoLoopCommand(this->treeWidgetRundown, [&seed, &taken](auto* cmd) {
        if (!taken) { seed = cmd->getAutoLoopDelay(); taken = true; }
    });

    bool ok = false;
    int val = QInputDialog::getInt(this, tr("Auto-Loop Delay"),
                                   tr("Seconds between fires:"), seed, 1, 3600, 1, &ok);
    if (!ok) return;

    forEachSelectedAutoLoopCommand(this->treeWidgetRundown, [val](auto* cmd) {
        cmd->setAutoLoopDelay(val);
    });
}

void RundownTreeWidget::createLinkedClone()
{
    if (this->treeWidgetRundown->selectedItems().count() != 1)
        return;

    QTreeWidgetItem* sourceItem = this->treeWidgetRundown->currentItem();
    if (sourceItem == NULL)
        return;

    QWidget* sourceWidget = this->treeWidgetRundown->itemWidget(sourceItem, 0);
    AbstractRundownWidget* sourceRundownWidget = dynamic_cast<AbstractRundownWidget*>(sourceWidget);
    if (sourceRundownWidget == NULL || sourceRundownWidget->isGroup())
        return;

    // Block clone creation for gateway objects.
    QString itemType = sourceRundownWidget->getLibraryModel()->getType();
    if (itemType == Rundown::AUTOPLAYGATEWAY || itemType == Rundown::FOCUSGATEWAY || itemType == Rundown::COMMANDGATEWAY)
        return;

    AbstractCommand* sourceCommand = sourceRundownWidget->getCommand();

    // Assign a clone group ID to the source if it doesn't have one yet.
    if (sourceCommand->getCloneGroupId().isEmpty())
        sourceCommand->setCloneGroupId(QUuid::createUuid().toString(QUuid::WithoutBraces));

    // Clone the item. cloneItem() adds the properties every command shares.
    AbstractRundownWidget* cloneWidget = sourceRundownWidget->cloneItem();
    cloneWidget->getCommand()->setCloneGroupId(sourceCommand->getCloneGroupId());

    // Insert the clone after the source item in the tree.
    QTreeWidgetItem* cloneItem = new QTreeWidgetItem();
    if (sourceItem->parent() != NULL)
    {
        // Source is inside a group — insert in same group.
        int index = sourceItem->parent()->indexOfChild(sourceItem);
        sourceItem->parent()->insertChild(index + 1, cloneItem);
    }
    else
    {
        // Source is top-level.
        int index = this->treeWidgetRundown->indexOfTopLevelItem(sourceItem);
        this->treeWidgetRundown->insertTopLevelItem(index + 1, cloneItem);
    }

    cloneItem->setFlags(cloneItem->flags() | Qt::ItemIsEditable);
    cloneItem->setSizeHint(0, QSize(cloneItem->sizeHint(0).width(), Rundown::DEFAULT_ITEM_HEIGHT));
    this->treeWidgetRundown->setItemWidget(cloneItem, 0, dynamic_cast<QWidget*>(cloneWidget));

    this->treeWidgetRundown->setCurrentItem(cloneItem);
    this->treeWidgetRundown->checkEmptyRundown();
}

void RundownTreeWidget::unlinkClone()
{
    if (this->treeWidgetRundown->selectedItems().count() != 1)
        return;

    QTreeWidgetItem* item = this->treeWidgetRundown->currentItem();
    if (item == NULL)
        return;

    QWidget* widget = this->treeWidgetRundown->itemWidget(item, 0);
    AbstractRundownWidget* rundownWidget = dynamic_cast<AbstractRundownWidget*>(widget);
    if (rundownWidget == NULL)
        return;

    AbstractCommand* command = rundownWidget->getCommand();
    if (command->getCloneGroupId().isEmpty())
        return;

    // Clearing the clone group ID unregisters from the registry
    // and auto-dissolves the group if only one member remains.
    command->setCloneGroupId("");
}

// Helper: get TransformData pointer from a content command, or nullptr.
static TransformData* getTransformFromCommand(AbstractCommand* cmd)
{
    if (auto* c = dynamic_cast<TemplateCommand*>(cmd)) return &c->getTransform();
    if (auto* c = dynamic_cast<MovieCommand*>(cmd)) return &c->getTransform();
    if (auto* c = dynamic_cast<StillCommand*>(cmd)) return &c->getTransform();
    if (auto* c = dynamic_cast<AudioCommand*>(cmd)) return &c->getTransform();
    if (auto* c = dynamic_cast<HtmlCommand*>(cmd)) return &c->getTransform();
    if (auto* c = dynamic_cast<ImageScrollerCommand*>(cmd)) return &c->getTransform();
    return nullptr;
}

void RundownTreeWidget::absorbTransforms()
{
    if (!this->active)
        return;

    QTreeWidgetItem* currentItem = this->treeWidgetRundown->currentItem();
    if (currentItem == nullptr)
        return;

    QWidget* widget = this->treeWidgetRundown->itemWidget(currentItem, 0);
    AbstractRundownWidget* rundownWidget = dynamic_cast<AbstractRundownWidget*>(widget);
    if (rundownWidget == nullptr)
        return;

    TransformData* td = getTransformFromCommand(rundownWidget->getCommand());
    if (td == nullptr)
        return; // Not a content item.

    // Find parent group — absorb only works on items inside a group.
    QTreeWidgetItem* parentItem = currentItem->parent();
    if (parentItem == nullptr)
        return; // Top-level item — no siblings to absorb from.

    int absorbed = 0;
    QList<QTreeWidgetItem*> toRemove;

    for (int i = 0; i < parentItem->childCount(); i++)
    {
        QTreeWidgetItem* sibling = parentItem->child(i);
        if (sibling == currentItem)
            continue;

        QWidget* sibWidget = this->treeWidgetRundown->itemWidget(sibling, 0);
        AbstractRundownWidget* sibRundown = dynamic_cast<AbstractRundownWidget*>(sibWidget);
        if (sibRundown == nullptr)
            continue;

        AbstractCommand* sibCmd = sibRundown->getCommand();

        // Check same channel+videolayer.
        if (sibCmd->getChannel() != rundownWidget->getCommand()->getChannel() ||
            sibCmd->getVideolayer() != rundownWidget->getCommand()->getVideolayer())
            continue;

        // Absorb each transform type.
        if (auto* fc = dynamic_cast<FillCommand*>(sibCmd))
        {
            td->fill = TransformData::Fill{fc->getPositionX(), fc->getPositionY(),
                                            fc->getScaleX(), fc->getScaleY(), fc->getUseMipmap()};
            toRemove.append(sibling);
            absorbed++;
        }
        else if (auto* oc = dynamic_cast<OpacityCommand*>(sibCmd))
        {
            td->opacity = oc->getOpacity();
            toRemove.append(sibling);
            absorbed++;
        }
        else if (auto* rc = dynamic_cast<RotationCommand*>(sibCmd))
        {
            td->rotation = rc->getRotation();
            toRemove.append(sibling);
            absorbed++;
        }
        else if (auto* cc = dynamic_cast<ClipCommand*>(sibCmd))
        {
            td->clip = TransformData::Clip{cc->getLeft(), cc->getTop(), cc->getWidth(), cc->getHeight()};
            toRemove.append(sibling);
            absorbed++;
        }
        else if (auto* cr = dynamic_cast<CropCommand*>(sibCmd))
        {
            td->crop = TransformData::Crop{cr->getLeft(), cr->getTop(), cr->getRight(), cr->getBottom()};
            toRemove.append(sibling);
            absorbed++;
        }
        else if (auto* ac = dynamic_cast<AnchorCommand*>(sibCmd))
        {
            td->anchor = TransformData::Anchor{ac->getPositionX(), ac->getPositionY()};
            toRemove.append(sibling);
            absorbed++;
        }
        else if (auto* bc = dynamic_cast<BrightnessCommand*>(sibCmd))
        {
            td->brightness = bc->getBrightness();
            toRemove.append(sibling);
            absorbed++;
        }
        else if (auto* cc2 = dynamic_cast<ContrastCommand*>(sibCmd))
        {
            td->contrast = cc2->getContrast();
            toRemove.append(sibling);
            absorbed++;
        }
        else if (auto* sc = dynamic_cast<SaturationCommand*>(sibCmd))
        {
            td->saturation = sc->getSaturation();
            toRemove.append(sibling);
            absorbed++;
        }
        else if (auto* vc = dynamic_cast<VolumeCommand*>(sibCmd))
        {
            td->volume = vc->getVolume();
            toRemove.append(sibling);
            absorbed++;
        }
    }

    // Remove absorbed items.
    for (int i = toRemove.count() - 1; i >= 0; i--)
    {
        QTreeWidgetItem* item = toRemove[i];
        QWidget* w = this->treeWidgetRundown->itemWidget(item, 0);
        delete w;
        delete item;
    }

    if (absorbed > 0)
    {
        this->treeWidgetRundown->updateAllGroupWidgets();
        EventManager::getInstance().fireStatusbarEvent(
            StatusbarEvent(QString("Absorbed %1 transform(s) into item").arg(absorbed)));
    }
}

void RundownTreeWidget::extractTransforms()
{
    if (!this->active)
        return;

    QTreeWidgetItem* currentItem = this->treeWidgetRundown->currentItem();
    if (currentItem == nullptr)
        return;

    QWidget* widget = this->treeWidgetRundown->itemWidget(currentItem, 0);
    AbstractRundownWidget* rundownWidget = dynamic_cast<AbstractRundownWidget*>(widget);
    if (rundownWidget == nullptr)
        return;

    TransformData* td = getTransformFromCommand(rundownWidget->getCommand());
    if (td == nullptr || !td->hasAnyTransform())
        return;

    // The item must be in a group to extract siblings into.
    QTreeWidgetItem* parentItem = currentItem->parent();
    if (parentItem == nullptr)
        return;

    int insertRow = parentItem->indexOfChild(currentItem);
    int channel = rundownWidget->getCommand()->getChannel();
    int videolayer = rundownWidget->getCommand()->getVideolayer();
    QString deviceName = rundownWidget->getLibraryModel()->getDeviceName();
    int extracted = 0;

    auto insertItem = [&](const QString& type) -> AbstractRundownWidget* {
        AbstractRundownWidget* newWidget = RundownItemFactory::getInstance().createWidget(
            LibraryModel(0, type, type, deviceName, type, 0, ""));
        newWidget->setInGroup(true);
        newWidget->setCompactView(this->treeWidgetRundown->getCompactView());
        newWidget->getCommand()->setChannel(channel);
        newWidget->getCommand()->setVideolayer(videolayer);

        QTreeWidgetItem* newItem = new QTreeWidgetItem();
        parentItem->insertChild(insertRow, newItem);
        QWidget* qw = dynamic_cast<QWidget*>(newWidget);
        int h = this->treeWidgetRundown->getCompactView() ? Rundown::COMPACT_ITEM_HEIGHT : Rundown::DEFAULT_ITEM_HEIGHT;
        qw->setFixedHeight(h);
        newItem->setSizeHint(0, QSize(qw->width(), h));
        this->treeWidgetRundown->setItemWidget(newItem, 0, qw);
        extracted++;
        return newWidget;
    };

    if (td->fill.has_value())
    {
        auto* w = insertItem(Rundown::FILL);
        FillCommand* fc = dynamic_cast<FillCommand*>(w->getCommand());
        fc->setPositionX(td->fill->x);
        fc->setPositionY(td->fill->y);
        fc->setScaleX(td->fill->scaleX);
        fc->setScaleY(td->fill->scaleY);
        fc->setDefer(true);
    }

    if (td->opacity.has_value())
    {
        auto* w = insertItem(Rundown::OPACITY);
        OpacityCommand* oc = dynamic_cast<OpacityCommand*>(w->getCommand());
        oc->setOpacity(*td->opacity);
        oc->setDefer(true);
    }

    if (td->rotation.has_value())
    {
        auto* w = insertItem(Rundown::ROTATION);
        RotationCommand* rc = dynamic_cast<RotationCommand*>(w->getCommand());
        rc->setRotation(*td->rotation);
        rc->setDefer(true);
    }

    if (td->brightness.has_value())
    {
        auto* w = insertItem(Rundown::BRIGHTNESS);
        BrightnessCommand* bc = dynamic_cast<BrightnessCommand*>(w->getCommand());
        bc->setBrightness(*td->brightness);
        bc->setDefer(true);
    }

    if (td->contrast.has_value())
    {
        auto* w = insertItem(Rundown::CONTRAST);
        ContrastCommand* cc = dynamic_cast<ContrastCommand*>(w->getCommand());
        cc->setContrast(*td->contrast);
        cc->setDefer(true);
    }

    if (td->saturation.has_value())
    {
        auto* w = insertItem(Rundown::SATURATION);
        SaturationCommand* sc = dynamic_cast<SaturationCommand*>(w->getCommand());
        sc->setSaturation(*td->saturation);
        sc->setDefer(true);
    }

    if (td->volume.has_value())
    {
        auto* w = insertItem(Rundown::VOLUME);
        VolumeCommand* vc = dynamic_cast<VolumeCommand*>(w->getCommand());
        vc->setVolume(*td->volume);
        vc->setDefer(true);
    }

    // Add a Commit item after the extracted transforms.
    if (extracted > 0)
    {
        insertRow = parentItem->indexOfChild(currentItem); // re-read since items were inserted before
        auto* commitWidget = insertItem(Rundown::COMMIT);
        Q_UNUSED(commitWidget);
    }

    // Clear the embedded transforms.
    td->clear();

    this->treeWidgetRundown->updateAllGroupWidgets();
    this->treeWidgetRundown->doItemsLayout();

    EventManager::getInstance().fireStatusbarEvent(
        StatusbarEvent(QString("Extracted %1 transform(s) from item").arg(extracted)));
}

void RundownTreeWidget::itemSelectionChanged()
{
    QList<QTreeWidgetItem*> selected = this->treeWidgetRundown->selectedItems();

    // Build a set of all currently-live items in the tree.  Undo/redo destroys
    // and rebuilds items, leaving previousSelectedItems holding dangling
    // pointers.  Calling indexFromItem() on those would crash inside Qt
    // (QTreeWidgetItem::icon access violation).
    QSet<QTreeWidgetItem*> liveItems;
    std::function<void(QTreeWidgetItem*)> collectLive = [&](QTreeWidgetItem* parent) {
        for (int i = 0; i < parent->childCount(); i++)
        {
            QTreeWidgetItem* item = parent->child(i);
            liveItems.insert(item);
            if (item->childCount() > 0)
                collectLive(item);
        }
    };
    collectLive(this->treeWidgetRundown->invisibleRootItem());

    // Clear selected flag on items no longer in the selection, but only if
    // they still exist in the tree.
    for (QTreeWidgetItem* prev : this->previousSelectedItems)
    {
        if (!liveItems.contains(prev))
            continue; // Dangling pointer; skip silently.

        if (!selected.contains(prev))
        {
            QWidget* w = this->treeWidgetRundown->itemWidget(prev, 0);
            if (w != NULL)
            {
                dynamic_cast<AbstractRundownWidget*>(w)->setSelected(false);

                // If this item was showing preview channel due to held modifier, revert to base.
                AbstractRundownWidget* rw = dynamic_cast<AbstractRundownWidget*>(w);
                if (rw != nullptr && rw->getCommand() != nullptr)
                {
                    QLabel* labelColor = w->findChild<QLabel*>("labelColor");
                    if (labelColor != nullptr)
                        RundownWidgetHelper::updateChannelBadge(labelColor,
                            rw->getCommand()->getBaseChannel(),
                            rw->getCommand()->getVideolayer());
                }
            }
        }
    }

    // Set selected flag on all currently selected items.
    for (QTreeWidgetItem* item : selected)
    {
        QWidget* w = this->treeWidgetRundown->itemWidget(item, 0);
        if (w != NULL)
            dynamic_cast<AbstractRundownWidget*>(w)->setSelected(true);
    }

    // If the preview modifier is currently held, refresh the new selection's
    // channel badge to the preview channel.
    if (shouldPreviewRedirect())
        updatePreviewChannelBadgeForSelection(true);

    this->previousSelectedItems = selected;

    if (this->treeWidgetRundown->currentItem() == NULL || selected.count() == 0)
    {
        EventManager::getInstance().fireSaveAsPresetMenuEvent(SaveAsPresetMenuEvent(false));
        return;
    }

    EventManager::getInstance().fireSaveAsPresetMenuEvent(SaveAsPresetMenuEvent(true));

    // Refresh inspector's allCommands so channel/layer changes apply to all selected items.
    // This covers keyboard multi-select (Ctrl+A, Ctrl+Space) where currentItemChanged may
    // have fired with an incomplete selection.
    if (!this->active)
        return;

    QTreeWidgetItem* currentItem = this->treeWidgetRundown->currentItem();
    if (currentItem == nullptr)
        return;

    QWidget* currentItemWidget = this->treeWidgetRundown->itemWidget(currentItem, 0);
    if (currentItemWidget == nullptr)
        return;

    AbstractCommand* command = dynamic_cast<AbstractRundownWidget*>(currentItemWidget)->getCommand();
    LibraryModel* model = dynamic_cast<AbstractRundownWidget*>(currentItemWidget)->getLibraryModel();

    QTreeWidgetItem* currentItemParent = currentItem->parent();
    QWidget* currentItemWidgetParent = nullptr;
    if (currentItemParent != nullptr)
        currentItemWidgetParent = this->treeWidgetRundown->itemWidget(currentItemParent, 0);

    QList<AbstractCommand*> allCommands;
    for (QTreeWidgetItem* sel : selected)
    {
        QWidget* w = this->treeWidgetRundown->itemWidget(sel, 0);
        AbstractRundownWidget* rw = dynamic_cast<AbstractRundownWidget*>(w);
        if (rw != nullptr && rw->getCommand() != nullptr)
            allCommands.append(rw->getCommand());
    }

    EventManager::getInstance().fireRundownItemSelectedEvent(
        RundownItemSelectedEvent(command, model, currentItemWidget, currentItemWidgetParent, allCommands));
}

void RundownTreeWidget::itemClicked(QTreeWidgetItem* current, int index)
{
    Q_UNUSED(current);
    Q_UNUSED(index);

    if (this->treeWidgetRundown->currentItem() == NULL)
        return;

    QTreeWidgetItem* currentItem = this->treeWidgetRundown->currentItem();
    QWidget* currentItemWidget = this->treeWidgetRundown->itemWidget(currentItem, 0);

    QTreeWidgetItem* currentItemParent = NULL;
    if (currentItem != NULL)
        currentItemParent = this->treeWidgetRundown->currentItem()->parent();

    QWidget* currentItemWidgetParent = NULL;
    if (currentItemParent != NULL)
        currentItemWidgetParent = this->treeWidgetRundown->itemWidget(currentItemParent, 0);

    if (currentItem != NULL && currentItemWidget != NULL)
    {
        AbstractCommand* command = dynamic_cast<AbstractRundownWidget*>(currentItemWidget)->getCommand();
        LibraryModel* model = dynamic_cast<AbstractRundownWidget*>(currentItemWidget)->getLibraryModel();

        // Gather commands from all selected items for multi-edit support.
        QList<AbstractCommand*> allCommands;
        for (QTreeWidgetItem* sel : this->treeWidgetRundown->selectedItems())
        {
            QWidget* w = this->treeWidgetRundown->itemWidget(sel, 0);
            AbstractRundownWidget* rw = dynamic_cast<AbstractRundownWidget*>(w);
            if (rw != NULL && rw->getCommand() != NULL)
                allCommands.append(rw->getCommand());
        }

        EventManager::getInstance().fireRundownItemSelectedEvent(RundownItemSelectedEvent(command, model, currentItemWidget, currentItemWidgetParent, allCommands));
        EventManager::getInstance().fireSaveAsPresetMenuEvent(SaveAsPresetMenuEvent(true));
    }
}

void RundownTreeWidget::currentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous)
{
    QWidget* currentWidget = (current != NULL) ? this->treeWidgetRundown->itemWidget(current, 0) : NULL;

    // Validate 'previous' — it may be dangling after undo/redo tree restoration.
    QWidget* previousWidget = NULL;
    if (previous != NULL && this->treeWidgetRundown->indexFromItem(previous).isValid())
        previousWidget = this->treeWidgetRundown->itemWidget(previous, 0);

    if (previous != NULL && previousWidget != NULL)
    {
        // Note: setSelected is now managed by itemSelectionChanged() for multi-selection support.
        if (!this->currentPlayingItems.values().contains(previous) && previous != this->currentPlayingAutoStepItem)
            RundownWidgetHelper::setSelectionHighlight(previousWidget, false);
    }

    if (current != NULL && currentWidget != NULL)
    {
        // Only activate visuals and fire inspector events when this pane is active.
        // This prevents the selection indicator from appearing in the unfocused pane
        // (e.g. when an item is dropped via drag-and-drop).
        if (this->active)
        {
            if (!this->currentPlayingItems.values().contains(current) && current != this->currentPlayingAutoStepItem)
                RundownWidgetHelper::setSelectionHighlight(currentWidget, true);
        }
    }

    // Only update the inspector when this pane is active.
    if (!this->active)
        return;

    QTreeWidgetItem* currentItem = this->treeWidgetRundown->currentItem();
    QWidget* currentItemWidget = this->treeWidgetRundown->itemWidget(currentItem, 0);

    QTreeWidgetItem* currentItemParent = NULL;
    if (currentItem != NULL)
        currentItemParent = this->treeWidgetRundown->currentItem()->parent();

    QWidget* currentItemWidgetParent = NULL;
    if (currentItemParent != NULL)
        currentItemWidgetParent = this->treeWidgetRundown->itemWidget(currentItemParent, 0);

    if (currentItem != NULL && currentItemWidget != NULL)
    {
        AbstractCommand* command = dynamic_cast<AbstractRundownWidget*>(currentItemWidget)->getCommand();
        LibraryModel* model = dynamic_cast<AbstractRundownWidget*>(currentItemWidget)->getLibraryModel();

        // Gather commands from all selected items for multi-edit support.
        QList<AbstractCommand*> allCommands;
        for (QTreeWidgetItem* sel : this->treeWidgetRundown->selectedItems())
        {
            QWidget* w = this->treeWidgetRundown->itemWidget(sel, 0);
            AbstractRundownWidget* rw = dynamic_cast<AbstractRundownWidget*>(w);
            if (rw != NULL && rw->getCommand() != NULL)
                allCommands.append(rw->getCommand());
        }

        EventManager::getInstance().fireRundownItemSelectedEvent(RundownItemSelectedEvent(command, model, currentItemWidget, currentItemWidgetParent, allCommands));
        EventManager::getInstance().fireSaveAsPresetMenuEvent(SaveAsPresetMenuEvent(true));
    }
    else if (currentItem == NULL && previous != NULL && this->treeWidgetRundown->invisibleRootItem()->childCount() == 1) // Last item was removed form the rundown.
    {
        EventManager::getInstance().fireEmptyRundownEvent(EmptyRundownEvent());
        EventManager::getInstance().fireSaveAsPresetMenuEvent(SaveAsPresetMenuEvent(false));
    }
}

void RundownTreeWidget::itemDoubleClicked(QTreeWidgetItem* item, int index)
{
    Q_UNUSED(index);

    QWidget* selectedWidget = this->treeWidgetRundown->itemWidget(this->treeWidgetRundown->currentItem(), 0);
    AbstractRundownWidget* rundownWidget = dynamic_cast<AbstractRundownWidget*>(selectedWidget);

    if (rundownWidget->isGroup()) // Group.
        rundownWidget->setExpanded(!item->isExpanded());
}

bool RundownTreeWidget::duplicateSelectedItems()
{
    return this->treeWidgetRundown->duplicateSelectedItems();
}

bool RundownTreeWidget::pasteSelectedItems()
{
    bool result = this->treeWidgetRundown->pasteSelectedItems(this->repositoryRundown);
    wireAllGatewayWidgets();
    return result;
}

bool RundownTreeWidget::copySelectedItems()
{
    return this->treeWidgetRundown->copySelectedItems();
}

void RundownTreeWidget::setUsed(bool used)
{
    if (this->treeWidgetRundown->currentItem() == NULL)
        return;

    QWidget* selectedWidget = this->treeWidgetRundown->itemWidget(this->treeWidgetRundown->currentItem(), 0);
    AbstractRundownWidget* rundownWidget = dynamic_cast<AbstractRundownWidget*>(selectedWidget);

    rundownWidget->setUsed(used);
    if (rundownWidget != NULL && rundownWidget->isGroup())
    {
        for (int i = 0; i < this->treeWidgetRundown->currentItem()->childCount(); i++)
        {
            QWidget* childWidget = this->treeWidgetRundown->itemWidget(this->treeWidgetRundown->currentItem()->child(i), 0);

            dynamic_cast<AbstractRundownWidget*>(childWidget)->setUsed(used);
        }
    }
}

void RundownTreeWidget::setAllUsed(bool used)
{
    for (int i = 0; i < this->treeWidgetRundown->invisibleRootItem()->childCount(); i++)
    {
        QTreeWidgetItem* currentItem = this->treeWidgetRundown->invisibleRootItem()->child(i);
        AbstractRundownWidget* rundownWidget = dynamic_cast<AbstractRundownWidget*>(this->treeWidgetRundown->itemWidget(currentItem, 0));

        rundownWidget->setUsed(used);
        if (rundownWidget != NULL && rundownWidget->isGroup())
        {
            for (int i = 0; i < currentItem->childCount(); i++)
            {
                QWidget* childWidget = this->treeWidgetRundown->itemWidget(currentItem->child(i), 0);

                dynamic_cast<AbstractRundownWidget*>(childWidget)->setUsed(used);
            }
        }
    }
}

bool RundownTreeWidget::executeCommand(Playout::PlayoutType type, Action::ActionType source, QTreeWidgetItem* item, bool allowPreview)
{
    // Determine if preview channel redirect is active for this execution.
    bool previewActive = allowPreview && shouldPreviewRedirect();

    // Helper: set or clear channel override on a widget for preview mode.
    // When preview is active, sets the override to the device's preview channel.
    // When preview is NOT active, clears any leftover override from a previous preview execution.
    // The override is NOT cleared after executeCommand() — it must persist through the
    // asynchronous itemScheduler so that getChannel() returns the preview channel when
    // the scheduler eventually fires executePlay().
    auto applyPreviewOverride = [&previewActive](AbstractRundownWidget* widget) {
        if (widget == nullptr || widget->getCommand() == nullptr)
            return;
        if (previewActive)
        {
            QString deviceName = widget->getLibraryModel()->getDeviceName();
            const QSharedPointer<DeviceModel> deviceModel = DeviceManager::getInstance().getDeviceModelByName(deviceName);
            if (deviceModel != nullptr && deviceModel->getPreviewChannel() > 0)
            {
                EventManager::getInstance().fireStatusbarEvent(
                    StatusbarEvent(QString("PVW: override ch→%1 (dev=%2)")
                        .arg(deviceModel->getPreviewChannel()).arg(deviceName)));
                widget->getCommand()->setChannelOverride(deviceModel->getPreviewChannel());
            }
            else
            {
                EventManager::getInstance().fireStatusbarEvent(
                    StatusbarEvent(QString("PVW: no preview channel on '%1'").arg(deviceName)));
            }
        }
        else
        {
            widget->getCommand()->clearChannelOverride();
        }
    };
    //QModelIndex currentIndex;
    QTreeWidgetItem* currentItem = nullptr;

    QWidget* selectedWidget = nullptr;
    QWidget* selectedWidgetParent = nullptr;

    AbstractRundownWidget* rundownWidget = nullptr;
    AbstractRundownWidget* rundownWidgetParent = nullptr;

    if (item == nullptr)
    {
        if (this->treeWidgetRundown->currentItem() == nullptr)
            return true;

        currentItem = this->treeWidgetRundown->currentItem();
        //currentIndex = this->treeWidgetRundown->currentIndex();

        selectedWidget = this->treeWidgetRundown->itemWidget(currentItem, 0);
        selectedWidgetParent = this->treeWidgetRundown->itemWidget(currentItem->parent(), 0);

        rundownWidget = dynamic_cast<AbstractRundownWidget*>(selectedWidget);
        rundownWidgetParent = dynamic_cast<AbstractRundownWidget*>(selectedWidgetParent);
    }
    else // External execution through OSC.
    {
        currentItem = item;
        //currentIndex = this->treeWidgetRundown->indexOfTopLevelItem(item);

        selectedWidget = this->treeWidgetRundown->itemWidget(currentItem, 0);
        selectedWidgetParent = this->treeWidgetRundown->itemWidget(currentItem->parent(), 0);

        rundownWidget = dynamic_cast<AbstractRundownWidget*>(selectedWidget);
        rundownWidgetParent = dynamic_cast<AbstractRundownWidget*>(selectedWidgetParent);
    }

    // Dropdown group: the group is a chooser, not a container to fire as a whole.
    // Every action aimed at it (Play button, F2, OSC, preview) is redirected to
    // the child currently selected in its dropdown.
    if (rundownWidget != nullptr)
    {
        if (GroupCommand* dropdownGroup = dynamic_cast<GroupCommand*>(rundownWidget->getCommand()))
        {
            if (dropdownGroup->getTreatAsDropdown() && currentItem != nullptr && currentItem->childCount() > 0)
            {
                int selected = qBound(0, dropdownGroup->getDropdownIndex(), currentItem->childCount() - 1);
                QTreeWidgetItem* chosen = currentItem->child(selected);
                QWidget* chosenWidget = this->treeWidgetRundown->itemWidget(chosen, 0);
                AbstractRundownWidget* chosenRundown = dynamic_cast<AbstractRundownWidget*>(chosenWidget);
                if (chosenRundown != nullptr)
                {
                    currentItem = chosen;
                    selectedWidget = chosenWidget;
                    selectedWidgetParent = this->treeWidgetRundown->itemWidget(chosen->parent(), 0);
                    rundownWidget = chosenRundown;
                    rundownWidgetParent = dynamic_cast<AbstractRundownWidget*>(selectedWidgetParent);
                }
            }
        }
    }

    if (source == Action::ActionType::GpiPulse && !rundownWidget->getCommand()->getAllowGpi())
        return true; // Gpi pulses cannot trigger this item.

    // Check disabled — block execution on disabled items or items inside a disabled group.
    if (rundownWidget->getCommand() != nullptr && rundownWidget->getCommand()->getDisabled())
        return true;
    {
        QTreeWidgetItem* p = currentItem ? currentItem->parent() : nullptr;
        while (p != nullptr)
        {
            AbstractRundownWidget* pw = dynamic_cast<AbstractRundownWidget*>(this->treeWidgetRundown->itemWidget(p, 0));
            if (pw != nullptr && pw->getCommand() != nullptr && pw->getCommand()->getDisabled())
                return true;
            p = p->parent();
        }
    }

    // Check channel lock — block execution on locked channels.
    // In preview mode, check the preview channel instead of the original.
    if (!rundownWidget->isGroup() && rundownWidget->getCommand() != nullptr)
    {
        QString deviceName = rundownWidget->getLibraryModel()->getDeviceName();
        int ch = rundownWidget->getCommand()->getChannel();
        if (previewActive)
        {
            const QSharedPointer<DeviceModel> dm = DeviceManager::getInstance().getDeviceModelByName(deviceName);
            if (dm != nullptr && dm->getPreviewChannel() > 0)
                ch = dm->getPreviewChannel();
        }
        if (DeviceManager::getInstance().isChannelLocked(deviceName, ch))
        {
            EventManager::getInstance().fireStatusbarEvent(
                StatusbarEvent(QString("Channel %1 is locked on %2").arg(ch).arg(deviceName), 3000, true));
            return true;
        }
    }

    // Autoplay Gateway: focus-jump to partner (same as Gateway).
    if (rundownWidget->getLibraryModel()->getType() == Rundown::AUTOPLAYGATEWAY)
    {
        GatewayCommand* tCmd = dynamic_cast<GatewayCommand*>(rundownWidget->getCommand());
        if (tCmd != nullptr)
        {
            bool partnerIsExit = !tCmd->getIsExit();
            QString targetLabel = partnerIsExit ? tCmd->getEffectiveExitLabel() : QString();

            GatewayExitLocation loc = findGatewayPartnerInTree(tCmd->getGatewayId(), partnerIsExit, targetLabel);
            if (loc.exitItem != nullptr)
            {
                this->treeWidgetRundown->setCurrentItem(loc.exitItem);
                this->treeWidgetRundown->scrollToItem(loc.exitItem, QAbstractItemView::EnsureVisible);
            }
            else
            {
                emit requestCrossTabFocusGateway(tCmd->getGatewayId(), tCmd->getIsExit(), targetLabel);
            }
        }
        return true;
    }

    // Command Gateway: relay command to item after matching exit.
    if (rundownWidget->getLibraryModel()->getType() == Rundown::COMMANDGATEWAY)
    {
        GatewayCommand* tCmd = dynamic_cast<GatewayCommand*>(rundownWidget->getCommand());
        if (tCmd != nullptr && tCmd->getIsExit())
        {
            // Exit: focus-jump to entrance.
            GatewayExitLocation loc = findGatewayPartnerInTree(tCmd->getGatewayId(), false, QString());
            if (loc.exitItem != nullptr)
            {
                this->treeWidgetRundown->setCurrentItem(loc.exitItem);
                this->treeWidgetRundown->scrollToItem(loc.exitItem, QAbstractItemView::EnsureVisible);
            }
            else
            {
                emit requestCrossTabFocusGateway(tCmd->getGatewayId(), tCmd->getIsExit(), QString());
            }
            return true;
        }
        if (tCmd != nullptr && !tCmd->getIsExit())
        {
            // Entrance: find matching exit and relay command to item after it.
            QTreeWidgetItem* parent = currentItem->parent();
            QTreeWidgetItem* container = (parent != nullptr) ? parent : this->treeWidgetRundown->invisibleRootItem();
            int childCount = container->childCount();
            int exitIdx = -1;
            int firstExitIdx = -1;
            QString selectedExit = tCmd->getEffectiveExitLabel();

            for (int j = 0; j < childCount; j++)
            {
                QWidget* cw = this->treeWidgetRundown->itemWidget(container->child(j), 0);
                AbstractRundownWidget* crw = dynamic_cast<AbstractRundownWidget*>(cw);
                GatewayCommand* ct = dynamic_cast<GatewayCommand*>(crw->getCommand());
                if (ct != nullptr && ct->getIsExit() && ct->getGatewayId() == tCmd->getGatewayId())
                {
                    if (firstExitIdx < 0)
                        firstExitIdx = j;
                    if (ct->getExitLabel() == selectedExit)
                    {
                        exitIdx = j;
                        break;
                    }
                }
            }
            if (exitIdx < 0)
                exitIdx = firstExitIdx;
            if (exitIdx >= 0 && exitIdx + 1 < childCount)
            {
                QTreeWidgetItem* targetItem = container->child(exitIdx + 1);
                this->treeWidgetRundown->setCurrentItem(targetItem);
                return executeCommand(type, source, targetItem);
            }
        }
        // No local exit: try cross-tab gateway relay.
        emit requestCrossTabGatewayRelay(tCmd->getGatewayId(), type, tCmd->getEffectiveExitLabel());
        return true;
    }

    // Focus Gateway: find partner and jump focus to it.
    if (rundownWidget->getLibraryModel()->getType() == Rundown::FOCUSGATEWAY)
    {
        GatewayCommand* tCmd = dynamic_cast<GatewayCommand*>(rundownWidget->getCommand());
        if (tCmd != nullptr)
        {
            bool partnerIsExit = !tCmd->getIsExit();
            QString targetLabel = partnerIsExit ? tCmd->getEffectiveExitLabel() : QString();

            GatewayExitLocation loc = findGatewayPartnerInTree(tCmd->getGatewayId(), partnerIsExit, targetLabel);
            if (loc.exitItem != nullptr)
            {
                this->treeWidgetRundown->setCurrentItem(loc.exitItem);
                this->treeWidgetRundown->scrollToItem(loc.exitItem, QAbstractItemView::EnsureVisible);
            }
            else
            {
                emit requestCrossTabFocusGateway(tCmd->getGatewayId(), tCmd->getIsExit(), targetLabel);
            }
        }
        return true;
    }

    // Helper to show last action in statusbar (if enabled).
    bool showLastAction = DatabaseManager::getInstance().getConfigurationByName("ShowLastAction").getValue() == "true";
    auto fireLastActionEvent = [&type, showLastAction](AbstractRundownWidget* widget) {
        if (widget == nullptr || widget->getCommand() == nullptr)
            return;
        // Skip groups for most actions, but always log clear commands.
        if (widget->isGroup() &&
            type != Playout::PlayoutType::Clear &&
            type != Playout::PlayoutType::ClearVideoLayer &&
            type != Playout::PlayoutType::ClearChannel)
            return;

        static const QMap<Playout::PlayoutType, QString> actionNames = {
            {Playout::PlayoutType::Stop, "Stop"}, {Playout::PlayoutType::Play, "Play"},
            {Playout::PlayoutType::PlayNow, "Play"}, {Playout::PlayoutType::Load, "Load"},
            {Playout::PlayoutType::PauseResume, "Pause"}, {Playout::PlayoutType::Next, "Next"},
            {Playout::PlayoutType::Update, "Update"}, {Playout::PlayoutType::Invoke, "Invoke"},
            {Playout::PlayoutType::Preview, "Preview"}, {Playout::PlayoutType::Clear, "Clear"},
            {Playout::PlayoutType::ClearVideoLayer, "Clear VL"}, {Playout::PlayoutType::ClearChannel, "Clear CH"}
        };
        QString action = actionNames.value(type, "Exec");
        QString label = widget->getLibraryModel()->getLabel();
        if (label.isEmpty()) label = widget->getLibraryModel()->getName();
        QString device = widget->getLibraryModel()->getDeviceName();
        int ch = widget->getCommand()->getChannel();
        int vl = widget->getCommand()->getVideolayer();

        // Always emit for the log panel (has its own enable/disable toggle).
        emit EventManager::getInstance().playoutAction(action, label, device, ch, vl);

        if (showLastAction)
            EventManager::getInstance().fireStatusbarEvent(
                StatusbarEvent(QString("%1: %2 on %3 CH%4-L%5").arg(action, label, device).arg(ch).arg(vl), 0));
    };

    // Helper to fire channel activity events for the status panel.
    auto fireActivityEvent = [this, &type, previewActive](AbstractRundownWidget* widget) {
        if (widget == nullptr || widget->getCommand() == nullptr || widget->isGroup())
            return;

        int ch = widget->getCommand()->getChannel();
        int vl = widget->getCommand()->getVideolayer();
        QString label = widget->getLibraryModel()->getLabel();
        if (label.isEmpty()) label = widget->getLibraryModel()->getName();
        if (label.isEmpty()) label = widget->getLibraryModel()->getType();
        QString itemType = widget->getLibraryModel()->getType();

        bool active = (type == Playout::PlayoutType::Play ||
                       type == Playout::PlayoutType::PlayNow ||
                       type == Playout::PlayoutType::Load ||
                       type == Playout::PlayoutType::Update ||
                       type == Playout::PlayoutType::Preview);

        // For media items being activated: skip if file not in library.
        if (active && (itemType == Rundown::MOVIE || itemType == Rundown::STILL ||
                       itemType == Rundown::AUDIO || itemType == Rundown::IMAGESCROLLER ||
                       itemType == Rundown::TEMPLATE))
        {
            QString name = widget->getLibraryModel()->getName();
            QString deviceName = widget->getLibraryModel()->getDeviceName();
            const QSharedPointer<DeviceModel> dm = DeviceManager::getInstance().getDeviceModelByName(deviceName);
            if (dm == nullptr || DatabaseManager::getInstance().getLibraryByNameAndDeviceId(name, dm->getId()).isEmpty())
                return;
        }

        EventManager::getInstance().fireChannelActivityEvent(
            ChannelActivityEvent(ch, vl, label, itemType, active));

        // When deactivating, also deactivate preview channel entry in case the item
        // was previously played on the preview channel (override now cleared).
        if (!active)
        {
            QString deviceName = widget->getLibraryModel()->getDeviceName();
            const QSharedPointer<DeviceModel> deviceModel = DeviceManager::getInstance().getDeviceModelByName(deviceName);
            if (deviceModel != nullptr && deviceModel->getPreviewChannel() > 0
                && deviceModel->getPreviewChannel() != ch)
            {
                EventManager::getInstance().fireChannelActivityEvent(
                    ChannelActivityEvent(deviceModel->getPreviewChannel(), vl, label, itemType, false));
            }
        }

        // A sheet-driven template reads the sheet itself every time it renders, so a
        // play is API traffic the client cannot see. Count it for the strain meter.
        if (type == Playout::PlayoutType::Play || type == Playout::PlayoutType::PlayNow)
        {
            if (TemplateCommand* sheetTemplate = dynamic_cast<TemplateCommand*>(widget->getCommand()))
                SheetDataResolver::getInstance().noteTemplatePlayed(widget->getLibraryModel()->getDeviceName(),
                                                                    sheetTemplate->getTemplateName());
        }

        // Preview bookkeeping: remember the preview layers we light, and when the
        // item is taken to program clear ONLY its own preview layer — other items
        // parked on other layers of the preview channel must stay up.
        {
            QString previewDevice = widget->getLibraryModel()->getDeviceName();
            const QSharedPointer<DeviceModel> previewModel =
                DeviceManager::getInstance().getDeviceModelByName(previewDevice);
            if (previewModel != nullptr && previewModel->getPreviewChannel() > 0)
            {
                QString key = QString("%1|%2|%3").arg(previewDevice)
                    .arg(previewModel->getPreviewChannel()).arg(vl);

                if (type == Playout::PlayoutType::Preview || previewActive)
                {
                    this->previewedLayers.insert(key);
                }
                else if ((type == Playout::PlayoutType::Play || type == Playout::PlayoutType::PlayNow)
                         && this->previewedLayers.remove(key))
                {
                    const QSharedPointer<CasparDevice> previewCasparDevice =
                        DeviceManager::getInstance().getDeviceByName(previewDevice);
                    if (previewCasparDevice != nullptr && previewCasparDevice->isConnected())
                        previewCasparDevice->clearVideolayer(previewModel->getPreviewChannel(), vl);
                }
            }
        }

        // A clear wipes more than the executed item: broadcast it (same event the
        // Clear Output panic item fires) so other items' auto-loops on the affected
        // channel/layer stop and the Activity panel sweeps every matching row.
        if (type == Playout::PlayoutType::ClearChannel)
            EventManager::getInstance().fireChannelClearedEvent(widget->getLibraryModel()->getDeviceName(), ch, -1);
        else if (type == Playout::PlayoutType::Clear || type == Playout::PlayoutType::ClearVideoLayer)
            EventManager::getInstance().fireChannelClearedEvent(widget->getLibraryModel()->getDeviceName(), ch, vl);
    };

    if (type == Playout::PlayoutType::Next && rundownWidgetParent != nullptr && rundownWidgetParent->isGroup() && dynamic_cast<GroupCommand*>(rundownWidgetParent->getCommand())->getAutoPlay())
    {
        EventManager::getInstance().fireAutoPlayNextRundownItemEvent(AutoPlayNextRundownItemEvent(dynamic_cast<QWidget*>(this->currentAutoPlayWidget)));

        return true;
    }
    else
    {
        // Get the channel for this item to track "last fired" state.
        int channel = rundownWidget->getCommand()->getChannel();
        bool perChannel = DatabaseManager::getInstance().getConfigurationByName("ActiveIndicatorPerChannel").getValue() != "false";

        if (perChannel)
        {
            // Deactivate previous item for this channel only.
            if (this->currentPlayingItems.contains(channel) && this->currentPlayingItems[channel] != nullptr)
            {
                QWidget* prevWidget = this->treeWidgetRundown->itemWidget(this->currentPlayingItems[channel], 0);
                if (prevWidget != nullptr)
                    dynamic_cast<AbstractRundownWidget*>(prevWidget)->setActive(false);
            }
        }
        else
        {
            // Legacy mode: deactivate ALL previous items across all channels.
            for (auto it = this->currentPlayingItems.begin(); it != this->currentPlayingItems.end(); ++it)
            {
                if (it.value() != nullptr)
                {
                    QWidget* prevWidget = this->treeWidgetRundown->itemWidget(it.value(), 0);
                    if (prevWidget != nullptr)
                        dynamic_cast<AbstractRundownWidget*>(prevWidget)->setActive(false);
                }
            }
            this->currentPlayingItems.clear();
        }

        dynamic_cast<AbstractRundownWidget*>(selectedWidget)->setActive(true);

        // Same moment the rundown marks its active item — the Simple Mode grid uses
        // this to light the tally on the matching key.
        EventManager::getInstance().fireRundownItemFiredEvent(currentItem, channel);

        // For top-level movies with autoPlay, use Next (direct PLAY) instead of Play (LOADBG AUTO).
        // LOADBG AUTO on an empty CasparCG layer doesn't start playback.
        Playout::PlayoutType executeType = type;
        if (type == Playout::PlayoutType::Play && currentItem->parent() == nullptr
            && !rundownWidget->isGroup())
        {
            if (MovieCommand* mc = dynamic_cast<MovieCommand*>(rundownWidget->getCommand()))
            {
                if (mc->getAutoPlay())
                    executeType = Playout::PlayoutType::Next;
            }
        }

        applyPreviewOverride(rundownWidget);
        dynamic_cast<AbstractPlayoutCommand*>(selectedWidget)->executeCommand(executeType);
        fireActivityEvent(rundownWidget);
        fireLastActionEvent(rundownWidget);

        this->currentPlayingItems[channel] = currentItem;

        // Global autostep: advance to next item after play.
        if (EventManager::getInstance().getAutostepMode() &&
            (type == Playout::PlayoutType::Play || type == Playout::PlayoutType::PlayNow))
        {
            // Clear previous autostep highlight.
            setAutostepHighlight(this->currentAutostepHighlightItem, false);

            if (rundownWidget->isGroup())
            {
                // Expanded group: step into first child. Collapsed group: stay on group.
                if (currentItem->isExpanded() && currentItem->childCount() > 0)
                {
                    QTreeWidgetItem* firstChild = currentItem->child(0);
                    QTreeWidgetItem* previousItem = this->treeWidgetRundown->currentItem();
                    this->treeWidgetRundown->setCurrentItem(firstChild);
                    EventManager::getInstance().fireCurrentItemChangedEvent(
                        CurrentItemChangedEvent(firstChild, previousItem));
                }
            }
            else
            {
                selectItemBelow();
            }

            // Apply purple autostep highlight on the new position.
            this->currentAutostepHighlightItem = this->treeWidgetRundown->currentItem();
            setAutostepHighlight(this->currentAutostepHighlightItem, true);
        }
    }

    if (rundownWidget != nullptr && rundownWidget->isGroup())
    {
        if (type == Playout::PlayoutType::Next && dynamic_cast<GroupCommand*>(rundownWidget->getCommand())->getAutoPlay())
        {
            if (this->currentAutoPlayWidget != nullptr)
                EventManager::getInstance().fireAutoPlayNextRundownItemEvent(AutoPlayNextRundownItemEvent(dynamic_cast<QWidget*>(this->currentAutoPlayWidget)));
        }
        else if (type == Playout::PlayoutType::PauseResume && dynamic_cast<GroupCommand*>(rundownWidget->getCommand())->getAutoPlay())
        {
            if (this->currentAutoPlayWidget != nullptr)
            {
                applyPreviewOverride(this->currentAutoPlayWidget);
                dynamic_cast<AbstractPlayoutCommand*>(this->currentAutoPlayWidget)->executeCommand(type);
            }
        }
        else if ((type == Playout::PlayoutType::Play || type == Playout::PlayoutType::Load) && dynamic_cast<GroupCommand*>(rundownWidget->getCommand())->getAutoPlay())
        {
            // The group have AutoPlay enabled, play the items within the group.
            bool isFirstChild = true;

            // Sync the group's autoPlay setting to all child items before building the queue.
            // This ensures stills and movies have their autoPlay property set correctly.
            for (int i = 0; i < currentItem->childCount(); i++)
            {
                QTreeWidgetItem* childTreeItem = currentItem->child(i);
                QWidget* childWidget = this->treeWidgetRundown->itemWidget(childTreeItem, 0);
                AbstractRundownWidget* rundownChildWidget = dynamic_cast<AbstractRundownWidget*>(childWidget);
                if (MovieCommand* movieCmd = dynamic_cast<MovieCommand*>(rundownChildWidget->getCommand()))
                    movieCmd->setAutoPlay(true);
                else if (StillCommand* stillCmd = dynamic_cast<StillCommand*>(rundownChildWidget->getCommand()))
                    stillCmd->setAutoPlay(true);

                // Also sync inner group children.
                if (rundownChildWidget->isGroup())
                {
                    for (int j = 0; j < childTreeItem->childCount(); j++)
                    {
                        QWidget* gcw = this->treeWidgetRundown->itemWidget(childTreeItem->child(j), 0);
                        AbstractRundownWidget* gcRundown = dynamic_cast<AbstractRundownWidget*>(gcw);
                        if (MovieCommand* mc = dynamic_cast<MovieCommand*>(gcRundown->getCommand()))
                            mc->setAutoPlay(true);
                        else if (StillCommand* sc = dynamic_cast<StillCommand*>(gcRundown->getCommand()))
                            sc->setAutoPlay(true);
                    }
                }
            }

            // Build a flat list of items to queue, flattening expanded inner groups.
            struct QueueCandidate {
                AbstractRundownWidget* widget;
                bool isCollapsedInnerGroup;
                QTreeWidgetItem* treeItem;
            };
            QList<QueueCandidate> candidates;
            for (int i = 0; i < currentItem->childCount(); i++)
            {
                QTreeWidgetItem* childTreeItem = currentItem->child(i);
                QWidget* childWidget = this->treeWidgetRundown->itemWidget(childTreeItem, 0);
                AbstractRundownWidget* rundownChildWidget = dynamic_cast<AbstractRundownWidget*>(childWidget);

                if (rundownChildWidget->isGroup())
                {
                    if (childTreeItem->isExpanded())
                    {
                        // Expanded inner group: flatten children into queue.
                        for (int j = 0; j < childTreeItem->childCount(); j++)
                        {
                            QTreeWidgetItem* gcTreeItem = childTreeItem->child(j);
                            QWidget* gcw = this->treeWidgetRundown->itemWidget(gcTreeItem, 0);
                            AbstractRundownWidget* gcrw = dynamic_cast<AbstractRundownWidget*>(gcw);
                            candidates.append({gcrw, false, gcTreeItem});
                        }
                    }
                    else
                    {
                        // Collapsed inner group: treat as single batch item.
                        candidates.append({rundownChildWidget, true, childTreeItem});
                    }
                }
                else
                {
                    candidates.append({rundownChildWidget, false, childTreeItem});
                }
            }

            QList<AbstractRundownWidget*>* autoPlayQueue = new QList<AbstractRundownWidget*>();
            for (const QueueCandidate& candidate : candidates)
            {
                AbstractRundownWidget* rundownChildWidget = candidate.widget;

                // Collapsed inner groups are batch items.
                if (candidate.isCollapsedInnerGroup)
                {
                    if (isFirstChild)
                    {
                        // Batch-execute all children of the collapsed inner group.
                        for (int j = 0; j < candidate.treeItem->childCount(); j++)
                        {
                            QWidget* gcw = this->treeWidgetRundown->itemWidget(candidate.treeItem->child(j), 0);
                            AbstractRundownWidget* gcRundown = dynamic_cast<AbstractRundownWidget*>(gcw);
                            applyPreviewOverride(gcRundown);
                            dynamic_cast<AbstractPlayoutCommand*>(gcRundown)->executeCommand(type);
                            fireActivityEvent(gcRundown);
                            fireLastActionEvent(gcRundown);
                        }

                        if (type == Playout::PlayoutType::Load)
                            break;

                        // Find trigger child (first movie or still with autoPlay) for queue tracking.
                        // The trigger child fires AutoPlayRundownItemEvent, which advances the queue.
                        AbstractRundownWidget* triggerChild = nullptr;
                        for (int j = 0; j < candidate.treeItem->childCount(); j++)
                        {
                            QWidget* gcw = this->treeWidgetRundown->itemWidget(candidate.treeItem->child(j), 0);
                            AbstractRundownWidget* gcRundown = dynamic_cast<AbstractRundownWidget*>(gcw);
                            if (MovieCommand* mc = dynamic_cast<MovieCommand*>(gcRundown->getCommand()))
                            {
                                if (mc->getAutoPlay()) { triggerChild = gcRundown; break; }
                            }
                            else if (!triggerChild)
                            {
                                if (StillCommand* sc = dynamic_cast<StillCommand*>(gcRundown->getCommand()))
                                {
                                    if (sc->getAutoPlay() && sc->getDuration() > 0)
                                        triggerChild = gcRundown;
                                }
                            }
                        }

                        if (triggerChild != nullptr)
                        {
                            autoPlayQueue->push_back(triggerChild);
                            setAutoPlayHighlight(this->currentAutoPlayWidget, false);
                            this->currentAutoPlayWidget = triggerChild;
                            setAutoPlayHighlight(this->currentAutoPlayWidget, true);
                        }
                    }
                    else
                    {
                        // Non-first collapsed inner group: add group widget as placeholder.
                        // Will be batch-executed when reached in autoPlayRundownItem().
                        autoPlayQueue->push_back(rundownChildWidget);
                    }
                    isFirstChild = false;
                    continue;
                }

                // Skip locked channels.
                if (rundownChildWidget->getCommand() != nullptr)
                {
                    QString childDevice = rundownChildWidget->getLibraryModel()->getDeviceName();
                    int childCh = rundownChildWidget->getCommand()->getChannel();
                    if (previewActive)
                    {
                        const QSharedPointer<DeviceModel> dm = DeviceManager::getInstance().getDeviceModelByName(childDevice);
                        if (dm != nullptr && dm->getPreviewChannel() > 0)
                            childCh = dm->getPreviewChannel();
                    }
                    if (DeviceManager::getInstance().isChannelLocked(childDevice, childCh))
                    {
                        EventManager::getInstance().fireStatusbarEvent(
                            StatusbarEvent(QString("Channel %1 is locked on %2 (skipped)").arg(childCh).arg(childDevice), 3000, true));
                        continue;
                    }
                }

                // Check if it's a movie with AutoPlay, or a still with AutoPlay and Duration.
                bool shouldQueue = false;
                bool isGateway = (rundownChildWidget->getLibraryModel()->getType() == Rundown::AUTOPLAYGATEWAY);
                if (dynamic_cast<MovieCommand*>(rundownChildWidget->getCommand()))
                {
                    if (dynamic_cast<MovieCommand*>(rundownChildWidget->getCommand())->getAutoPlay())
                        shouldQueue = true;
                }
                else if (dynamic_cast<StillCommand*>(rundownChildWidget->getCommand()))
                {
                    StillCommand* stillCmd = dynamic_cast<StillCommand*>(rundownChildWidget->getCommand());
                    if (stillCmd->getAutoPlay() && stillCmd->getDuration() > 0)
                        shouldQueue = true;
                }

                if (shouldQueue)
                {
                    if (isFirstChild)
                    {
                        applyPreviewOverride(rundownChildWidget);
                        if (type == Playout::PlayoutType::Play && dynamic_cast<MovieCommand*>(rundownChildWidget->getCommand()))
                            dynamic_cast<AbstractPlayoutCommand*>(rundownChildWidget)->executeCommand(Playout::PlayoutType::Next);
                        else
                            dynamic_cast<AbstractPlayoutCommand*>(rundownChildWidget)->executeCommand(type);
                        fireActivityEvent(rundownChildWidget);
                        fireLastActionEvent(rundownChildWidget);
                        if (type == Playout::PlayoutType::Load)
                            break;

                        setAutoPlayHighlight(this->currentAutoPlayWidget, false);
                        this->currentAutoPlayWidget = rundownChildWidget;
                        setAutoPlayHighlight(this->currentAutoPlayWidget, true);
                    }

                    autoPlayQueue->push_back(rundownChildWidget);
                    isFirstChild = false;
                }
                else if (isGateway && !isFirstChild)
                {
                    autoPlayQueue->push_back(rundownChildWidget);
                }
                else if (!isGateway)
                {
                    applyPreviewOverride(rundownChildWidget);
                    dynamic_cast<AbstractPlayoutCommand*>(rundownChildWidget)->executeCommand(type);
                    fireActivityEvent(rundownChildWidget);
                    fireLastActionEvent(rundownChildWidget);
                }
            }

            if (autoPlayQueue->count() > 0)
            {
                AutoPlayQueueInfo queueInfo;
                queueInfo.queue = autoPlayQueue;
                queueInfo.groupItem = currentItem;
                queueInfo.isTopLevelChain = (currentItem->parent() == nullptr);
                this->autoPlayQueues.push_back(queueInfo);
            }
            else
            {
                delete autoPlayQueue;
            }
        }
        else
        {
            // Execute command on the selected item.
            for (int i = 0; i < currentItem->childCount(); i++)
            {
                QWidget* childWidget = this->treeWidgetRundown->itemWidget(currentItem->child(i), 0);
                AbstractRundownWidget* childRundownWidget = dynamic_cast<AbstractRundownWidget*>(childWidget);

                // Skip locked channels.
                // In preview mode, check the preview channel instead.
                if (childRundownWidget != nullptr && childRundownWidget->getCommand() != nullptr)
                {
                    QString childDevice = childRundownWidget->getLibraryModel()->getDeviceName();
                    int childCh = childRundownWidget->getCommand()->getChannel();
                    if (previewActive)
                    {
                        const QSharedPointer<DeviceModel> dm = DeviceManager::getInstance().getDeviceModelByName(childDevice);
                        if (dm != nullptr && dm->getPreviewChannel() > 0)
                            childCh = dm->getPreviewChannel();
                    }
                    if (DeviceManager::getInstance().isChannelLocked(childDevice, childCh))
                    {
                        EventManager::getInstance().fireStatusbarEvent(
                            StatusbarEvent(QString("Channel %1 is locked on %2 (skipped)").arg(childCh).arg(childDevice), 3000, true));
                        continue;
                    }
                }

                EventManager::getInstance().fireRemoveItemFromAutoPlayQueueEvent(RemoveItemFromAutoPlayQueueEvent(currentItem->child(i)));

                applyPreviewOverride(childRundownWidget);
                dynamic_cast<AbstractPlayoutCommand*>(childWidget)->executeCommand(type);
                fireActivityEvent(childRundownWidget);
                fireLastActionEvent(childRundownWidget);
            }

            if (type == Playout::PlayoutType::Preview)
                return true; // We are done.
        }

    }
    else if (rundownWidgetParent != nullptr && rundownWidgetParent->isGroup())
    {
        // The selected items parent is a group. If the group have AutoPlay property set, then play current item and below within the group.
        if (type == Playout::PlayoutType::Play && dynamic_cast<GroupCommand*>(rundownWidgetParent->getCommand())->getAutoPlay())
        {
            QList<AbstractRundownWidget*>* autoPlayQueue = new QList<AbstractRundownWidget*>();
            for (int i = currentItem->parent()->indexOfChild(currentItem); i < currentItem->parent()->childCount(); i++)
            {
                QWidget* childWidget = this->treeWidgetRundown->itemWidget(currentItem->parent()->child(i), 0);
                AbstractRundownWidget* rundownChildWidget = dynamic_cast<AbstractRundownWidget*>(childWidget);

                // Check if it's a movie with AutoPlay, or a still with AutoPlay and Duration.
                bool shouldQueue = false;
                if (dynamic_cast<MovieCommand*>(rundownChildWidget->getCommand()))
                {
                    if (dynamic_cast<MovieCommand*>(rundownChildWidget->getCommand())->getAutoPlay())
                        shouldQueue = true;
                }
                else if (dynamic_cast<StillCommand*>(rundownChildWidget->getCommand()))
                {
                    // Include stills that have autoPlay enabled and duration set (for auto-advance).
                    StillCommand* stillCmd = dynamic_cast<StillCommand*>(rundownChildWidget->getCommand());
                    if (stillCmd->getAutoPlay() && stillCmd->getDuration() > 0)
                        shouldQueue = true;
                }

                if (shouldQueue)
                    autoPlayQueue->push_back(rundownChildWidget); // Add our widget to the execution queue.
            }

            if (autoPlayQueue->count() > 0)
            {
                AutoPlayQueueInfo queueInfo;
                queueInfo.queue = autoPlayQueue;
                queueInfo.groupItem = currentItem->parent();
                this->autoPlayQueues.push_back(queueInfo);
            }
            else
            {
                delete autoPlayQueue;
            }
        }
    }
    else if (rundownWidget != nullptr && !rundownWidget->isGroup()
             && currentItem->parent() == nullptr
             && type == Playout::PlayoutType::Play)
    {
        // Top-level non-group item with autoPlay: create single-item queue for chaining.
        bool hasAutoPlay = false;
        if (MovieCommand* mc = dynamic_cast<MovieCommand*>(rundownWidget->getCommand()))
            hasAutoPlay = mc->getAutoPlay();
        else if (StillCommand* sc = dynamic_cast<StillCommand*>(rundownWidget->getCommand()))
            hasAutoPlay = sc->getAutoPlay() && sc->getDuration() > 0;
        else if (TemplateCommand* tc = dynamic_cast<TemplateCommand*>(rundownWidget->getCommand()))
            hasAutoPlay = tc->getAutoPlay() && tc->getDuration() > 0;

        if (hasAutoPlay)
        {
            QList<AbstractRundownWidget*>* autoPlayQueue = new QList<AbstractRundownWidget*>();
            autoPlayQueue->push_back(rundownWidget);

            AutoPlayQueueInfo queueInfo;
            queueInfo.queue = autoPlayQueue;
            queueInfo.groupItem = nullptr;
            queueInfo.isTopLevelChain = true;
            this->autoPlayQueues.push_back(queueInfo);
            setAutoPlayHighlight(this->currentAutoPlayWidget, false);
            this->currentAutoPlayWidget = rundownWidget;
            setAutoPlayHighlight(this->currentAutoPlayWidget, true);
        }
    }

    return true;
}

void RundownTreeWidget::selectItemBelow()
{
    this->treeWidgetRundown->selectItemBelow();
}

void RundownTreeWidget::executePreview()
{
    EventManager::getInstance().fireExecutePlayoutCommandEvent(ExecutePlayoutCommandEvent(QEvent::KeyPress, Qt::Key_F8, Qt::NoModifier));
}

void RundownTreeWidget::addBlendModeItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::BLENDMODE);
}

void RundownTreeWidget::addBrightnessItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::BRIGHTNESS);
}

void RundownTreeWidget::addContrastItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::CONTRAST);
}

void RundownTreeWidget::addClipItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::CLIP);
}

void RundownTreeWidget::addCropItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::CROP);
}

void RundownTreeWidget::addImageScrollerItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::IMAGESCROLLER);
}

void RundownTreeWidget::addDeckLinkInputItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::DECKLINKINPUT);
}

void RundownTreeWidget::addPrintItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::PRINT);
}

void RundownTreeWidget::addClearOutputItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::CLEAROUTPUT);
}

void RundownTreeWidget::addFillItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::FILL);
}

void RundownTreeWidget::addGpiOutputItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::GPIOUTPUT);
}

void RundownTreeWidget::addHttpGetItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::HTTPGET);
}

void RundownTreeWidget::addHttpPostItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::HTTPPOST);
}

void RundownTreeWidget::addShellCommandItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::SHELLCOMMAND);
}

void RundownTreeWidget::addOscOutputItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::OSCOUTPUT);
}

void RundownTreeWidget::addPlayoutCommandItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::PLAYOUTCOMMAND);
}

void RundownTreeWidget::addRouteChannelItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::ROUTECHANNEL);
}

void RundownTreeWidget::addRouteVideolayerItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::ROUTEVIDEOLAYER);
}

void RundownTreeWidget::addFileRecorderItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::FILERECORDER);
}

void RundownTreeWidget::addSeparatorItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::SEPARATOR);
}

void RundownTreeWidget::addStopAutoLoopsItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::STOPAUTOLOOPS);
}

void RundownTreeWidget::addAutoPlayGatewayItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::AUTOPLAYGATEWAY);
}

void RundownTreeWidget::addFocusGatewayItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::FOCUSGATEWAY);
}

void RundownTreeWidget::addCommandGatewayItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::COMMANDGATEWAY);
}

void RundownTreeWidget::addGatewayExit()
{
    if (this->treeWidgetRundown->selectedItems().count() != 1)
        return;

    QTreeWidgetItem* currentItem = this->treeWidgetRundown->currentItem();
    QWidget* currentWidget = this->treeWidgetRundown->itemWidget(currentItem, 0);
    AbstractRundownWidget* rw = dynamic_cast<AbstractRundownWidget*>(currentWidget);
    QString gatewayType = rw->getLibraryModel()->getType();
    if (rw == nullptr || (gatewayType != Rundown::AUTOPLAYGATEWAY && gatewayType != Rundown::FOCUSGATEWAY && gatewayType != Rundown::COMMANDGATEWAY))
        return;

    GatewayCommand* tCmd = dynamic_cast<GatewayCommand*>(rw->getCommand());
    if (tCmd == nullptr)
        return;

    QString gatewayId = tCmd->getGatewayId();

    // Count existing exits for this gatewayId to determine next label letter.
    int exitCount = 0;
    QTreeWidgetItem* lastExitItem = nullptr;
    QTreeWidgetItem* parent = currentItem->parent();
    QTreeWidgetItem* container = (parent != nullptr) ? parent : this->treeWidgetRundown->invisibleRootItem();
    for (int i = 0; i < container->childCount(); i++)
    {
        QWidget* cw = this->treeWidgetRundown->itemWidget(container->child(i), 0);
        AbstractRundownWidget* crw = dynamic_cast<AbstractRundownWidget*>(cw);
        if (crw == nullptr) continue;
        GatewayCommand* ct = dynamic_cast<GatewayCommand*>(crw->getCommand());
        if (ct != nullptr && ct->getGatewayId() == gatewayId && ct->getIsExit())
        {
            exitCount++;
            lastExitItem = container->child(i);
        }
    }

    // Generate next label: "Exit A", "Exit B", "Exit C", etc.
    QChar letter = QChar('A' + exitCount);
    QString exitLabel = QString("Exit %1").arg(letter);

    // Create exit widget using the same model label as the entrance.
    // Find the entrance to get its label.
    QString entranceLabel;
    for (int i = 0; i < container->childCount(); i++)
    {
        QWidget* cw = this->treeWidgetRundown->itemWidget(container->child(i), 0);
        AbstractRundownWidget* crw = dynamic_cast<AbstractRundownWidget*>(cw);
        if (crw == nullptr) continue;
        GatewayCommand* ct = dynamic_cast<GatewayCommand*>(crw->getCommand());
        if (ct != nullptr && ct->getGatewayId() == gatewayId && !ct->getIsExit())
        {
            entranceLabel = crw->getLibraryModel()->getLabel();
            break;
        }
    }
    if (entranceLabel.isEmpty())
        entranceLabel = rw->getLibraryModel()->getLabel();

    LibraryModel exitModel(0, entranceLabel, "", "", gatewayType, 0, "");
    AbstractRundownWidget* exitWidget = RundownItemFactory::getInstance().createWidget(exitModel);
    if (exitWidget == nullptr)
        return;

    GatewayCommand* exitCmd = dynamic_cast<GatewayCommand*>(exitWidget->getCommand());
    exitCmd->setGatewayId(gatewayId);
    exitCmd->setIsExit(true);
    exitCmd->setExitLabel(exitLabel);

    bool compact = this->treeWidgetRundown->getCompactView();
    exitWidget->setCompactView(compact);
    int height = compact ? Rundown::COMPACT_ITEM_HEIGHT : Rundown::DEFAULT_ITEM_HEIGHT;

    // Insert after the last exit with this gatewayId (or after entrance if no exits yet).
    int insertIdx = -1;
    if (lastExitItem != nullptr)
    {
        for (int i = 0; i < container->childCount(); i++)
        {
            if (container->child(i) == lastExitItem)
            {
                insertIdx = i + 1;
                break;
            }
        }
    }
    else
    {
        // No existing exits — find entrance position.
        for (int i = 0; i < container->childCount(); i++)
        {
            QWidget* cw = this->treeWidgetRundown->itemWidget(container->child(i), 0);
            AbstractRundownWidget* crw = dynamic_cast<AbstractRundownWidget*>(cw);
            if (crw == nullptr) continue;
            GatewayCommand* ct = dynamic_cast<GatewayCommand*>(crw->getCommand());
            if (ct != nullptr && ct->getGatewayId() == gatewayId && !ct->getIsExit())
            {
                insertIdx = i + 1;
                break;
            }
        }
    }

    QTreeWidgetItem* treeItem = new QTreeWidgetItem();
    if (insertIdx >= 0)
        container->insertChild(insertIdx, treeItem);
    else
        container->addChild(treeItem);

    if (parent != nullptr)
        exitWidget->setInGroup(true);

    this->treeWidgetRundown->setItemWidget(treeItem, 0, dynamic_cast<QWidget*>(exitWidget));
    dynamic_cast<QWidget*>(exitWidget)->setFixedHeight(height);
    wireGatewayWidget(exitWidget, treeItem);
    if (auto* apw = dynamic_cast<RundownAutoPlayGatewayWidget*>(exitWidget))
        apw->updateVisuals();
    else if (auto* ftw = dynamic_cast<RundownFocusGatewayWidget*>(exitWidget))
        ftw->updateVisuals();
    else if (auto* cgw = dynamic_cast<RundownCommandGatewayWidget*>(exitWidget))
        cgw->updateVisuals();

    this->treeWidgetRundown->setCurrentItem(treeItem);
    this->treeWidgetRundown->doItemsLayout();
    this->treeWidgetRundown->repaint();

    EventManager::getInstance().fireGatewayExitsChangedEvent(gatewayId);
}

void RundownTreeWidget::addAutoPlayGatewayExitItem()
{
    LibraryModel model(0, "Gateway Exit", "", "", Rundown::AUTOPLAYGATEWAY, 0, "");
    AbstractRundownWidget* exitWidget = RundownItemFactory::getInstance().createWidget(model);
    if (exitWidget == nullptr)
        return;

    GatewayCommand* exitCmd = dynamic_cast<GatewayCommand*>(exitWidget->getCommand());
    if (exitCmd == nullptr) return;
    exitCmd->setIsExit(true);
    exitCmd->setExitLabel("Exit A");
    exitCmd->setGatewayId(QString());

    bool compact = this->treeWidgetRundown->getCompactView();
    exitWidget->setCompactView(compact);
    int height = compact ? Rundown::COMPACT_ITEM_HEIGHT : Rundown::DEFAULT_ITEM_HEIGHT;

    QTreeWidgetItem* treeItem = new QTreeWidgetItem();
    QTreeWidgetItem* currentItem = this->treeWidgetRundown->currentItem();
    QTreeWidgetItem* parent = (currentItem != nullptr) ? currentItem->parent() : nullptr;
    QTreeWidgetItem* container = (parent != nullptr) ? parent : this->treeWidgetRundown->invisibleRootItem();

    int insertIdx = -1;
    if (currentItem != nullptr)
    {
        for (int i = 0; i < container->childCount(); i++)
        {
            if (container->child(i) == currentItem)
            {
                insertIdx = i + 1;
                break;
            }
        }
    }

    if (insertIdx >= 0)
        container->insertChild(insertIdx, treeItem);
    else
        container->addChild(treeItem);

    if (parent != nullptr)
        exitWidget->setInGroup(true);

    this->treeWidgetRundown->setItemWidget(treeItem, 0, dynamic_cast<QWidget*>(exitWidget));
    dynamic_cast<QWidget*>(exitWidget)->setFixedHeight(height);
    wireGatewayWidget(exitWidget, treeItem);
    if (auto* apw = dynamic_cast<RundownAutoPlayGatewayWidget*>(exitWidget))
        apw->updateVisuals();

    this->treeWidgetRundown->setCurrentItem(treeItem);
    this->treeWidgetRundown->doItemsLayout();
    this->treeWidgetRundown->repaint();
}

void RundownTreeWidget::addFocusGatewayExitItem()
{
    LibraryModel model(0, "Gateway Exit", "", "", Rundown::FOCUSGATEWAY, 0, "");
    AbstractRundownWidget* exitWidget = RundownItemFactory::getInstance().createWidget(model);
    if (exitWidget == nullptr)
        return;

    GatewayCommand* exitCmd = dynamic_cast<GatewayCommand*>(exitWidget->getCommand());
    if (exitCmd == nullptr) return;
    exitCmd->setIsExit(true);
    exitCmd->setExitLabel("Exit A");
    exitCmd->setGatewayId(QString());

    bool compact = this->treeWidgetRundown->getCompactView();
    exitWidget->setCompactView(compact);
    int height = compact ? Rundown::COMPACT_ITEM_HEIGHT : Rundown::DEFAULT_ITEM_HEIGHT;

    QTreeWidgetItem* treeItem = new QTreeWidgetItem();
    QTreeWidgetItem* currentItem = this->treeWidgetRundown->currentItem();
    QTreeWidgetItem* parent = (currentItem != nullptr) ? currentItem->parent() : nullptr;
    QTreeWidgetItem* container = (parent != nullptr) ? parent : this->treeWidgetRundown->invisibleRootItem();

    int insertIdx = -1;
    if (currentItem != nullptr)
    {
        for (int i = 0; i < container->childCount(); i++)
        {
            if (container->child(i) == currentItem)
            {
                insertIdx = i + 1;
                break;
            }
        }
    }

    if (insertIdx >= 0)
        container->insertChild(insertIdx, treeItem);
    else
        container->addChild(treeItem);

    if (parent != nullptr)
        exitWidget->setInGroup(true);

    this->treeWidgetRundown->setItemWidget(treeItem, 0, dynamic_cast<QWidget*>(exitWidget));
    dynamic_cast<QWidget*>(exitWidget)->setFixedHeight(height);
    wireGatewayWidget(exitWidget, treeItem);
    if (auto* fgw = dynamic_cast<RundownFocusGatewayWidget*>(exitWidget))
        fgw->updateVisuals();

    this->treeWidgetRundown->setCurrentItem(treeItem);
    this->treeWidgetRundown->doItemsLayout();
    this->treeWidgetRundown->repaint();
}

void RundownTreeWidget::addCommandGatewayExitItem()
{
    LibraryModel model(0, "Gateway Exit", "", "", Rundown::COMMANDGATEWAY, 0, "");
    AbstractRundownWidget* exitWidget = RundownItemFactory::getInstance().createWidget(model);
    if (exitWidget == nullptr)
        return;

    GatewayCommand* exitCmd = dynamic_cast<GatewayCommand*>(exitWidget->getCommand());
    if (exitCmd == nullptr) return;
    exitCmd->setIsExit(true);
    exitCmd->setExitLabel("Exit A");
    exitCmd->setGatewayId(QString());

    bool compact = this->treeWidgetRundown->getCompactView();
    exitWidget->setCompactView(compact);
    int height = compact ? Rundown::COMPACT_ITEM_HEIGHT : Rundown::DEFAULT_ITEM_HEIGHT;

    QTreeWidgetItem* treeItem = new QTreeWidgetItem();
    QTreeWidgetItem* currentItem = this->treeWidgetRundown->currentItem();
    QTreeWidgetItem* parent = (currentItem != nullptr) ? currentItem->parent() : nullptr;
    QTreeWidgetItem* container = (parent != nullptr) ? parent : this->treeWidgetRundown->invisibleRootItem();

    int insertIdx = -1;
    if (currentItem != nullptr)
    {
        for (int i = 0; i < container->childCount(); i++)
        {
            if (container->child(i) == currentItem)
            {
                insertIdx = i + 1;
                break;
            }
        }
    }

    if (insertIdx >= 0)
        container->insertChild(insertIdx, treeItem);
    else
        container->addChild(treeItem);

    if (parent != nullptr)
        exitWidget->setInGroup(true);

    this->treeWidgetRundown->setItemWidget(treeItem, 0, dynamic_cast<QWidget*>(exitWidget));
    dynamic_cast<QWidget*>(exitWidget)->setFixedHeight(height);
    wireGatewayWidget(exitWidget, treeItem);
    if (auto* cgw = dynamic_cast<RundownCommandGatewayWidget*>(exitWidget))
        cgw->updateVisuals();

    this->treeWidgetRundown->setCurrentItem(treeItem);
    this->treeWidgetRundown->doItemsLayout();
    this->treeWidgetRundown->repaint();
}

void RundownTreeWidget::addGridItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::GRID);
}

void RundownTreeWidget::addCustomCommandItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::CUSTOMCOMMAND);
}

void RundownTreeWidget::addChromaKeyItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::CHROMAKEY);
}

void RundownTreeWidget::addSolidColorItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::SOLIDCOLOR);
}

void RundownTreeWidget::addHtmlItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::HTML);
}

void RundownTreeWidget::addPerspectiveItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::PERSPECTIVE);
}

void RundownTreeWidget::addRotationItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::ROTATION);
}

void RundownTreeWidget::addAnchorItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::ANCHOR);
}

void RundownTreeWidget::addFadeToBlackItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::FADETOBLACK);
}

void RundownTreeWidget::addKeyerItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::KEYER);
}

void RundownTreeWidget::addLevelsItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::LEVELS);
}

void RundownTreeWidget::addOpacityItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::OPACITY);
}

void RundownTreeWidget::addResetItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::RESET);
}

void RundownTreeWidget::addSaturationItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::SATURATION);
}

void RundownTreeWidget::addVolumeItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::VOLUME);
}

void RundownTreeWidget::addCommitItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::COMMIT);
}

void RundownTreeWidget::addAudioItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::AUDIO);
}

void RundownTreeWidget::addImageItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::STILL);
}

void RundownTreeWidget::addTemplateItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::TEMPLATE);
}

void RundownTreeWidget::addVideoItem()
{
    EventManager::getInstance().fireAddRudnownItemEvent(Rundown::MOVIE);
}


void RundownTreeWidget::saveAsPreset()
{
    if (!copySelectedItems())
        return;

    PresetDialog* dialog = new PresetDialog(this);
    if (dialog->exec() == QDialog::Accepted)
    {
        DatabaseManager::getInstance().insertPreset(PresetModel(0, dialog->getName(), qApp->clipboard()->text()));
        EventManager::getInstance().firePresetChangedEvent(PresetChangedEvent());
    }
}

void RundownTreeWidget::removeItemFromAutoPlayQueue(const RemoveItemFromAutoPlayQueueEvent& event)
{
    // Ignore events from the other pane's tree.
    if (event.getItem()->treeWidget() != this->treeWidgetRundown)
        return;

    AbstractRundownWidget* widget = dynamic_cast<AbstractRundownWidget*>(this->treeWidgetRundown->itemWidget(event.getItem(), 0));
    for (int i = 0; i < this->autoPlayQueues.count(); i++)
    {
        AutoPlayQueueInfo& queueInfo = this->autoPlayQueues[i];
        if (queueInfo.queue->contains(widget))
        {
            if (widget == this->currentAutoPlayWidget)
            {
                setAutoPlayHighlight(this->currentAutoPlayWidget, false);
                this->currentAutoPlayWidget = NULL;
            }
            queueInfo.queue->removeOne(widget);

            if (queueInfo.queue->isEmpty())
            {
                delete queueInfo.queue;
                this->autoPlayQueues.removeAt(i);
            }

            break;
        }
    }
}

void RundownTreeWidget::clearCurrentPlayingItem(const ClearCurrentPlayingItemEvent& event)
{
    // Remove the item from all channels in the map.
    QMutableMapIterator<int, QTreeWidgetItem*> it(this->currentPlayingItems);
    while (it.hasNext())
    {
        it.next();
        if (it.value() == event.getItem())
            it.remove();
    }

    if (this->currentPlayingAutoStepItem == event.getItem())
        this->currentPlayingAutoStepItem = NULL;
}

void RundownTreeWidget::currentItemChanged(const CurrentItemChangedEvent& event)
{
    currentItemChanged(event.getCurrentItem(), event.getPreviousItem());
}

bool RundownTreeWidget::getAllowRemoteTriggering() const
{
    return this->allowRemoteRundownTriggering;
}

bool RundownTreeWidget::isLocked() const
{
    return this->treeWidgetRundown->isLocked();
}

void RundownTreeWidget::setLocked(bool locked)
{
    this->treeWidgetRundown->setLocked(locked);
}

RundownTreeBaseWidget* RundownTreeWidget::treeWidget() const
{
    return this->treeWidgetRundown;
}

QUndoStack* RundownTreeWidget::undoStack() const
{
    return this->treeWidgetRundown->undoStack();
}

void RundownTreeWidget::resetOscSubscriptions()
{
    delete this->upControlSubscription;
    this->upControlSubscription = nullptr;

    delete this->downControlSubscription;
    this->downControlSubscription = nullptr;

    delete this->playNowIfChannelControlSubscription;
    this->playNowIfChannelControlSubscription = nullptr;

    delete this->stopControlSubscription;
    this->stopControlSubscription = nullptr;

    delete this->playControlSubscription;
    this->playControlSubscription = nullptr;

    delete this->playNowControlSubscription;
    this->playNowControlSubscription = nullptr;

    delete this->loadControlSubscription;
    this->loadControlSubscription = nullptr;

    delete this->pauseControlSubscription;
    this->pauseControlSubscription = nullptr;

    delete this->nextControlSubscription;
    this->nextControlSubscription = nullptr;

    delete this->updateControlSubscription;
    this->updateControlSubscription = nullptr;

    delete this->previewControlSubscription;
    this->previewControlSubscription = nullptr;

    delete this->clearControlSubscription;
    this->clearControlSubscription = nullptr;

    delete this->clearVideolayerControlSubscription;
    this->clearVideolayerControlSubscription = nullptr;

    delete this->clearChannelControlSubscription;
    this->clearChannelControlSubscription = nullptr;
}

void RundownTreeWidget::configureOscSubscriptions()
{
    resetOscSubscriptions();

    QFileInfo path(this->activeRundown);

    QString upControlFilter = Osc::RUNDOWN_CONTROL_UP_FILTER;
    this->upControlSubscription = new OscSubscription(upControlFilter, this);
    QObject::connect(this->upControlSubscription, SIGNAL(subscriptionReceived(const QString&, const QList<QVariant>&)),
                     this, SLOT(upControlSubscriptionReceived(const QString&, const QList<QVariant>&)));

    QString downControlFilter = Osc::RUNDOWN_CONTROL_DOWN_FILTER;
    this->downControlSubscription = new OscSubscription(downControlFilter, this);
    QObject::connect(this->downControlSubscription, SIGNAL(subscriptionReceived(const QString&, const QList<QVariant>&)),
                     this, SLOT(downControlSubscriptionReceived(const QString&, const QList<QVariant>&)));

    QString playNowIfChannelControlFilter = Osc::RUNDOWN_CONTROL_PLAYNOWIFCHANNEL_FILTER;
    this->playNowIfChannelControlSubscription = new OscSubscription(playNowIfChannelControlFilter, this);
    QObject::connect(this->playNowIfChannelControlSubscription, SIGNAL(subscriptionReceived(const QString&, const QList<QVariant>&)),
                     this, SLOT(playNowIfChannelControlSubscriptionReceived(const QString&, const QList<QVariant>&)));

    QString stopControlFilter = Osc::RUNDOWN_CONTROL_STOP_FILTER;
    this->stopControlSubscription = new OscSubscription(stopControlFilter, this);
    QObject::connect(this->stopControlSubscription, SIGNAL(subscriptionReceived(const QString&, const QList<QVariant>&)),
                     this, SLOT(stopControlSubscriptionReceived(const QString&, const QList<QVariant>&)));

    QString playControlFilter = Osc::RUNDOWN_CONTROL_PLAY_FILTER;
    this->playControlSubscription = new OscSubscription(playControlFilter, this);
    QObject::connect(this->playControlSubscription, SIGNAL(subscriptionReceived(const QString&, const QList<QVariant>&)),
                     this, SLOT(playControlSubscriptionReceived(const QString&, const QList<QVariant>&)));

    QString playNowControlFilter = Osc::RUNDOWN_CONTROL_PLAYNOW_FILTER;
    this->playNowControlSubscription = new OscSubscription(playNowControlFilter, this);
    QObject::connect(this->playNowControlSubscription, SIGNAL(subscriptionReceived(const QString&, const QList<QVariant>&)),
                     this, SLOT(playNowControlSubscriptionReceived(const QString&, const QList<QVariant>&)));

    QString loadControlFilter = Osc::RUNDOWN_CONTROL_LOAD_FILTER;
    this->loadControlSubscription = new OscSubscription(loadControlFilter, this);
    QObject::connect(this->loadControlSubscription, SIGNAL(subscriptionReceived(const QString&, const QList<QVariant>&)),
                     this, SLOT(loadControlSubscriptionReceived(const QString&, const QList<QVariant>&)));

    QString pauseControlFilter = Osc::RUNDOWN_CONTROL_PAUSE_FILTER;
    this->pauseControlSubscription = new OscSubscription(pauseControlFilter, this);
    QObject::connect(this->pauseControlSubscription, SIGNAL(subscriptionReceived(const QString &, const QList<QVariant> &)),
                     this, SLOT(pauseControlSubscriptionReceived(const QString &, const QList<QVariant> &)));
                     
    QString nextControlFilter = Osc::RUNDOWN_CONTROL_NEXT_FILTER;
    this->nextControlSubscription = new OscSubscription(nextControlFilter, this);
    QObject::connect(this->nextControlSubscription, SIGNAL(subscriptionReceived(const QString&, const QList<QVariant>&)),
                     this, SLOT(nextControlSubscriptionReceived(const QString&, const QList<QVariant>&)));

    QString updateControlFilter = Osc::RUNDOWN_CONTROL_UPDATE_FILTER;
    this->updateControlSubscription = new OscSubscription(updateControlFilter, this);
    QObject::connect(this->updateControlSubscription, SIGNAL(subscriptionReceived(const QString&, const QList<QVariant>&)),
                     this, SLOT(updateControlSubscriptionReceived(const QString&, const QList<QVariant>&)));

    QString invokeControlFilter = Osc::RUNDOWN_CONTROL_INVOKE_FILTER;
    this->invokeControlSubscription = new OscSubscription(invokeControlFilter, this);
    QObject::connect(this->invokeControlSubscription, SIGNAL(subscriptionReceived(const QString&, const QList<QVariant>&)),
                     this, SLOT(invokeControlSubscriptionReceived(const QString&, const QList<QVariant>&)));

    QString previewControlFilter = Osc::RUNDOWN_CONTROL_PREVIEW_FILTER;
    this->previewControlSubscription = new OscSubscription(previewControlFilter, this);
    QObject::connect(this->previewControlSubscription, SIGNAL(subscriptionReceived(const QString&, const QList<QVariant>&)),
                     this, SLOT(previewControlSubscriptionReceived(const QString&, const QList<QVariant>&)));

    QString clearControlFilter = Osc::RUNDOWN_CONTROL_CLEAR_FILTER;
    this->clearControlSubscription = new OscSubscription(clearControlFilter, this);
    QObject::connect(this->clearControlSubscription, SIGNAL(subscriptionReceived(const QString&, const QList<QVariant>&)),
                     this, SLOT(clearControlSubscriptionReceived(const QString&, const QList<QVariant>&)));

    QString clearVideolayerControlFilter = Osc::RUNDOWN_CONTROL_CLEARVIDEOLAYER_FILTER;
    this->clearVideolayerControlSubscription = new OscSubscription(clearVideolayerControlFilter, this);
    QObject::connect(this->clearVideolayerControlSubscription, SIGNAL(subscriptionReceived(const QString&, const QList<QVariant>&)),
                     this, SLOT(clearVideolayerControlSubscriptionReceived(const QString&, const QList<QVariant>&)));

    QString clearChannelControlFilter = Osc::RUNDOWN_CONTROL_CLEARCHANNELFILTER;
    this->clearChannelControlSubscription = new OscSubscription(clearChannelControlFilter, this);
    QObject::connect(this->clearChannelControlSubscription, SIGNAL(subscriptionReceived(const QString&, const QList<QVariant>&)),
                     this, SLOT(clearChannelControlSubscriptionReceived(const QString&, const QList<QVariant>&)));

}

void RundownTreeWidget::upControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    if (!this->active)
        return;

    if (this->allowRemoteRundownTriggering && arguments.count() > 0 && arguments[0].toInt() > 0)
    {
        this->treeWidgetRundown->blockSignals(true);
        this->treeWidgetRundown->selectItemAbove();
        this->treeWidgetRundown->blockSignals(false);
    }
}

void RundownTreeWidget::downControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    if (!this->active)
        return;

    if (this->allowRemoteRundownTriggering && arguments.count() > 0 && arguments[0].toInt() > 0)
    {
        this->treeWidgetRundown->blockSignals(true);
        this->treeWidgetRundown->selectItemBelow();
        this->treeWidgetRundown->blockSignals(false);
    }
}

void RundownTreeWidget::playNowIfChannelControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    if (!this->active)
        return;

    if (this->treeWidgetRundown->currentItem() == NULL)
        return;

    if (this->allowRemoteRundownTriggering && arguments.count() > 0)
    {
        QTreeWidgetItem* currentItem = this->treeWidgetRundown->currentItem();
        AbstractRundownWidget* rundownWidget = dynamic_cast<AbstractRundownWidget*>(this->treeWidgetRundown->itemWidget(currentItem, 0));

        if (rundownWidget != NULL && rundownWidget->isGroup())
        {
            for (int i = 0; i < currentItem->childCount(); i++)
            {
                QTreeWidgetItem* childItem = currentItem->child(i);
                AbstractRundownWidget* rundownChildWidget = dynamic_cast<AbstractRundownWidget*>(this->treeWidgetRundown->itemWidget(childItem, 0));

                LibraryModel* model = rundownChildWidget->getLibraryModel();
                if (model == NULL)
                    continue;

                const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(model->getDeviceName());
                if (device == NULL)
                    continue; // Only CasparCG devices.

                AbstractCommand* command = dynamic_cast<AbstractCommand*>(rundownChildWidget->getCommand());
                if (arguments[0].toInt() == command->getChannel())
                    EventManager::getInstance().fireExecuteRundownItemEvent(ExecuteRundownItemEvent(Playout::PlayoutType::PlayNow, childItem));
            }
        }
        else
        {
            LibraryModel* model = rundownWidget->getLibraryModel();
            if (model == NULL)
                return;

            const QSharedPointer<CasparDevice> device = DeviceManager::getInstance().getDeviceByName(model->getDeviceName());
            if (device == NULL)
                return; // Only CasparCG devices.

            AbstractCommand* command = dynamic_cast<AbstractCommand*>(rundownWidget->getCommand());
            if (arguments[0].toInt() == command->getChannel())
                EventManager::getInstance().fireExecuteRundownItemEvent(ExecuteRundownItemEvent(Playout::PlayoutType::PlayNow, currentItem));
        }
    }
}

void RundownTreeWidget::stopControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    if (!this->active)
        return;

    if (this->treeWidgetRundown->currentItem() == NULL)
        return;

    if (this->allowRemoteRundownTriggering && arguments.count() > 0 && arguments[0].toInt() > 0)
        EventManager::getInstance().fireExecuteRundownItemEvent(ExecuteRundownItemEvent(Playout::PlayoutType::Stop, this->treeWidgetRundown->currentItem()));
}

void RundownTreeWidget::playControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    if (!this->active)
        return;

    if (this->treeWidgetRundown->currentItem() == NULL)
        return;

    if (this->allowRemoteRundownTriggering && arguments.count() > 0 && arguments[0].toInt() > 0)
        EventManager::getInstance().fireExecuteRundownItemEvent(ExecuteRundownItemEvent(Playout::PlayoutType::Play, this->treeWidgetRundown->currentItem()));
}

void RundownTreeWidget::playNowControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    if (!this->active)
        return;

    if (this->treeWidgetRundown->currentItem() == NULL)
        return;

    if (this->allowRemoteRundownTriggering && arguments.count() > 0 && arguments[0].toInt() > 0)
        EventManager::getInstance().fireExecuteRundownItemEvent(ExecuteRundownItemEvent(Playout::PlayoutType::PlayNow, this->treeWidgetRundown->currentItem()));
}

void RundownTreeWidget::loadControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    if (!this->active)
        return;

    if (this->treeWidgetRundown->currentItem() == NULL)
        return;

    if (this->allowRemoteRundownTriggering && arguments.count() > 0 && arguments[0].toInt() > 0)
        EventManager::getInstance().fireExecuteRundownItemEvent(ExecuteRundownItemEvent(Playout::PlayoutType::Load, this->treeWidgetRundown->currentItem()));
}

void RundownTreeWidget::pauseControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    if (!this->active)
        return;

    if (this->treeWidgetRundown->currentItem() == NULL)
        return;

    if (this->allowRemoteRundownTriggering && arguments.count() > 0 && arguments[0].toInt() > 0)
        EventManager::getInstance().fireExecuteRundownItemEvent(ExecuteRundownItemEvent(Playout::PlayoutType::PauseResume, this->treeWidgetRundown->currentItem()));
}

void RundownTreeWidget::nextControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    if (!this->active)
        return;

    if (this->treeWidgetRundown->currentItem() == NULL)
        return;

    if (this->allowRemoteRundownTriggering && arguments.count() > 0 && arguments[0].toInt() > 0)
        EventManager::getInstance().fireExecuteRundownItemEvent(ExecuteRundownItemEvent(Playout::PlayoutType::Next, this->treeWidgetRundown->currentItem()));
}

void RundownTreeWidget::updateControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    if (!this->active)
        return;

    if (this->treeWidgetRundown->currentItem() == NULL)
        return;

    if (this->allowRemoteRundownTriggering && arguments.count() > 0 && arguments[0].toInt() > 0)
        EventManager::getInstance().fireExecuteRundownItemEvent(ExecuteRundownItemEvent(Playout::PlayoutType::Update, this->treeWidgetRundown->currentItem()));
}

void RundownTreeWidget::invokeControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    if (!this->active)
        return;

    if (this->treeWidgetRundown->currentItem() == NULL)
        return;

    if (this->allowRemoteRundownTriggering && arguments.count() > 0 && arguments[0].toInt() > 0)
        EventManager::getInstance().fireExecuteRundownItemEvent(ExecuteRundownItemEvent(Playout::PlayoutType::Invoke, this->treeWidgetRundown->currentItem()));
}

void RundownTreeWidget::previewControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    if (!this->active)
        return;

    if (this->treeWidgetRundown->currentItem() == NULL)
        return;

    if (this->allowRemoteRundownTriggering && arguments.count() > 0 && arguments[0].toInt() > 0)
        EventManager::getInstance().fireExecuteRundownItemEvent(ExecuteRundownItemEvent(Playout::PlayoutType::Preview, this->treeWidgetRundown->currentItem()));
}

void RundownTreeWidget::clearControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    if (!this->active)
        return;

    if (this->treeWidgetRundown->currentItem() == NULL)
        return;

    if (this->allowRemoteRundownTriggering && arguments.count() > 0 && arguments[0].toInt() > 0)
        EventManager::getInstance().fireExecuteRundownItemEvent(ExecuteRundownItemEvent(Playout::PlayoutType::Clear, this->treeWidgetRundown->currentItem()));
}

void RundownTreeWidget::clearVideolayerControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    if (!this->active)
        return;

    if (this->treeWidgetRundown->currentItem() == NULL)
        return;

    if (this->allowRemoteRundownTriggering && arguments.count() > 0 && arguments[0].toInt() > 0)
        EventManager::getInstance().fireExecuteRundownItemEvent(ExecuteRundownItemEvent(Playout::PlayoutType::ClearVideoLayer, this->treeWidgetRundown->currentItem()));
}

void RundownTreeWidget::clearChannelControlSubscriptionReceived(const QString& predicate, const QList<QVariant>& arguments)
{
    Q_UNUSED(predicate);

    if (!this->active)
        return;

    if (this->treeWidgetRundown->currentItem() == NULL)
        return;

    if (this->allowRemoteRundownTriggering && arguments.count() > 0 && arguments[0].toInt() > 0)
        EventManager::getInstance().fireExecuteRundownItemEvent(ExecuteRundownItemEvent(Playout::PlayoutType::ClearChannel, this->treeWidgetRundown->currentItem()));
}

void RundownTreeWidget::assignBank(const AssignBankEvent& event)
{
    if (!this->active)
        return;

    QTreeWidgetItem* item = this->treeWidgetRundown->currentItem();
    if (item == nullptr)
        return;

    int bankId = event.getBankId();

    // Toggle: if this item already has this bank, unassign it.
    if (TriggerBankRegistry::getInstance().getBankForItem(item) == bankId)
    {
        AbstractRundownWidget* widget = dynamic_cast<AbstractRundownWidget*>(this->treeWidgetRundown->itemWidget(item, 0));
        if (widget != nullptr && widget->getCommand() != nullptr)
            widget->getCommand()->setTriggerBank(0);

        TriggerBankRegistry::getInstance().unassign(bankId);
    }
    else
    {
        // Clear the previous occupant of this bank (if any).
        QTreeWidgetItem* prev = TriggerBankRegistry::getInstance().getItem(bankId);
        if (prev != nullptr)
        {
            AbstractRundownWidget* prevWidget = dynamic_cast<AbstractRundownWidget*>(prev->treeWidget()->itemWidget(prev, 0));
            if (prevWidget != nullptr && prevWidget->getCommand() != nullptr)
                prevWidget->getCommand()->setTriggerBank(0);
        }

        // Clear any old bank on this item.
        int oldBank = TriggerBankRegistry::getInstance().getBankForItem(item);
        if (oldBank > 0 && oldBank != bankId)
        {
            AbstractRundownWidget* widget = dynamic_cast<AbstractRundownWidget*>(this->treeWidgetRundown->itemWidget(item, 0));
            if (widget != nullptr && widget->getCommand() != nullptr)
                widget->getCommand()->setTriggerBank(0);
        }

        TriggerBankRegistry::getInstance().assign(bankId, item);

        // Set triggerBank on the command. The triggerBankChanged signal fires on the widget,
        // which creates bank-specific OscSubscriptions via configureBankOscSubscriptions().
        // This does NOT touch remoteTriggerId or allowRemoteTriggering — those remain independent.
        AbstractRundownWidget* widget = dynamic_cast<AbstractRundownWidget*>(this->treeWidgetRundown->itemWidget(item, 0));
        if (widget != nullptr && widget->getCommand() != nullptr)
            widget->getCommand()->setTriggerBank(bankId);
    }
}

void RundownTreeWidget::bankTriggered(int bankId)
{
    QTreeWidgetItem* item = TriggerBankRegistry::getInstance().getItem(bankId);
    if (item == nullptr || item->treeWidget() != this->treeWidgetRundown)
        return;

    // Get the channel for per-channel tracking.
    QWidget* itemWidget = this->treeWidgetRundown->itemWidget(item, 0);
    AbstractRundownWidget* rundownWidget = dynamic_cast<AbstractRundownWidget*>(itemWidget);
    if (rundownWidget == nullptr || rundownWidget->getCommand() == nullptr)
        return;

    int channel = rundownWidget->getCommand()->getChannel();

    // Deactivate previous playing item(s).
    bool perChannel = DatabaseManager::getInstance().getConfigurationByName("ActiveIndicatorPerChannel").getValue() != "false";

    if (perChannel)
    {
        if (this->currentPlayingItems.contains(channel) && this->currentPlayingItems[channel] != nullptr)
        {
            QWidget* w = this->treeWidgetRundown->itemWidget(this->currentPlayingItems[channel], 0);
            if (w != nullptr)
                dynamic_cast<AbstractRundownWidget*>(w)->setActive(false);
        }
    }
    else
    {
        for (auto it = this->currentPlayingItems.begin(); it != this->currentPlayingItems.end(); ++it)
        {
            if (it.value() != nullptr)
            {
                QWidget* w = this->treeWidgetRundown->itemWidget(it.value(), 0);
                if (w != nullptr)
                    dynamic_cast<AbstractRundownWidget*>(w)->setActive(false);
            }
        }
        this->currentPlayingItems.clear();
    }

    this->currentPlayingItems[channel] = item;
}

QTreeWidgetItem* RundownTreeWidget::findTopLevelTreeItem(AbstractRundownWidget* widget)
{
    QTreeWidgetItem* root = this->treeWidgetRundown->invisibleRootItem();
    for (int i = 0; i < root->childCount(); i++)
    {
        QTreeWidgetItem* item = root->child(i);
        QWidget* itemWidget = this->treeWidgetRundown->itemWidget(item, 0);
        if (dynamic_cast<AbstractRundownWidget*>(itemWidget) == widget)
            return item;
    }
    return nullptr;
}

void RundownTreeWidget::setAutoPlayHighlight(AbstractRundownWidget* widget, bool highlight)
{
    QWidget* w = dynamic_cast<QWidget*>(widget);
    if (!w) return;

    QString style = highlight ? "background-color: rgba(33, 150, 243, 32);" : "";

    QFrame* frameItem = w->findChild<QFrame*>("frameItem");
    if (frameItem) frameItem->setStyleSheet(style);

    QFrame* frameStatus = w->findChild<QFrame*>("frameStatus");
    if (frameStatus) frameStatus->setStyleSheet(style);
}

void RundownTreeWidget::autostepModeChanged(bool active)
{
    if (!active)
    {
        setAutostepHighlight(this->currentAutostepHighlightItem, false);
        this->currentAutostepHighlightItem = nullptr;
    }
}

void RundownTreeWidget::setAutostepHighlight(QTreeWidgetItem* item, bool highlight)
{
    if (!item) return;

    QWidget* w = this->treeWidgetRundown->itemWidget(item, 0);
    if (!w) return;

    // Apply to the entire row widget for a solid fill, not just sub-frames.
    if (highlight)
    {
        const QString& val = ColorCache::autostepHighlight();
        QColor c(val);
        if (!c.isValid() || val.isEmpty())
            c = QColor(130, 80, 200, 80);
        w->setStyleSheet(QString("background-color: rgba(%1,%2,%3,%4);").arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha()));
    }
    else
    {
        w->setStyleSheet("");
    }
}

void RundownTreeWidget::chainToNextTopLevelItem(int queueIndex, AbstractRundownWidget* lastWidget)
{
    AutoPlayQueueInfo& queueInfo = this->autoPlayQueues[queueIndex];

    // Find the current top-level tree item's position.
    QTreeWidgetItem* topLevelItem = queueInfo.groupItem;
    if (topLevelItem == nullptr)
        topLevelItem = findTopLevelTreeItem(lastWidget);

    // Clean up the old queue.
    delete queueInfo.queue;
    this->autoPlayQueues.removeAt(queueIndex);

    if (topLevelItem == nullptr)
        return;

    QTreeWidgetItem* root = this->treeWidgetRundown->invisibleRootItem();
    int idx = root->indexOfChild(topLevelItem);
    if (idx < 0 || idx + 1 >= root->childCount())
        return;

    // Walk forward, skipping gateway items at top level.
    int nextIdx = idx + 1;
    while (nextIdx < root->childCount())
    {
        QTreeWidgetItem* candidateItem = root->child(nextIdx);
        QWidget* candidateWidget = this->treeWidgetRundown->itemWidget(candidateItem, 0);
        AbstractRundownWidget* candidateRundown = dynamic_cast<AbstractRundownWidget*>(candidateWidget);
        if (candidateRundown == nullptr)
            return;

        if (candidateRundown->getLibraryModel()->getType() != Rundown::AUTOPLAYGATEWAY)
            break; // Not a gateway, use this item.

        GatewayCommand* tCmd = dynamic_cast<GatewayCommand*>(candidateRundown->getCommand());
        if (tCmd->getIsExit())
        {
            // Exit marker: skip it.
            nextIdx++;
            continue;
        }

        // Gateway entrance: find matching exit (by effectiveExitLabel) and jump past it.
        QString selectedExit = tCmd->getEffectiveExitLabel();
        int exitIdx = -1;
        int firstExitIdx = -1;
        for (int j = nextIdx + 1; j < root->childCount(); j++)
        {
            QWidget* cw = this->treeWidgetRundown->itemWidget(root->child(j), 0);
            AbstractRundownWidget* crw = dynamic_cast<AbstractRundownWidget*>(cw);
            GatewayCommand* ct = dynamic_cast<GatewayCommand*>(crw->getCommand());
            if (ct != nullptr && ct->getIsExit() && ct->getGatewayId() == tCmd->getGatewayId())
            {
                if (firstExitIdx < 0)
                    firstExitIdx = j;
                if (ct->getExitLabel() == selectedExit)
                {
                    exitIdx = j;
                    break;
                }
            }
        }
        if (exitIdx < 0)
            exitIdx = firstExitIdx;

        if (exitIdx < 0)
        {
            // No local exit: try cross-tab gateway.
            emit requestCrossTabGateway(tCmd->getGatewayId(), selectedExit);
            return;
        }

        nextIdx = exitIdx + 1; // Jump past exit.
    }

    if (nextIdx >= root->childCount())
        return;

    QTreeWidgetItem* nextTopItem = root->child(nextIdx);
    QWidget* nextTopWidget = this->treeWidgetRundown->itemWidget(nextTopItem, 0);
    AbstractRundownWidget* nextRundown = dynamic_cast<AbstractRundownWidget*>(nextTopWidget);
    if (nextRundown == nullptr)
        return;

    // Check if the next top-level item has autoPlay.
    bool hasAutoPlay = false;
    if (nextRundown->isGroup())
    {
        if (GroupCommand* gc = dynamic_cast<GroupCommand*>(nextRundown->getCommand()))
            hasAutoPlay = gc->getAutoPlay();
    }
    else if (MovieCommand* mc = dynamic_cast<MovieCommand*>(nextRundown->getCommand()))
    {
        hasAutoPlay = mc->getAutoPlay();
    }
    else if (StillCommand* sc = dynamic_cast<StillCommand*>(nextRundown->getCommand()))
    {
        hasAutoPlay = sc->getAutoPlay() && sc->getDuration() > 0;
    }
    else if (TemplateCommand* tc = dynamic_cast<TemplateCommand*>(nextRundown->getCommand()))
    {
        hasAutoPlay = tc->getAutoPlay() && tc->getDuration() > 0;
    }

    if (!hasAutoPlay)
        return;

    // Play the next item via executeCommand, which handles group queue building,
    // top-level queue building, and AMCP commands.
    executeCommand(Playout::PlayoutType::Play, Action::ActionType::KeyPress, nextTopItem);
}

bool RundownTreeWidget::shouldPreviewRedirect() const
{
    bool toggleActive = EventManager::getInstance().getPreviewMode();
    if (toggleActive)
        return true;

    QString modifier = DatabaseManager::getInstance()
        .getConfigurationByName("PreviewModifier").getValue();
    Qt::KeyboardModifiers mods = QApplication::queryKeyboardModifiers();
    bool modifierHeld =
        (modifier == "Shift" && (mods & Qt::ShiftModifier)) ||
        (modifier == "Ctrl" && (mods & Qt::ControlModifier)) ||
        (modifier == "Alt" && (mods & Qt::AltModifier));

    return modifierHeld;
}

void RundownTreeWidget::updatePreviewChannelBadgeForSelection(bool showPreview)
{
    if (!this->active)
        return;

    QList<QTreeWidgetItem*> selected = this->treeWidgetRundown->selectedItems();
    for (QTreeWidgetItem* item : selected)
    {
        AbstractRundownWidget* widget = dynamic_cast<AbstractRundownWidget*>(this->treeWidgetRundown->itemWidget(item, 0));
        if (widget == nullptr || widget->getCommand() == nullptr || widget->getLibraryModel() == nullptr)
            continue;

        // Find this item's channel badge label.
        QLabel* labelColor = dynamic_cast<QWidget*>(widget)->findChild<QLabel*>("labelColor");
        if (labelColor == nullptr)
            continue;

        int videolayer = widget->getCommand()->getVideolayer();
        int channelToShow = widget->getCommand()->getBaseChannel();

        if (showPreview)
        {
            QString deviceName = widget->getLibraryModel()->getDeviceName();
            const QSharedPointer<DeviceModel> dm = DeviceManager::getInstance().getDeviceModelByName(deviceName);
            if (dm != nullptr && dm->getPreviewChannel() > 0)
                channelToShow = dm->getPreviewChannel();
        }

        RundownWidgetHelper::updateChannelBadge(labelColor, channelToShow, videolayer);
    }
}

RundownTreeWidget::GatewayExitLocation RundownTreeWidget::findGatewayExitInTree(const QString& gatewayId, const QString& exitLabel) const
{
    GatewayExitLocation loc;
    QTreeWidgetItem* root = this->treeWidgetRundown->invisibleRootItem();

    for (int i = 0; i < root->childCount(); i++)
    {
        QTreeWidgetItem* topItem = root->child(i);
        QWidget* topWidget = this->treeWidgetRundown->itemWidget(topItem, 0);
        AbstractRundownWidget* topRundown = dynamic_cast<AbstractRundownWidget*>(topWidget);
        if (topRundown == nullptr) continue;

        // Check top-level items.
        {
            GatewayCommand* tCmd = dynamic_cast<GatewayCommand*>(topRundown->getCommand());
            if (tCmd != nullptr && tCmd->getIsExit() && tCmd->getGatewayId() == gatewayId)
            {
                if (exitLabel.isEmpty() || tCmd->getExitLabel() == exitLabel)
                {
                    loc.exitItem = topItem;
                    loc.container = nullptr;
                    loc.childIndex = i;
                    return loc;
                }
            }
        }

        // Check children of groups.
        if (topRundown->isGroup())
        {
            for (int j = 0; j < topItem->childCount(); j++)
            {
                QWidget* childWidget = this->treeWidgetRundown->itemWidget(topItem->child(j), 0);
                AbstractRundownWidget* childRundown = dynamic_cast<AbstractRundownWidget*>(childWidget);
                if (childRundown == nullptr) continue;

                {
                    GatewayCommand* tCmd = dynamic_cast<GatewayCommand*>(childRundown->getCommand());
                    if (tCmd != nullptr && tCmd->getIsExit() && tCmd->getGatewayId() == gatewayId)
                    {
                        if (exitLabel.isEmpty() || tCmd->getExitLabel() == exitLabel)
                        {
                            loc.exitItem = topItem->child(j);
                            loc.container = topItem;
                            loc.childIndex = j;
                            return loc;
                        }
                    }
                }
            }
        }
    }

    return loc;
}

QList<RundownTreeWidget::GatewayExitLocation> RundownTreeWidget::findAllGatewayExitsInTree(const QString& gatewayId) const
{
    QList<GatewayExitLocation> exits;
    QTreeWidgetItem* root = this->treeWidgetRundown->invisibleRootItem();

    for (int i = 0; i < root->childCount(); i++)
    {
        QTreeWidgetItem* topItem = root->child(i);
        QWidget* topWidget = this->treeWidgetRundown->itemWidget(topItem, 0);
        AbstractRundownWidget* topRundown = dynamic_cast<AbstractRundownWidget*>(topWidget);
        if (topRundown == nullptr) continue;

        {
            GatewayCommand* tCmd = dynamic_cast<GatewayCommand*>(topRundown->getCommand());
            if (tCmd != nullptr && tCmd->getIsExit() && tCmd->getGatewayId() == gatewayId)
            {
                GatewayExitLocation loc;
                loc.exitItem = topItem;
                loc.container = nullptr;
                loc.childIndex = i;
                exits.append(loc);
            }
        }

        if (topRundown->isGroup())
        {
            for (int j = 0; j < topItem->childCount(); j++)
            {
                QWidget* childWidget = this->treeWidgetRundown->itemWidget(topItem->child(j), 0);
                AbstractRundownWidget* childRundown = dynamic_cast<AbstractRundownWidget*>(childWidget);
                if (childRundown == nullptr) continue;

                {
                    GatewayCommand* tCmd = dynamic_cast<GatewayCommand*>(childRundown->getCommand());
                    if (tCmd != nullptr && tCmd->getIsExit() && tCmd->getGatewayId() == gatewayId)
                    {
                        GatewayExitLocation loc;
                        loc.exitItem = topItem->child(j);
                        loc.container = topItem;
                        loc.childIndex = j;
                        exits.append(loc);
                    }
                }
            }
        }
    }

    return exits;
}

QList<QPair<QString, QString>> RundownTreeWidget::getGatewayEntrances(const QString& type) const
{
    QList<QPair<QString, QString>> result;
    QSet<QString> seen;
    QTreeWidgetItem* root = this->treeWidgetRundown->invisibleRootItem();

    auto checkItem = [&](AbstractRundownWidget* rw) {
        if (rw == nullptr || rw->getLibraryModel()->getType() != type) return;
        GatewayCommand* tCmd = dynamic_cast<GatewayCommand*>(rw->getCommand());
        if (tCmd == nullptr || tCmd->getIsExit() || tCmd->getGatewayId().isEmpty()) return;
        if (seen.contains(tCmd->getGatewayId())) return;
        seen.insert(tCmd->getGatewayId());
        result.append(qMakePair(tCmd->getGatewayId(), rw->getLibraryModel()->getLabel()));
    };

    for (int i = 0; i < root->childCount(); i++)
    {
        QTreeWidgetItem* topItem = root->child(i);
        AbstractRundownWidget* topRw = dynamic_cast<AbstractRundownWidget*>(this->treeWidgetRundown->itemWidget(topItem, 0));
        checkItem(topRw);

        if (topRw != nullptr && topRw->isGroup())
        {
            for (int j = 0; j < topItem->childCount(); j++)
            {
                AbstractRundownWidget* childRw = dynamic_cast<AbstractRundownWidget*>(this->treeWidgetRundown->itemWidget(topItem->child(j), 0));
                checkItem(childRw);
            }
        }
    }

    return result;
}

QStringList RundownTreeWidget::getGatewayExitLabels(const QString& gatewayId) const
{
    QStringList labels;
    QList<GatewayExitLocation> exits = findAllGatewayExitsInTree(gatewayId);
    for (const GatewayExitLocation& loc : exits)
    {
        QWidget* w = this->treeWidgetRundown->itemWidget(loc.exitItem, 0);
        AbstractRundownWidget* rw = dynamic_cast<AbstractRundownWidget*>(w);
        if (rw == nullptr) continue;
        GatewayCommand* cmd = dynamic_cast<GatewayCommand*>(rw->getCommand());
        if (cmd != nullptr)
            labels << cmd->getExitLabel();
    }
    labels.sort();
    return labels;
}

bool RundownTreeWidget::hasGatewayExit(const QString& gatewayId) const
{
    return findGatewayExitInTree(gatewayId).exitItem != nullptr;
}

bool RundownTreeWidget::startAutoPlayFromGatewayExit(const QString& gatewayId, const QString& exitLabel)
{
    GatewayExitLocation loc = findGatewayExitInTree(gatewayId, exitLabel);
    if (loc.exitItem == nullptr)
        return false;

    if (loc.container != nullptr)
    {
        // Exit is inside a group. Build autoplay queue from children after the exit.
        QTreeWidgetItem* groupItem = loc.container;
        int childCount = groupItem->childCount();

        // Sync autoPlay on children.
        for (int i = 0; i < childCount; i++)
        {
            QWidget* cw = this->treeWidgetRundown->itemWidget(groupItem->child(i), 0);
            AbstractRundownWidget* rw = dynamic_cast<AbstractRundownWidget*>(cw);
            if (MovieCommand* mc = dynamic_cast<MovieCommand*>(rw->getCommand()))
                mc->setAutoPlay(true);
            else if (StillCommand* sc = dynamic_cast<StillCommand*>(rw->getCommand()))
                sc->setAutoPlay(true);
        }

        QList<AbstractRundownWidget*>* autoPlayQueue = new QList<AbstractRundownWidget*>();
        bool isFirstChild = true;

        for (int j = loc.childIndex + 1; j < childCount; j++)
        {
            QWidget* cw = this->treeWidgetRundown->itemWidget(groupItem->child(j), 0);
            AbstractRundownWidget* rw = dynamic_cast<AbstractRundownWidget*>(cw);

            bool shouldQueue = false;
            bool isGateway = (rw->getLibraryModel()->getType() == Rundown::AUTOPLAYGATEWAY);
            if (MovieCommand* mc = dynamic_cast<MovieCommand*>(rw->getCommand()))
                shouldQueue = mc->getAutoPlay();
            else if (StillCommand* sc = dynamic_cast<StillCommand*>(rw->getCommand()))
                shouldQueue = sc->getAutoPlay() && sc->getDuration() > 0;
            else if (TemplateCommand* tc = dynamic_cast<TemplateCommand*>(rw->getCommand()))
                shouldQueue = tc->getAutoPlay() && tc->getDuration() > 0;

            if (shouldQueue)
            {
                if (isFirstChild)
                {
                    if (dynamic_cast<MovieCommand*>(rw->getCommand()))
                        dynamic_cast<AbstractPlayoutCommand*>(rw)->executeCommand(Playout::PlayoutType::Next);
                    else
                        dynamic_cast<AbstractPlayoutCommand*>(rw)->executeCommand(Playout::PlayoutType::Play);

                    setAutoPlayHighlight(this->currentAutoPlayWidget, false);
                    this->currentAutoPlayWidget = rw;
                    setAutoPlayHighlight(this->currentAutoPlayWidget, true);
                }
                autoPlayQueue->push_back(rw);
                isFirstChild = false;
            }
            else if (isGateway && !isFirstChild)
            {
                autoPlayQueue->push_back(rw);
            }
        }

        if (autoPlayQueue->count() > 0)
        {
            AutoPlayQueueInfo queueInfo;
            queueInfo.queue = autoPlayQueue;
            queueInfo.groupItem = groupItem;
            queueInfo.isTopLevelChain = (groupItem->parent() == nullptr);
            this->autoPlayQueues.push_back(queueInfo);
        }
        else
        {
            delete autoPlayQueue;
        }
    }
    else
    {
        // Exit is at top level. Play the item after the exit.
        QTreeWidgetItem* root = this->treeWidgetRundown->invisibleRootItem();
        if (loc.childIndex + 1 < root->childCount())
        {
            QTreeWidgetItem* nextTopItem = root->child(loc.childIndex + 1);
            executeCommand(Playout::PlayoutType::Play, Action::ActionType::KeyPress, nextTopItem);
        }
    }

    return true;
}

bool RundownTreeWidget::relayCommandToGatewayExit(const QString& gatewayId, Playout::PlayoutType type, const QString& exitLabel)
{
    GatewayExitLocation loc = findGatewayExitInTree(gatewayId, exitLabel);
    if (loc.exitItem == nullptr)
        return false;

    QTreeWidgetItem* container = (loc.container != nullptr) ? loc.container : this->treeWidgetRundown->invisibleRootItem();
    if (loc.childIndex + 1 < container->childCount())
    {
        QTreeWidgetItem* targetItem = container->child(loc.childIndex + 1);
        this->treeWidgetRundown->setCurrentItem(targetItem);
        return executeCommand(type, Action::ActionType::KeyPress, targetItem);
    }

    return false;
}

RundownTreeWidget::GatewayExitLocation RundownTreeWidget::findGatewayPartnerInTree(const QString& gatewayId, bool partnerIsExit, const QString& exitLabel) const
{
    GatewayExitLocation loc;
    QTreeWidgetItem* root = this->treeWidgetRundown->invisibleRootItem();

    auto matches = [&](GatewayCommand* tCmd) -> bool {
        if (tCmd == nullptr || tCmd->getIsExit() != partnerIsExit || tCmd->getGatewayId() != gatewayId)
            return false;
        if (!exitLabel.isEmpty() && partnerIsExit && tCmd->getExitLabel() != exitLabel)
            return false;
        return true;
    };

    for (int i = 0; i < root->childCount(); i++)
    {
        QTreeWidgetItem* topItem = root->child(i);
        QWidget* topWidget = this->treeWidgetRundown->itemWidget(topItem, 0);
        AbstractRundownWidget* topRundown = dynamic_cast<AbstractRundownWidget*>(topWidget);
        if (topRundown == nullptr) continue;

        {
            GatewayCommand* tCmd = dynamic_cast<GatewayCommand*>(topRundown->getCommand());
            if (matches(tCmd))
            {
                loc.exitItem = topItem;
                loc.container = nullptr;
                loc.childIndex = i;
                return loc;
            }
        }

        if (topRundown->isGroup())
        {
            for (int j = 0; j < topItem->childCount(); j++)
            {
                QTreeWidgetItem* childItem = topItem->child(j);
                QWidget* childWidget = this->treeWidgetRundown->itemWidget(childItem, 0);
                AbstractRundownWidget* childRundown = dynamic_cast<AbstractRundownWidget*>(childWidget);
                if (childRundown == nullptr) continue;

                {
                    GatewayCommand* tCmd = dynamic_cast<GatewayCommand*>(childRundown->getCommand());
                    if (matches(tCmd))
                    {
                        loc.exitItem = childItem;
                        loc.container = topItem;
                        loc.childIndex = j;
                        return loc;
                    }
                }

                // Also scan inside inner groups.
                if (childRundown->isGroup())
                {
                    for (int k = 0; k < childItem->childCount(); k++)
                    {
                        QWidget* gcw = this->treeWidgetRundown->itemWidget(childItem->child(k), 0);
                        AbstractRundownWidget* gcRundown = dynamic_cast<AbstractRundownWidget*>(gcw);
                        if (gcRundown == nullptr) continue;
                        GatewayCommand* tCmd = dynamic_cast<GatewayCommand*>(gcRundown->getCommand());
                        if (matches(tCmd))
                        {
                            loc.exitItem = childItem->child(k);
                            loc.container = childItem;
                            loc.childIndex = k;
                            return loc;
                        }
                    }
                }
            }
        }
    }

    return loc;
}

bool RundownTreeWidget::hasFocusGatewayPartner(const QString& gatewayId, bool fromIsExit, const QString& exitLabel) const
{
    bool partnerIsExit = !fromIsExit;
    GatewayExitLocation loc = findGatewayPartnerInTree(gatewayId, partnerIsExit, exitLabel);
    return loc.exitItem != nullptr;
}

bool RundownTreeWidget::executeFocusJump(const QString& gatewayId, bool fromIsExit, const QString& exitLabel)
{
    bool partnerIsExit = !fromIsExit;
    GatewayExitLocation loc = findGatewayPartnerInTree(gatewayId, partnerIsExit, exitLabel);
    if (loc.exitItem == nullptr)
        return false;

    this->treeWidgetRundown->setCurrentItem(loc.exitItem);
    this->treeWidgetRundown->scrollToItem(loc.exitItem, QAbstractItemView::EnsureVisible);
    this->treeWidgetRundown->setFocus();
    return true;
}

void RundownTreeWidget::wireGatewayWidget(AbstractRundownWidget* widget, QTreeWidgetItem* treeItem)
{
    if (auto* apw = dynamic_cast<RundownAutoPlayGatewayWidget*>(widget))
    {
        apw->setTreeItem(treeItem);
        QObject::connect(apw, &RundownAutoPlayGatewayWidget::requestFocusJumpToEntrance,
                         this, &RundownTreeWidget::handleFocusJumpToEntrance, Qt::UniqueConnection);
    }
    else if (auto* ftw = dynamic_cast<RundownFocusGatewayWidget*>(widget))
    {
        ftw->setTreeItem(treeItem);
        QObject::connect(ftw, &RundownFocusGatewayWidget::requestFocusJumpToEntrance,
                         this, &RundownTreeWidget::handleFocusJumpToEntrance, Qt::UniqueConnection);
    }
    else if (auto* cgw = dynamic_cast<RundownCommandGatewayWidget*>(widget))
    {
        cgw->setTreeItem(treeItem);
        QObject::connect(cgw, &RundownCommandGatewayWidget::requestFocusJumpToEntrance,
                         this, &RundownTreeWidget::handleFocusJumpToEntrance, Qt::UniqueConnection);
    }
}

void RundownTreeWidget::handleFocusJumpToEntrance(const QString& gatewayId)
{
    GatewayExitLocation loc = findGatewayPartnerInTree(gatewayId, false, QString());
    if (loc.exitItem != nullptr)
    {
        this->treeWidgetRundown->setCurrentItem(loc.exitItem);
        this->treeWidgetRundown->scrollToItem(loc.exitItem, QAbstractItemView::EnsureVisible);
    }
    else
    {
        emit requestCrossTabFocusGateway(gatewayId, true, QString());
    }
}

void RundownTreeWidget::wireAllGatewayWidgets()
{
    auto wireItem = [this](QTreeWidgetItem* item) {
        QWidget* w = this->treeWidgetRundown->itemWidget(item, 0);
        AbstractRundownWidget* rw = dynamic_cast<AbstractRundownWidget*>(w);
        if (rw != nullptr)
            wireGatewayWidget(rw, item);
    };

    for (int i = 0; i < this->treeWidgetRundown->invisibleRootItem()->childCount(); i++)
    {
        QTreeWidgetItem* item = this->treeWidgetRundown->invisibleRootItem()->child(i);
        wireItem(item);
        for (int j = 0; j < item->childCount(); j++)
            wireItem(item->child(j));
    }
}

void RundownTreeWidget::refreshUnitLabels()
{
    auto refreshItem = [this](QTreeWidgetItem* treeItem)
    {
        QWidget* widget = this->treeWidgetRundown->itemWidget(treeItem, 0);
        AbstractRundownWidget* rundownWidget = dynamic_cast<AbstractRundownWidget*>(widget);
        if (!rundownWidget)
            return;

        AbstractCommand* cmd = rundownWidget->getCommand();
        LibraryModel* model = rundownWidget->getLibraryModel();
        if (!cmd || !model)
            return;

        double fps = RundownWidgetHelper::getChannelFps(model->getDeviceName(), cmd->getChannel());

        QLabel* labelDelay = widget->findChild<QLabel*>("labelDelay");
        if (labelDelay)
            labelDelay->setText(RundownWidgetHelper::formatDelay(cmd->getDelay(), "", fps));

        QLabel* labelDuration = widget->findChild<QLabel*>("labelDuration");
        if (labelDuration)
        {
            if (model->getType() == Rundown::MOVIE)
            {
                double secs = RundownWidgetHelper::timecodeToSeconds(model->getTimecode());
                QString formatted = RundownWidgetHelper::formatSeconds(secs);
                labelDuration->setText(formatted.isEmpty() ? QString::fromUtf8("\xe2\x8f\xb1 -") : QString::fromUtf8("\xe2\x8f\xb1 %1").arg(formatted));
            }
            else
            {
                labelDuration->setText(RundownWidgetHelper::formatDuration(cmd->getDuration(), fps));
            }
        }
    };

    for (int i = 0; i < this->treeWidgetRundown->topLevelItemCount(); i++)
    {
        QTreeWidgetItem* topItem = this->treeWidgetRundown->topLevelItem(i);
        refreshItem(topItem);

        for (int j = 0; j < topItem->childCount(); j++)
            refreshItem(topItem->child(j));
    }
}


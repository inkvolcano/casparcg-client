#include "ShotboxPanelWidget.h"

#include "PanelHelper.h"
#include "ShotboxRules.h"
#include "DatabaseManager.h"
#include "DeviceManager.h"
#include "EventManager.h"
#include "Library/LibraryWidget.h"
#include "Rundown/AbstractRundownWidget.h"
#include "Rundown/RundownItemFactory.h"
#include "Rundown/RundownWidgetHelper.h"
#include "Commands/AbstractPlayoutCommand.h"
#include "Commands/MovieCommand.h"
#include "Events/StatusbarEvent.h"
#include "Events/Rundown/ChannelActivityEvent.h"
#include "Models/ConfigurationModel.h"
#include "Models/DeviceModel.h"
#include "Models/LibraryModel.h"

#include <sstream>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <QtCore/QMimeData>
#include <QtGui/QDrag>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDragMoveEvent>
#include <QtGui/QDropEvent>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>
#include <QtCore/QXmlStreamWriter>

namespace
{
    const char* ROW_MIME = "application/shotbox-row";
    const char* SETTING = "ShotboxItems";
    const int ROW_HEIGHT = 30;

    QString labelOf(AbstractRundownWidget* widget)
    {
        LibraryModel* model = widget->getLibraryModel();
        if (!model->getLabel().isEmpty())
            return model->getLabel();
        if (!model->getName().isEmpty())
            return model->getName();
        return model->getType();
    }

    bool isMediaType(const QString& type)
    {
        return type == Rundown::MOVIE || type == Rundown::STILL || type == Rundown::AUDIO
            || type == Rundown::IMAGESCROLLER || type == Rundown::TEMPLATE;
    }

    QString actionName(Playout::PlayoutType type)
    {
        switch (type)
        {
            case Playout::PlayoutType::Play: return "Play";
            case Playout::PlayoutType::Stop: return "Stop";
            case Playout::PlayoutType::Next: return "Next";
            default: return "Exec";
        }
    }
}

ShotboxPanelWidget::ShotboxPanelWidget(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    this->tabWidget = new QTabWidget(this);
    this->tabWidget->setObjectName("tabWidgetShotbox");
    outer->addWidget(this->tabWidget);

    this->page = new QWidget();
    this->page->setAcceptDrops(false);
    QVBoxLayout* pageLayout = new QVBoxLayout(this->page);
    pageLayout->setContentsMargins(4, 4, 4, 4);
    pageLayout->setSpacing(2);

    this->rowsLayout = new QVBoxLayout();
    this->rowsLayout->setSpacing(2);
    pageLayout->addLayout(this->rowsLayout);

    this->emptyHint = new QLabel("Drag up to 8 items here from a rundown or the Library.", this->page);
    this->emptyHint->setAlignment(Qt::AlignCenter);
    this->emptyHint->setWordWrap(true);
    this->emptyHint->setStyleSheet("color: rgba(160, 160, 160, 200); font-size: 11px; padding: 12px;");
    pageLayout->addWidget(this->emptyHint);
    pageLayout->addStretch();

    this->tabWidget->addTab(this->page, "Shotbox");

    this->holder = new QWidget(this);
    this->holder->hide();

    setupMenus();

    this->collapsed = PanelHelper::isPanelCollapsed("Shotbox");
    if (this->collapsed)
        this->expandCollapseAction->setText("Expand");

    this->setFixedHeight(Panel::DEFAULT_SHOTBOX_HEIGHT);

    setAcceptDrops(true);

    load();
    rebuildRows();
}

ShotboxPanelWidget::~ShotboxPanelWidget()
{
    // The copies are children of the holder and go with it.
}

void ShotboxPanelWidget::setupMenus()
{
    this->dropdownMenu = new QMenu(this);
    this->dropdownMenu->setObjectName("panelMenu");
    PanelHelper::addMoveActions(this->dropdownMenu, "Shotbox", this);
    this->dropdownMenu->addSeparator();
    this->dropdownMenu->addAction("Clear Shotbox...", this, [this]() { clearAll(); });
    this->dropdownMenu->addSeparator();
    this->expandCollapseAction = this->dropdownMenu->addAction("Collapse", this, SLOT(toggleExpandCollapse()));

    this->menuButton = new QToolButton(this->tabWidget);
    this->menuButton->setText(QString::fromUtf8("\xe2\x89\xa1"));
    this->menuButton->setFixedSize(22, 22);
    this->menuButton->setMenu(this->dropdownMenu);
    this->menuButton->setPopupMode(QToolButton::InstantPopup);
    this->tabWidget->setCornerWidget(this->menuButton);
}

void ShotboxPanelWidget::toggleExpandCollapse()
{
    this->collapsed = !this->collapsed;
    PanelHelper::setPanelCollapsed("Shotbox", this->collapsed);

    this->expandCollapseAction->setText(this->collapsed ? "Expand" : "Collapse");
    this->page->setVisible(!this->collapsed);

    if (this->collapsed)
        this->setFixedHeight(Panel::COMPACT_SHOTBOX_HEIGHT);
    else
        this->setFixedHeight(Panel::DEFAULT_SHOTBOX_HEIGHT);
}

// ---------------------------------------------------------------------------
// Storage: one <items> document, written the way a rundown file writes items.

void ShotboxPanelWidget::save() const
{
    QString xml;
    QXmlStreamWriter writer(&xml);
    writer.writeStartDocument();
    writer.writeStartElement("items");

    for (const Row& row : this->rows)
    {
        AbstractRundownWidget* widget = row.item;
        LibraryModel* model = widget->getLibraryModel();

        writer.writeStartElement("item");
        writer.writeTextElement("type", model->getType());
        writer.writeTextElement("devicename", model->getDeviceName());
        writer.writeTextElement("label", model->getLabel());
        writer.writeTextElement("name", model->getName());
        widget->getCommand()->writeProperties(writer);
        widget->writeProperties(writer);
        writer.writeEndElement();
    }

    writer.writeEndElement();
    writer.writeEndDocument();

    DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, SETTING, xml));
}

void ShotboxPanelWidget::load()
{
    const QString xml = DatabaseManager::getInstance().getConfigurationByName(SETTING).getValue();
    if (xml.trimmed().isEmpty())
        return;

    int offered = 0, added = 0, refused = 0;
    addFromRundownXml(xml, offered, added, refused);

    if (added != offered)
        qWarning("Shotbox: %d of %d stored items could not be rebuilt", offered - added, offered);
}

bool ShotboxPanelWidget::append(AbstractRundownWidget* widget)
{
    if (widget == nullptr)
        return false;

    if (widget->getCommand() == nullptr || ShotboxRules::room(this->rows.count()) == 0)
    {
        delete dynamic_cast<QWidget*>(widget);
        return false;
    }

    QWidget* asWidget = dynamic_cast<QWidget*>(widget);
    asWidget->setParent(this->holder);
    asWidget->hide();

    Row row;
    row.item = widget;
    this->rows.append(row);
    return true;
}

void ShotboxPanelWidget::addFromRundownXml(const QString& xml, int& offered, int& added, int& refused)
{
    std::wstringstream stream;
    stream << xml.toStdWString();

    boost::property_tree::wptree pt;
    try
    {
        boost::property_tree::read_xml(stream, pt);
    }
    catch (const std::exception& e)
    {
        qWarning("Shotbox: could not read items: %s", e.what());
        return;
    }

    if (pt.count(L"items") == 0)
        return;

    for (boost::property_tree::wptree::value_type& value : pt.get_child(L"items"))
    {
        if (value.first != L"item")
            continue;

        offered++;

        const QString type = QString::fromStdWString(value.second.get(L"type", L""));
        if (!ShotboxRules::accepts(type))
        {
            refused++;
            continue;
        }

        if (ShotboxRules::room(this->rows.count()) == 0)
            continue;

        const QString deviceName = QString::fromStdWString(value.second.get(L"devicename", L""));
        const QString label = QString::fromStdWString(value.second.get(L"label", L""));
        const QString name = QString::fromStdWString(value.second.get(L"name", L""));

        AbstractRundownWidget* widget =
            RundownItemFactory::getInstance().createWidget(LibraryModel(0, label, name, deviceName, type, 0, ""));
        if (widget == nullptr)
            continue;

        // A damaged value throws from inside readProperties, as it can in a
        // rundown file; the item is dropped rather than the client.
        try
        {
            widget->getCommand()->readProperties(value.second);
            widget->readProperties(value.second);
        }
        catch (const std::exception& e)
        {
            qWarning("Shotbox: skipped an item that could not be read: %s", e.what());
            delete dynamic_cast<QWidget*>(widget);
            continue;
        }

        if (append(widget))
            added++;
    }
}

void ShotboxPanelWidget::addFromLibraryDrag(const QString& data, int& offered, int& added, int& refused)
{
    // The Library's own drag format: entries separated by ";", fields by ",,".
    if (!(data.startsWith("<treeWidgetVideo>") || data.startsWith("<treeWidgetTool>") ||
          data.startsWith("<treeWidgetTemplate>") || data.startsWith("<treeWidgetImage>") ||
          data.startsWith("<treeWidgetAudio>")))
        return;

    for (const QString& entry : data.split(";"))
    {
        const QStringList fields = entry.split(",,");
        if (fields.count() < 8)
            continue;

        offered++;

        LibraryModel model(fields.at(2).toInt(), fields.at(3), fields.at(1), fields.at(4), fields.at(5),
                           fields.at(6).toInt(), fields.at(7));
        if (!ShotboxRules::accepts(model.getType()))
        {
            refused++;
            continue;
        }

        if (ShotboxRules::room(this->rows.count()) == 0)
            continue;

        AbstractRundownWidget* widget = RundownItemFactory::getInstance().createWidget(model);
        if (widget == nullptr)
            continue;

        // The same channel and layer a drop into a rundown would get.
        if (widget->getCommand() != nullptr)
        {
            widget->getCommand()->setChannel(LibraryWidget::dropChannel());
            widget->getCommand()->setVideolayer(LibraryWidget::dropVideolayer());
        }

        if (append(widget))
            added++;
    }
}

// ---------------------------------------------------------------------------
// Rows

void ShotboxPanelWidget::rebuildRows()
{
    while (QLayoutItem* item = this->rowsLayout->takeAt(0))
    {
        delete item->widget();
        delete item;
    }

    static const QString buttonStyle =
        "QPushButton { font-size: 11px; padding: 0px; border-radius: 3px; "
        "background-color: rgba(60, 60, 60, 200); color: rgba(220, 220, 220, 230); border: 1px solid rgba(80, 80, 80, 200); }"
        "QPushButton:hover { background-color: rgba(85, 85, 85, 220); }"
        "QPushButton:pressed { background-color: rgba(110, 110, 110, 230); }";

    for (int i = 0; i < this->rows.count(); i++)
    {
        Row& row = this->rows[i];
        AbstractCommand* command = row.item->getCommand();

        row.frame = new QFrame(this->page);
        row.frame->setObjectName("shotboxRow");
        row.frame->setFixedHeight(ROW_HEIGHT);
        row.frame->setCursor(Qt::OpenHandCursor);
        row.frame->setToolTip("Drag to reorder. Right-click to remove.");
        row.frame->installEventFilter(this);

        QHBoxLayout* layout = new QHBoxLayout(row.frame);
        layout->setContentsMargins(4, 2, 4, 2);
        layout->setSpacing(4);

        row.badge = new QLabel(RundownWidgetHelper::badgeText(command->getBaseChannel(), command->getVideolayer()), row.frame);
        row.badge->setFixedWidth(RundownWidgetHelper::BADGE_WIDTH);
        row.badge->setAlignment(Qt::AlignCenter);
        row.badge->setStyleSheet(QString("background-color: %1; color: white; border-radius: 3px; font-size: 10px; font-weight: bold;")
                                     .arg(RundownWidgetHelper::channelColor(command->getBaseChannel()).name()));
        row.badge->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(row.badge);

        row.label = new QLabel(labelOf(row.item), row.frame);
        row.label->setToolTip(QString("%1 on %2").arg(row.item->getLibraryModel()->getType(), row.item->getLibraryModel()->getDeviceName()));
        row.label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        row.label->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(row.label, 1);

        auto addButton = [&](const QString& glyph, const QString& tip, Playout::PlayoutType type) {
            QPushButton* button = new QPushButton(glyph, row.frame);
            button->setFixedSize(30, 24);
            button->setFocusPolicy(Qt::NoFocus);
            button->setToolTip(tip);
            button->setStyleSheet(buttonStyle);
            button->setCursor(Qt::ArrowCursor);
            QObject::connect(button, &QPushButton::clicked, this, [this, i, type]() { fire(i, type); });
            layout->addWidget(button);
        };

        addButton(QString::fromUtf8("\xe2\x96\xb6"), "Play", Playout::PlayoutType::Play);
        addButton(QString::fromUtf8("\xe2\x96\xa0"), "Stop", Playout::PlayoutType::Stop);
        addButton(QString::fromUtf8("\xe2\x8f\xad"), "Next - does what the item's own Next does, and nothing for an item without one",
                  Playout::PlayoutType::Next);

        this->rowsLayout->addWidget(row.frame);
        updateRowStyle(i);
    }

    this->emptyHint->setVisible(this->rows.isEmpty());
}

void ShotboxPanelWidget::updateRowStyle(int index)
{
    if (index < 0 || index >= this->rows.count() || this->rows[index].frame == nullptr)
        return;

    const Row& row = this->rows[index];
    const QString style = row.playing
        ? QString("QFrame#shotboxRow { background-color: rgba(40, 120, 60, 160); border: 1px solid rgba(76, 175, 80, 220); border-radius: 3px; }")
        : QString("QFrame#shotboxRow { background-color: rgba(45, 45, 45, 180); border: 1px solid rgba(70, 70, 70, 200); border-radius: 3px; }");

    RundownWidgetHelper::setStyleSheetIfChanged(row.frame, style);
}

int ShotboxPanelWidget::rowAt(const QPoint& pagePosition) const
{
    for (int i = 0; i < this->rows.count(); i++)
    {
        const QFrame* frame = this->rows[i].frame;
        if (frame != nullptr && pagePosition.y() < frame->geometry().bottom())
            return i;
    }

    return this->rows.count(); // Below the last row: the end.
}

void ShotboxPanelWidget::removeRow(int index)
{
    if (index < 0 || index >= this->rows.count())
        return;

    Row row = this->rows.takeAt(index);
    QWidget* widget = dynamic_cast<QWidget*>(row.item);
    if (widget != nullptr)
    {
        row.item->clearDelayedCommands();
        widget->deleteLater();
    }

    save();
    rebuildRows();
}

void ShotboxPanelWidget::moveRow(int from, int to)
{
    const int target = ShotboxRules::moveTarget(from, to, this->rows.count());
    if (target < 0)
        return;

    this->rows.move(from, target);
    save();
    rebuildRows();
}

void ShotboxPanelWidget::clearAll()
{
    if (this->rows.isEmpty())
        return;

    if (QMessageBox::question(this, "Clear Shotbox", "Remove every item from the Shotbox?\n\nThe items in your rundowns are not affected.",
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    while (!this->rows.isEmpty())
    {
        Row row = this->rows.takeLast();
        row.item->clearDelayedCommands();
        dynamic_cast<QWidget*>(row.item)->deleteLater();
    }

    save();
    rebuildRows();
}

// ---------------------------------------------------------------------------
// Firing - the same checks the rundown makes before it fires an item.

bool ShotboxPanelWidget::previewRequested()
{
    if (EventManager::getInstance().getPreviewMode())
        return true;

    const QString modifier = DatabaseManager::getInstance().getConfigurationByName("PreviewModifier").getValue();
    const Qt::KeyboardModifiers mods = QApplication::queryKeyboardModifiers();
    return (modifier == "Shift" && (mods & Qt::ShiftModifier))
        || (modifier == "Ctrl" && (mods & Qt::ControlModifier))
        || (modifier == "Alt" && (mods & Qt::AltModifier));
}

void ShotboxPanelWidget::fire(int index, Playout::PlayoutType type)
{
    if (index < 0 || index >= this->rows.count())
        return;

    AbstractRundownWidget* widget = this->rows[index].item;
    AbstractCommand* command = widget->getCommand();
    AbstractPlayoutCommand* playout = dynamic_cast<AbstractPlayoutCommand*>(widget);
    if (command == nullptr || playout == nullptr)
        return;

    const QString label = labelOf(widget);
    const QString deviceName = widget->getLibraryModel()->getDeviceName();

    if (command->getDisabled())
    {
        EventManager::getInstance().fireStatusbarEvent(StatusbarEvent(QString("%1 is disabled").arg(label), 3000));
        return;
    }

    // Preview mode sends it to the server's preview channel, as it does for a
    // rundown item; otherwise any override left from an earlier preview goes.
    const QSharedPointer<DeviceModel> deviceModel = DeviceManager::getInstance().getDeviceModelByName(deviceName);
    if (previewRequested() && deviceModel != nullptr && deviceModel->getPreviewChannel() > 0)
        command->setChannelOverride(deviceModel->getPreviewChannel());
    else
        command->clearChannelOverride();

    const int channel = command->getChannel();
    const int videolayer = command->getVideolayer();

    if (DeviceManager::getInstance().isChannelLocked(deviceName, channel))
    {
        EventManager::getInstance().fireStatusbarEvent(
            StatusbarEvent(QString("Channel %1 is locked on %2").arg(channel).arg(deviceName), 3000, true));
        return;
    }

    // A movie set to autoplay is started with PLAY rather than LOADBG AUTO, which
    // does not start on an empty layer - what the rundown does for a top-level one.
    Playout::PlayoutType executeType = type;
    if (type == Playout::PlayoutType::Play)
    {
        if (MovieCommand* movie = dynamic_cast<MovieCommand*>(command))
        {
            if (movie->getAutoPlay())
                executeType = Playout::PlayoutType::Next;
        }
    }

    playout->executeCommand(executeType);

    emit EventManager::getInstance().playoutAction(actionName(type) + " (Shotbox)", label, deviceName, channel, videolayer);

    // The Activity panel, as for a rundown item - but not for media the server
    // does not have, which would leave a row there that nothing clears.
    const QString itemType = widget->getLibraryModel()->getType();
    const bool active = type == Playout::PlayoutType::Play;
    bool known = true;
    if (active && isMediaType(itemType))
    {
        known = deviceModel != nullptr
             && !DatabaseManager::getInstance().getLibraryByNameAndDeviceId(widget->getLibraryModel()->getName(), deviceModel->getId()).isEmpty();
    }
    if (type != Playout::PlayoutType::Next && known)
        EventManager::getInstance().fireChannelActivityEvent(ChannelActivityEvent(channel, videolayer, label, itemType, active));

    if (type == Playout::PlayoutType::Play)
        this->rows[index].playing = true;
    else if (type == Playout::PlayoutType::Stop)
        this->rows[index].playing = false;

    updateRowStyle(index);
}

// ---------------------------------------------------------------------------
// Drag and drop

void ShotboxPanelWidget::dragEnterEvent(QDragEnterEvent* event)
{
    const QMimeData* mime = event->mimeData();
    if (mime->hasFormat(ROW_MIME))
    {
        event->setDropAction(Qt::MoveAction);
        event->accept();
    }
    else if (mime->hasFormat("application/rundown-item") || mime->hasFormat("application/library-item"))
    {
        event->setDropAction(Qt::CopyAction);
        event->accept();
    }
    else
    {
        event->ignore();
    }
}

void ShotboxPanelWidget::dragMoveEvent(QDragMoveEvent* event)
{
    const QMimeData* mime = event->mimeData();
    if (mime->hasFormat(ROW_MIME))
    {
        event->setDropAction(Qt::MoveAction);
        event->accept();
    }
    else if (mime->hasFormat("application/rundown-item") || mime->hasFormat("application/library-item"))
    {
        event->setDropAction(Qt::CopyAction);
        event->accept();
    }
    else
    {
        event->ignore();
    }
}

void ShotboxPanelWidget::dropEvent(QDropEvent* event)
{
    const QMimeData* mime = event->mimeData();

    if (mime->hasFormat(ROW_MIME))
    {
        // Only recorded here. The drop arrives inside the drag's own event loop,
        // started from the row frame being dragged; rebuilding the rows now would
        // delete that frame while its event is still being handled. The move is
        // made once the drag has returned.
        const QPoint onPage = this->page->mapFrom(this, event->position().toPoint());
        this->pendingMoveTo = rowAt(onPage);
        event->setDropAction(Qt::MoveAction);
        event->accept();
        return;
    }

    int offered = 0, added = 0, refused = 0;

    if (mime->hasFormat("application/rundown-item"))
    {
        const QString xml = QString::fromUtf8(mime->data("application/rundown-item"));
        if (xml.contains("<items>"))
            addFromRundownXml(xml, offered, added, refused);
    }
    else if (mime->hasFormat("application/library-item"))
    {
        addFromLibraryDrag(QString::fromUtf8(mime->data("application/library-item")), offered, added, refused);
    }

    if (added > 0)
    {
        save();
        rebuildRows();
    }

    const QString notice = ShotboxRules::dropNotice(offered, added, refused);
    if (!notice.isEmpty())
        EventManager::getInstance().fireStatusbarEvent(StatusbarEvent(notice, 4000));

    event->setDropAction(Qt::CopyAction);
    event->accept();
}

bool ShotboxPanelWidget::eventFilter(QObject* watched, QEvent* event)
{
    int index = -1;
    for (int i = 0; i < this->rows.count(); i++)
    {
        if (this->rows[i].frame == watched)
        {
            index = i;
            break;
        }
    }

    if (index < 0)
        return QWidget::eventFilter(watched, event);

    if (event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton)
        {
            this->pressPosition = mouse->position().toPoint();
            this->pressedRow = index;
        }
        else if (mouse->button() == Qt::RightButton)
        {
            QMenu menu(this);
            QAction* remove = menu.addAction(QString("Remove \"%1\"").arg(labelOf(this->rows[index].item)));
            if (menu.exec(mouse->globalPosition().toPoint()) == remove)
                QMetaObject::invokeMethod(this, [this, index]() { removeRow(index); }, Qt::QueuedConnection);
            return true;
        }
    }
    else if (event->type() == QEvent::MouseMove && this->pressedRow == index)
    {
        QMouseEvent* mouse = static_cast<QMouseEvent*>(event);
        if ((mouse->buttons() & Qt::LeftButton)
            && (mouse->position().toPoint() - this->pressPosition).manhattanLength() >= QApplication::startDragDistance())
        {
            const int from = this->pressedRow;
            this->pressedRow = -1;

            QMimeData* mime = new QMimeData();
            mime->setData(ROW_MIME, QByteArray::number(from));

            // The drag belongs to the panel, not the row frame, and the move the
            // drop asked for is queued until this handler has returned: the
            // rebuild it causes deletes the frame this event is for.
            this->pendingMoveTo = -1;
            QDrag* drag = new QDrag(this);
            drag->setMimeData(mime);
            drag->setPixmap(this->rows[from].frame->grab());
            drag->exec(Qt::MoveAction);

            const int to = this->pendingMoveTo;
            this->pendingMoveTo = -1;
            if (to >= 0)
                QMetaObject::invokeMethod(this, [this, from, to]() { moveRow(from, to); }, Qt::QueuedConnection);
            return true;
        }
    }
    else if (event->type() == QEvent::MouseButtonRelease)
    {
        this->pressedRow = -1;
    }

    return QWidget::eventFilter(watched, event);
}

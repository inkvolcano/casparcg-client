#include "ServerConfigDialog.h"

#include "DatabaseManager.h"
#include "DeviceManager.h"
#include "ServerProcessControl.h"
#include "ServerReleases.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QMap>
#include <QtCore/QPointer>
#include <QtCore/QSaveFile>
#include <QtCore/QSignalBlocker>
#include <QtCore/QTimer>
#include <QtCore/QUrl>

#include <QtGui/QColor>
#include <QtGui/QDesktopServices>
#include <QtGui/QFont>

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>

using namespace ServerConfigXml;
using namespace ServerConfigSchema;

namespace
{
    const int TARGET_ROLE = Qt::UserRole + 1;

    // What each list holds, where it lives under <configuration>, and the kind
    // of item a new one is.
    struct ListSpec
    {
        QString heading;
        QString title;
        QString container;
        QString itemKind;
        QString help;
    };

    QList<ListSpec> lists()
    {
        return QList<ListSpec>()
            << ListSpec{"heading:controllers", "AMCP ports", "controllers", "tcp",
                        "The ports clients connect to. This client uses the one set for the server in Settings (usually 5250)."}
            << ListSpec{"heading:osc-clients", "OSC clients", "osc/predefined-clients", "predefined-client",
                        "Addresses that always get the server's OSC, whether or not they are connected over AMCP."}
            << ListSpec{"heading:channels", "Channels", "channels", "channel",
                        "Channels are numbered by their order here: moving one renumbers the channels after it."}
            << ListSpec{"heading:video-modes", "Custom video modes", "video-modes", "custom-mode",
                        "Formats beyond the built-in ones, for channels and screens. DeckLink outputs cannot run them."}
            << ListSpec{"heading:template-hosts", "Flash template hosts", "template-hosts", "template-host",
                        "Which Flash template host file plays at which video mode. Only read when Flash is on, on Windows."};
    }

    ListSpec listFor(const QString& heading)
    {
        for (const ListSpec& spec : lists())
        {
            if (spec.heading == heading)
                return spec;
        }
        return ListSpec();
    }

    QString addLabel(const QString& kind)
    {
        if (kind == "channel") return "Channel";
        if (kind == "tcp") return "AMCP Port";
        if (kind == "predefined-client") return "OSC Client";
        if (kind == "custom-mode") return "Custom Video Mode";
        if (kind == "template-host") return "Flash Template Host";
        if (kind == "producer") return "Producer (plays at startup)";
        if (kind == "port") return "DeckLink Port (in sync)";
        if (kind == "fixture") return "Art-Net Fixture";
        return consumerLabel(kind);
    }

    // The settings kind of an element found in the file.
    QString kindOfItem(const NodePtr& node, const QString& listKind)
    {
        if (listKind == "consumer")
            return consumerKinds().contains(node->name) ? node->name : QString("unknown");
        if (listKind == "tcp")
            return node->name == "tcp" ? QString("tcp") : QString("unknown");
        if (listKind == "predefined-client")
            return node->name == "predefined-client" ? listKind : QString("unknown");
        if (listKind == "channel")
            return node->name == "channel" ? listKind : QString("unknown");
        if (listKind == "custom-mode")
            return node->name == "video-mode" ? listKind : QString("unknown");
        if (listKind == "template-host")
            return listKind;   // any element name is read as a template host
        return listKind;
    }

    QString fieldKindOf(const QString& kind)
    {
        return consumerKinds().contains(kind) ? consumerFieldKind(kind) : kind;
    }
}

void ServerConfigDialog::openFor(const QString& deviceName, QWidget* parent)
{
    static QPointer<ServerConfigDialog> open;

    if (open != nullptr)
    {
        open->show();
        open->raise();
        open->activateWindow();
        return;
    }

    open = new ServerConfigDialog(deviceName, parent);
    open->setAttribute(Qt::WA_DeleteOnClose);
    open->show();
}

ServerConfigDialog::ServerConfigDialog(const QString& deviceName, QWidget* parent)
    : QDialog(parent), deviceName(deviceName)
{
    setWindowTitle(QString("Server Config - %1").arg(deviceName));
    resize(1000, 760);

    const DeviceModel model = DatabaseManager::getInstance().getDeviceByName(deviceName);
    this->serverExecutable = ServerProcessControl::serverExecutableFor(model.getServerPath());

    // Which settings to offer: what the server says it is, else the folder's name.
    const QString reported = model.getVersion().trimmed();
    if (!reported.isEmpty())
        this->version = versionFromText(reported);
    else if (!this->serverExecutable.isEmpty())
        this->version = versionFromText(QFileInfo(QFileInfo(this->serverExecutable).absolutePath()).fileName());

    this->refreshTimer = new QTimer(this);
    this->refreshTimer->setSingleShot(true);
    this->refreshTimer->setInterval(250);
    QObject::connect(this->refreshTimer, &QTimer::timeout, this, [this]() { refresh(); });

    QVBoxLayout* layout = new QVBoxLayout(this);

    QHBoxLayout* top = new QHBoxLayout();
    this->labelFile = new QLabel(this);
    this->labelFile->setTextInteractionFlags(Qt::TextSelectableByMouse);
    this->labelFile->setWordWrap(true);
    top->addWidget(this->labelFile, 1);
    top->addWidget(new QLabel("Settings of server version", this));
    this->comboVersion = new QComboBox(this);
    this->comboVersion->addItem(versionText(Version::V24), int(Version::V24));
    this->comboVersion->addItem(versionText(Version::V25), int(Version::V25));
    this->comboVersion->addItem(versionText(Version::Master), int(Version::Master));
    this->comboVersion->setCurrentIndex(int(this->version));
    this->comboVersion->setToolTip(reported.isEmpty()
        ? QString("Which server version's settings are shown and checked. The server has not reported its version yet.")
        : QString("Which server version's settings are shown and checked. The server reports %1.").arg(reported));
    QObject::connect(this->comboVersion, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        this->version = Version(this->comboVersion->currentData().toInt());
        showForm();
        refresh();
    });
    top->addWidget(this->comboVersion);
    layout->addLayout(top);

    QSplitter* vertical = new QSplitter(Qt::Vertical, this);
    QSplitter* split = new QSplitter(Qt::Horizontal, vertical);

    QWidget* left = new QWidget(split);
    QVBoxLayout* leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    this->tree = new QTreeWidget(left);
    this->tree->setHeaderHidden(true);
    this->tree->setUniformRowHeights(true);
    QObject::connect(this->tree, &QTreeWidget::currentItemChanged, this, [this]() {
        showForm();
        updateButtons();
    });
    leftLayout->addWidget(this->tree, 1);

    QHBoxLayout* edit = new QHBoxLayout();
    this->buttonAdd = new QToolButton(left);
    this->buttonAdd->setText("Add");
    this->buttonAdd->setPopupMode(QToolButton::InstantPopup);
    QMenu* addMenu = new QMenu(this->buttonAdd);
    this->buttonAdd->setMenu(addMenu);
    QObject::connect(addMenu, &QMenu::aboutToShow, this, [this, addMenu]() { fillAddMenu(addMenu); });
    edit->addWidget(this->buttonAdd);
    this->buttonRemove = new QPushButton("Remove", left);
    QObject::connect(this->buttonRemove, &QPushButton::clicked, this, [this]() { removeCurrent(); });
    edit->addWidget(this->buttonRemove);
    this->buttonUp = new QPushButton("Up", left);
    QObject::connect(this->buttonUp, &QPushButton::clicked, this, [this]() { moveCurrent(-1); });
    edit->addWidget(this->buttonUp);
    this->buttonDown = new QPushButton("Down", left);
    QObject::connect(this->buttonDown, &QPushButton::clicked, this, [this]() { moveCurrent(1); });
    edit->addWidget(this->buttonDown);
    edit->addStretch(1);
    leftLayout->addLayout(edit);

    this->formArea = new QScrollArea(split);
    this->formArea->setWidgetResizable(true);
    split->setStretchFactor(0, 2);
    split->setStretchFactor(1, 3);
    split->setSizes(QList<int>() << 340 << 640);

    this->tabs = new QTabWidget(vertical);
    this->listProblems = new QListWidget(this->tabs);
    this->listProblems->setWordWrap(true);
    this->tabs->addTab(this->listProblems, "Problems");
    this->textSource = new QPlainTextEdit(this->tabs);
    this->textSource->setReadOnly(true);
    this->textSource->setLineWrapMode(QPlainTextEdit::NoWrap);
    QFont mono("Consolas");
    mono.setStyleHint(QFont::Monospace);
    this->textSource->setFont(mono);
    this->tabs->addTab(this->textSource, "File as it will be saved");
    vertical->setStretchFactor(0, 4);
    vertical->setStretchFactor(1, 1);
    layout->addWidget(vertical, 1);

    this->labelResult = new QLabel(this);
    this->labelResult->setWordWrap(true);
    this->labelResult->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(this->labelResult);

    QHBoxLayout* buttons = new QHBoxLayout();
    QPushButton* buttonOpen = new QPushButton("Open Other File...", this);
    QObject::connect(buttonOpen, &QPushButton::clicked, this, [this]() {
        if (this->dirty && QMessageBox::question(this, "Open Other File", "Discard the changes made here?",
                                                 QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
            return;
        const QString path = QFileDialog::getOpenFileName(this, "Open casparcg.config", QFileInfo(this->configPath).absolutePath(),
                                                          "CasparCG config (*.config);;All files (*)");
        if (!path.isEmpty())
            load(path);
    });
    buttons->addWidget(buttonOpen);
    QPushButton* buttonReload = new QPushButton("Reload", this);
    buttonReload->setToolTip("Read the file again, dropping changes not saved.");
    QObject::connect(buttonReload, &QPushButton::clicked, this, [this]() {
        if (this->dirty && QMessageBox::question(this, "Reload", "Discard the changes made here?",
                                                 QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
            return;
        load(this->configPath);
    });
    buttons->addWidget(buttonReload);
    QPushButton* buttonFolder = new QPushButton("Open Folder", this);
    QObject::connect(buttonFolder, &QPushButton::clicked, this, [this]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(this->configPath).absolutePath()));
    });
    buttons->addWidget(buttonFolder);
    buttons->addStretch(1);
    this->buttonSave = new QPushButton("Save", this);
    QObject::connect(this->buttonSave, &QPushButton::clicked, this, [this]() { save(); });
    buttons->addWidget(this->buttonSave);
    this->buttonRestart = new QPushButton("Save and Restart Server", this);
    this->buttonRestart->setToolTip("The server reads its config only when it starts.");
    QObject::connect(this->buttonRestart, &QPushButton::clicked, this, [this]() { saveAndRestart(); });
    buttons->addWidget(this->buttonRestart);
    QPushButton* buttonClose = new QPushButton("Close", this);
    QObject::connect(buttonClose, &QPushButton::clicked, this, &QDialog::reject);
    buttons->addWidget(buttonClose);
    layout->addLayout(buttons);

    if (this->serverExecutable.isEmpty())
    {
        load(QString());
        say(QString("No server folder is set for '%1' (Settings \xe2\x86\x92 Servers). Use Open Other File to edit a casparcg.config.").arg(deviceName), true);
    }
    else
    {
        load(QDir(QFileInfo(this->serverExecutable).absolutePath()).filePath("casparcg.config"));
    }
}

void ServerConfigDialog::reject()
{
    if (this->busy)
    {
        say("Restarting the server - this window closes once that is done.");
        return;
    }

    if (this->dirty)
    {
        const QMessageBox::StandardButton answer = QMessageBox::question(this, "Server Config",
            "Save the changes to casparcg.config?", QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
        if (answer == QMessageBox::Cancel)
            return;
        if (answer == QMessageBox::Save && !save())
            return;
    }

    QDialog::reject();
}

void ServerConfigDialog::say(const QString& text, bool problem)
{
    this->labelResult->setText(text);
    this->labelResult->setStyleSheet(problem ? "color: rgb(230, 140, 140);" : QString());
}

bool ServerConfigDialog::canRestart() const
{
    return ServerProcessControl::supported() && !this->serverExecutable.isEmpty() && !this->configPath.isEmpty()
           && ServerProcessControl::samePath(QFileInfo(this->configPath).absolutePath(), QFileInfo(this->serverExecutable).absolutePath());
}

void ServerConfigDialog::load(const QString& path)
{
    this->configPath = path;
    this->dirty = false;

    if (path.isEmpty())
    {
        this->document = ServerConfigXml::Document();
        this->labelFile->setText("No file.");
    }
    else if (!QFileInfo(path).isFile())
    {
        // A server folder with no config: start from one that works.
        this->document = parse(starterConfig());
        this->dirty = true;
        this->labelFile->setText(QDir::toNativeSeparators(path) + " (not there yet)");
        say("There is no casparcg.config here. This is one that works - a channel on screen, AMCP on 5250 - to start from. Save writes it.");
    }
    else
    {
        QFile file(path);
        QString text;
        if (file.open(QIODevice::ReadOnly))
            text = QString::fromUtf8(file.readAll());
        this->document = parse(text);
        this->labelFile->setText(QDir::toNativeSeparators(path));
        if (this->document.ok)
            say(QString());
        else
            say(QString("The file is not readable XML (%1, line %2), so it cannot be edited here. The server cannot read it either: "
                        "fix it in a text editor, or rename it and Reload to start from a working one.")
                    .arg(this->document.error).arg(this->document.errorLine), true);
    }

    rebuildTree();
    refresh();
}

QString ServerConfigDialog::titleOf(const Target& target) const
{
    if (target.kind.startsWith("heading:"))
        return listFor(target.kind).title;

    for (const Page& page : pages())
    {
        if (page.kind == target.kind)
            return page.title;
    }

    if (target.kind == "unknown")
        return QString("<%1> (not read here)").arg(target.node->name);

    return itemTitle(target.node, target.kind, target.number);
}

void ServerConfigDialog::rebuildTree(const NodePtr& select, const QString& selectKind)
{
    QSignalBlocker blocker(this->tree);

    // Kept across a rebuild: what was chosen, unless something else is asked for.
    NodePtr keepNode = select;
    QString keepKind = selectKind;
    if (keepNode == nullptr && currentTarget() >= 0)
    {
        keepNode = this->targets.at(currentTarget()).node;
        keepKind = this->targets.at(currentTarget()).kind;
    }

    this->tree->clear();
    this->targets.clear();

    const NodePtr root = this->document.ok ? this->document.root : nullptr;
    this->tree->setEnabled(root != nullptr);
    if (root == nullptr)
        return;

    QTreeWidgetItem* chosen = nullptr;
    auto add = [this, &chosen, &keepNode, &keepKind](QTreeWidgetItem* parent, const Target& target) {
        QTreeWidgetItem* item = parent == nullptr ? new QTreeWidgetItem(this->tree) : new QTreeWidgetItem(parent);
        item->setText(0, titleOf(target));
        item->setData(0, TARGET_ROLE, this->targets.count());
        this->targets.append(target);
        if (target.kind.startsWith("heading:"))
        {
            QFont font = item->font(0);
            font.setBold(true);
            item->setFont(0, font);
        }
        if (keepNode != nullptr && target.node.get() == keepNode.get() && (keepKind.isEmpty() || target.kind == keepKind))
            chosen = item;
        return item;
    };

    for (const Page& page : pages())
        add(nullptr, Target{page.kind, root, 0});

    for (const ListSpec& spec : lists())
    {
        QTreeWidgetItem* heading = add(nullptr, Target{spec.heading, root, 0});
        int number = 0;
        for (const NodePtr& element : elements(ServerConfigXml::find(root, spec.container)))
        {
            number++;
            QTreeWidgetItem* item = add(heading, Target{kindOfItem(element, spec.itemKind), element, number});
            if (spec.itemKind != "channel" || element->name != "channel")
                continue;

            for (const NodePtr& consumer : elements(child(element, "consumers")))
            {
                QTreeWidgetItem* output = add(item, Target{kindOfItem(consumer, "consumer"), consumer, 0});
                if (consumer->name == "decklink")
                {
                    for (const NodePtr& port : elements(child(consumer, "ports")))
                        add(output, Target{port->name == "port" ? QString("port") : QString("unknown"), port, 0});
                }
                else if (consumer->name == "artnet")
                {
                    for (const NodePtr& fixture : elements(child(consumer, "fixtures")))
                        add(output, Target{fixture->name == "fixture" ? QString("fixture") : QString("unknown"), fixture, 0});
                }
            }
            for (const NodePtr& producer : elements(child(element, "producers")))
                add(item, Target{producer->name == "producer" ? QString("producer") : QString("unknown"), producer, 0});
        }
    }

    this->tree->expandAll();
    this->tree->setCurrentItem(chosen != nullptr ? chosen : this->tree->topLevelItem(0));

    blocker.unblock();
    showForm();
    updateButtons();
}

int ServerConfigDialog::currentTarget() const
{
    const QTreeWidgetItem* item = this->tree->currentItem();
    if (item == nullptr)
        return -1;
    const int index = item->data(0, TARGET_ROLE).toInt();
    return index >= 0 && index < this->targets.count() ? index : -1;
}

NodePtr ServerConfigDialog::enclosing(const QString& name) const
{
    const int index = currentTarget();
    if (index < 0)
        return nullptr;

    // The chosen element or one of its ancestors, as the tree's own pointer.
    const Node* wanted = nullptr;
    for (const Node* node = this->targets.at(index).node.get(); node != nullptr; node = node->parent)
    {
        if (node->name == name && node != this->document.root.get())
        {
            wanted = node;
            break;
        }
    }
    if (wanted == nullptr)
        return nullptr;

    for (const Target& target : this->targets)
    {
        if (target.node.get() == wanted)
            return target.node;
    }
    return nullptr;
}

void ServerConfigDialog::updateButtons()
{
    const int index = currentTarget();
    const bool item = index >= 0 && !this->targets.at(index).kind.startsWith("heading:")
                      && this->targets.at(index).node.get() != this->document.root.get();
    this->buttonAdd->setEnabled(this->document.ok && !this->busy);
    this->buttonRemove->setEnabled(item && !this->busy);
    this->buttonUp->setEnabled(item && !this->busy);
    this->buttonDown->setEnabled(item && !this->busy);
    this->buttonSave->setEnabled(this->document.ok && !this->configPath.isEmpty() && !this->busy);
    this->buttonRestart->setEnabled(this->document.ok && canRestart() && !this->busy);
}

void ServerConfigDialog::fillAddMenu(QMenu* menu)
{
    menu->clear();
    if (!this->document.ok)
        return;

    const NodePtr root = this->document.root;
    auto action = [this, menu](QMenu* into, const QString& kind, const NodePtr& container) {
        QAction* a = (into != nullptr ? into : menu)->addAction(addLabel(kind));
        QObject::connect(a, &QAction::triggered, this, [this, kind, container]() {
            // After the menu has closed: the tree under it is rebuilt.
            QTimer::singleShot(0, this, [this, kind, container]() { addItem(kind, container); });
        });
    };

    const NodePtr channel = enclosing("channel");
    if (channel != nullptr)
    {
        QMenu* outputs = menu->addMenu(QString("Output on Channel %1").arg(elements(child(root, "channels")).indexOf(channel) + 1));
        for (const QString& kind : consumerKinds())
            action(outputs, kind, channel);
        action(nullptr, "producer", channel);

        const NodePtr decklink = enclosing("decklink");
        if (decklink != nullptr)
            action(nullptr, "port", decklink);
        const NodePtr artnet = enclosing("artnet");
        if (artnet != nullptr)
            action(nullptr, "fixture", artnet);
        menu->addSeparator();
    }

    action(nullptr, "channel", root);
    action(nullptr, "tcp", root);
    action(nullptr, "predefined-client", root);
    action(nullptr, "custom-mode", root);
    action(nullptr, "template-host", root);
}

void ServerConfigDialog::addItem(const QString& kind, const NodePtr& owner)
{
    const NodePtr root = this->document.root;
    NodePtr container;
    NodePtr item = newItem(kind, root, owner.get() == root.get() ? nullptr : owner);

    if (consumerKinds().contains(kind))
        container = ensure(owner, "consumers");
    else if (kind == "producer")
        container = ensure(owner, "producers");
    else if (kind == "port")
        container = ensure(owner, "ports");
    else if (kind == "fixture")
        container = ensure(owner, "fixtures");
    else
    {
        for (const ListSpec& spec : lists())
        {
            if (spec.itemKind == kind)
                container = ensure(root, spec.container);
        }
    }
    if (container == nullptr)
        return;

    appendChild(container, item);
    rebuildTree(item, consumerKinds().contains(kind) ? kind : QString());
    changed();
}

void ServerConfigDialog::removeCurrent()
{
    const int index = currentTarget();
    if (index < 0)
        return;
    const Target target = this->targets.at(index);
    if (target.kind.startsWith("heading:") || target.node.get() == this->document.root.get())
        return;

    QString question = QString("Remove %1?").arg(titleOf(target));
    if (target.kind == "channel")
        question += "\n\nThe channels after it move up one number, and anything that plays to those numbers then plays to another channel.";
    if (QMessageBox::question(this, "Remove", question, QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    // Selected after: the item before it, or the list it was in.
    NodePtr next;
    Node* parent = target.node->parent;
    QList<NodePtr> siblings;
    for (const Target& t : this->targets)
    {
        if (t.node->parent == parent && t.node.get() != target.node.get() && !t.kind.startsWith("heading:"))
            siblings.append(t.node);
    }
    if (!siblings.isEmpty())
        next = siblings.first();

    remove(target.node);
    rebuildTree(next);
    changed();
}

void ServerConfigDialog::moveCurrent(int delta)
{
    const int index = currentTarget();
    if (index < 0)
        return;
    const Target target = this->targets.at(index);
    if (ServerConfigXml::move(target.node, delta))
    {
        rebuildTree(target.node, target.kind);
        changed();
    }
}

void ServerConfigDialog::changed()
{
    this->dirty = true;
    this->refreshTimer->start();
}

void ServerConfigDialog::refresh()
{
    // Titles follow what was typed.
    for (int i = 0; i < this->tree->topLevelItemCount(); i++)
    {
        QList<QTreeWidgetItem*> pending;
        pending << this->tree->topLevelItem(i);
        while (!pending.isEmpty())
        {
            QTreeWidgetItem* item = pending.takeFirst();
            const int index = item->data(0, TARGET_ROLE).toInt();
            if (index >= 0 && index < this->targets.count())
                item->setText(0, titleOf(this->targets.at(index)));
            for (int c = 0; c < item->childCount(); c++)
                pending << item->child(c);
        }
    }

#if defined(Q_OS_WIN)
    const bool windows = true;
#else
    const bool windows = false;
#endif

    this->listProblems->clear();
    const QList<Problem> problems = this->configPath.isEmpty() && !this->document.ok ? QList<Problem>() : check(this->document, this->version, windows);
    int stops = 0;
    for (const Problem& problem : problems)
    {
        QListWidgetItem* item = new QListWidgetItem(QString("%1%2: %3")
            .arg(problem.stopsServer ? QString("Stops the server - ") : QString(), problem.where, problem.text), this->listProblems);
        item->setForeground(problem.stopsServer ? QColor(230, 120, 120) : QColor(220, 180, 100));
        if (problem.stopsServer)
            stops++;
    }
    if (problems.isEmpty() && this->document.ok)
        new QListWidgetItem("Nothing found that would stop the server or lose an output.", this->listProblems);

    this->tabs->setTabText(0, stops > 0 ? QString("Problems (%1, %2 stopping the server)").arg(problems.count()).arg(stops)
                              : problems.isEmpty() ? QString("Problems") : QString("Problems (%1)").arg(problems.count()));

    this->textSource->setPlainText(this->document.ok ? write(this->document) : QString());

    QString title = QString("Server Config - %1").arg(this->deviceName);
    if (this->dirty)
        title += " *";
    setWindowTitle(title);
    updateButtons();
}

void ServerConfigDialog::showForm()
{
    QWidget* form = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(form);

    const int index = currentTarget();
    if (index < 0 || !this->document.ok)
    {
        layout->addStretch(1);
        this->formArea->setWidget(form);
        return;
    }

    const Target target = this->targets.at(index);

    QLabel* title = new QLabel(titleOf(target), form);
    QFont font = title->font();
    font.setBold(true);
    if (font.pointSizeF() > 0)
        font.setPointSizeF(font.pointSizeF() * 1.25);
    title->setFont(font);
    layout->addWidget(title);

    auto note = [form, layout](const QString& text) {
        QLabel* label = new QLabel(text, form);
        label->setWordWrap(true);
        label->setStyleSheet("color: rgb(160, 160, 160);");
        layout->addWidget(label);
    };

    if (target.kind.startsWith("heading:"))
    {
        const ListSpec spec = listFor(target.kind);
        note(spec.help);
        QPushButton* button = new QPushButton(QString("Add %1").arg(addLabel(spec.itemKind)), form);
        const QString kind = spec.itemKind;
        const NodePtr root = this->document.root;
        QObject::connect(button, &QPushButton::clicked, this, [this, kind, root]() {
            QTimer::singleShot(0, this, [this, kind, root]() { addItem(kind, root); });
        });
        QHBoxLayout* row = new QHBoxLayout();
        row->addWidget(button);
        row->addStretch(1);
        layout->addLayout(row);
        layout->addStretch(1);
        this->formArea->setWidget(form);
        return;
    }

    if (target.kind == "unknown")
    {
        note("The server does not read this element here. It is kept in the file as it is; Remove takes it out.");
        layout->addStretch(1);
        this->formArea->setWidget(form);
        return;
    }

    if (target.kind == "channel")
        note(QString("Channel %1. Outputs and producers are added with Add, below the list.").arg(target.number));
    else if (target.kind == "ffmpeg")
        note("Records or streams the channel. A file already at the path is deleted when the server starts.");
    else if (target.kind == "bluefish")
        note("Windows servers only.");

    QMap<QString, QFormLayout*> sections;
    QFormLayout* plain = new QFormLayout();
    plain->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    layout->addLayout(plain);

    int shown = 0;
    for (const Field& field : fieldsFor(fieldKindOf(target.kind)))
    {
        bool present = false;
        value(target.node, field.path, &present);
        if (field.type == Type::Presence)
            present = ServerConfigXml::find(target.node, field.path) != nullptr;

        // A newer server's setting is shown only when the file has it already.
        if (field.since > this->version && !present)
            continue;

        QFormLayout* into = plain;
        if (!field.section.isEmpty())
        {
            if (!sections.contains(field.section))
            {
                QGroupBox* box = new QGroupBox(field.section, form);
                QFormLayout* boxLayout = new QFormLayout(box);
                boxLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
                layout->addWidget(box);
                sections.insert(field.section, boxLayout);
            }
            into = sections.value(field.section);
        }

        QString label = field.label;
        if (field.required)
            label += " *";
        if (field.since > this->version)
            label += QString(" (%1+)").arg(versionText(field.since));

        QWidget* editor = editorFor(field, target.node, form);
        QLabel* labelWidget = new QLabel(label, form);
        QString tip = field.help;
        tip += QString("%1In the file as <%2>.").arg(tip.isEmpty() ? QString() : QString("\n\n"), field.path);
        if (field.since > Version::V24)
            tip += QString(" Read from version %1.").arg(versionText(field.since));
        labelWidget->setToolTip(tip);
        editor->setToolTip(tip);

        if (field.help.isEmpty())
        {
            into->addRow(labelWidget, editor);
        }
        else
        {
            QWidget* cell = new QWidget(form);
            QVBoxLayout* cellLayout = new QVBoxLayout(cell);
            cellLayout->setContentsMargins(0, 0, 0, 0);
            cellLayout->setSpacing(1);
            cellLayout->addWidget(editor);
            QLabel* help = new QLabel(field.help, cell);
            help->setWordWrap(true);
            help->setStyleSheet("color: rgb(150, 150, 150); font-size: 11px;");
            cellLayout->addWidget(help);
            into->addRow(labelWidget, cell);
        }
        shown++;
    }

    if (shown == 0)
        note("Nothing to set here for this server version.");

    if (target.kind == "decklink")
        note(QString::fromUtf8("More DeckLink outputs in sync with this one: Add \xe2\x86\x92 DeckLink Port."));
    else if (target.kind == "artnet")
        note(QString::fromUtf8("Fixtures are added with Add \xe2\x86\x92 Art-Net Fixture."));

    layout->addStretch(1);
    this->formArea->setWidget(form);
}

QWidget* ServerConfigDialog::editorFor(const Field& field, const NodePtr& owner, QWidget* parent)
{
    bool present = false;
    const QString current = value(owner, field.path, &present);
    const QString path = field.path;
    const bool keep = field.keepGroups;

    auto apply = [this, owner, path, keep](const QString& text) {
        setValue(owner, path, text, keep);
        changed();
    };

    if (field.type == Type::Presence)
    {
        QCheckBox* box = new QCheckBox("On", parent);
        box->setChecked(ServerConfigXml::find(owner, path) != nullptr);
        QObject::connect(box, &QCheckBox::toggled, this, [this, owner, path](bool on) {
            if (on)
                ensure(owner, path);
            else if (ServerConfigXml::find(owner, path) != nullptr)
                remove(ServerConfigXml::find(owner, path));
            changed();
            QTimer::singleShot(0, this, [this]() { showForm(); });
        });
        return box;
    }

    if (field.type == Type::Bool || field.type == Type::Enum || field.type == Type::VideoMode)
    {
        QComboBox* combo = new QComboBox(parent);
        if (!field.required || field.path == ".")
            combo->addItem(QString("Default (%1)").arg(field.defaultText), QString());

        QStringList values;
        if (field.type == Type::Bool)
        {
            combo->addItem("Yes", "true");
            combo->addItem("No", "false");
        }
        else
        {
            values = field.type == Type::VideoMode ? videoModes() + customModes(this->document.root) : field.values;
            for (const QString& v : values)
                combo->addItem(v, v);
        }

        int at = present && !current.isEmpty() ? -1 : 0;
        if (present && !current.isEmpty())
        {
            QString normal = current;
            if (field.type == Type::Bool)
                normal = current == "1" ? QString("true") : current == "0" ? QString("false") : current;
            for (int i = 0; i < combo->count(); i++)
            {
                const QString data = combo->itemData(i).toString();
                const bool same = (field.type == Type::VideoMode || !field.caseSensitive)
                    ? QString::compare(data, normal, Qt::CaseInsensitive) == 0 : data == normal;
                if (!data.isEmpty() && same)
                    at = i;
            }
            if (at < 0)
            {
                // Kept as it is, and shown as what it is.
                combo->addItem(QString("%1 (not a value the server takes)").arg(current), current);
                at = combo->count() - 1;
            }
        }
        combo->setCurrentIndex(qMax(0, at));

        QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [combo, apply]() {
            apply(combo->currentData().toString());
        });
        return combo;
    }

    QLineEdit* line = new QLineEdit(current, parent);
    line->setPlaceholderText(field.required ? QString("Required") : QString("Default: %1").arg(field.defaultText));
    QObject::connect(line, &QLineEdit::textEdited, this, [apply](const QString& text) { apply(text); });

    if (field.type != Type::Path)
        return line;

    QWidget* row = new QWidget(parent);
    QHBoxLayout* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->addWidget(line, 1);
    QPushButton* browse = new QPushButton("...", row);
    browse->setFixedWidth(30);
    QObject::connect(browse, &QPushButton::clicked, this, [this, line, apply]() {
        const QString start = QDir(QFileInfo(this->configPath).absolutePath()).filePath(line->text().isEmpty() ? QString(".") : line->text());
        const QString folder = QFileDialog::getExistingDirectory(this, "Choose Folder", start);
        if (folder.isEmpty())
            return;
        QString text = QDir::fromNativeSeparators(folder);
        if (!text.endsWith('/'))
            text += '/';
        line->setText(text);
        apply(text);
    });
    rowLayout->addWidget(browse);
    return row;
}

bool ServerConfigDialog::save()
{
    if (!this->document.ok || this->configPath.isEmpty())
        return false;

#if defined(Q_OS_WIN)
    const bool windows = true;
#else
    const bool windows = false;
#endif

    int stops = 0;
    for (const Problem& problem : check(this->document, this->version, windows))
    {
        if (problem.stopsServer)
            stops++;
    }
    if (stops > 0 && QMessageBox::question(this, "Save",
            QString("%1 of the problems listed would stop the server from starting. Save anyway?").arg(stops),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return false;

    const QString folder = QFileInfo(this->configPath).absolutePath();
    QString backup;
    if (QFileInfo(this->configPath).isFile())
    {
        backup = ServerReleases::configBackupName(QDateTime::currentDateTime(), [folder](const QString& name) {
            return QFileInfo::exists(QDir(folder).filePath(name));
        });
        if (!QFile::copy(this->configPath, QDir(folder).filePath(backup)))
        {
            say("The backup could not be written in " + QDir::toNativeSeparators(folder) + ", so nothing was saved.", true);
            return false;
        }
    }

    QSaveFile file(this->configPath);
    if (!file.open(QIODevice::WriteOnly) || file.write(write(this->document).toUtf8()) < 0 || !file.commit())
    {
        say("casparcg.config could not be written: " + file.errorString(), true);
        return false;
    }

    this->dirty = false;
    this->labelFile->setText(QDir::toNativeSeparators(this->configPath));
    say(backup.isEmpty() ? QString("Saved. The server reads it when it next starts.")
                         : QString("Saved; the previous file is %1. The server reads it when it next starts.").arg(backup));
    refresh();
    return true;
}

void ServerConfigDialog::saveAndRestart()
{
    if (!save())
        return;

    using namespace ServerProcessControl;
    const QString scanner = scannerPathFor(this->serverExecutable);
    const bool running = !runningFrom(this->serverExecutable).isEmpty();

    if (QMessageBox::question(this, "Restart Server",
            running ? QString("Restart '%1' and its scanner now?\n\nEvery output of this server stops until it is back.").arg(this->deviceName)
                    : QString("'%1' is not running. Start it and its scanner now?").arg(this->deviceName),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    this->busy = true;
    updateButtons();
    say(running ? "Restarting the server..." : "Starting the server...");

    stopThenStart(this, QStringList() << this->serverExecutable << scanner, QStringList() << scanner << this->serverExecutable,
                  [this](const QString& problem) {
        this->busy = false;
        updateButtons();

        auto device = DeviceManager::getInstance().getDeviceByName(this->deviceName);
        if (device != nullptr && !device->isConnected())
            device->connectDevice();

        say(problem.isEmpty() ? QString("Saved, and the server started with it. If it does not come back, its log says why "
                                        "(the log folder of the server).")
                              : problem,
            !problem.isEmpty());
    });
}

#include "InspectorGroupWidget.h"

#include "Global.h"
#include <QtWidgets/QLineEdit>
#include <QtCore/QSignalBlocker>
#include "Models/DeviceModel.h"
#include "Library/LibraryWidget.h"
#include "DeviceManager.h"
#include "WatchFolderRules.h"
#include "WatchFolders.h"

#include "DatabaseManager.h"
#include "EventManager.h"
#include "Events/PreviewEvent.h"
#include "Events/Inspector/AutoPlayChangedEvent.h"
#include "Models/TweenModel.h"

#include <QtCore/QDebug>

#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>

InspectorGroupWidget::InspectorGroupWidget(QWidget* parent)
    : QWidget(parent),
      model(NULL), command(NULL), enableOscInputControl(true)
{
    setupUi(this);

    this->enableOscInputControl = (DatabaseManager::getInstance().getConfigurationByName("EnableOscInputControl").getValue() == "true") ? true : false;

    QObject::connect(&EventManager::getInstance(), SIGNAL(rundownItemSelected(const RundownItemSelectedEvent&)), this, SLOT(rundownItemSelected(const RundownItemSelectedEvent&)));

    QGridLayout* grid = qobject_cast<QGridLayout*>(this->layout());
    if (grid != nullptr)
    {
        QLabel* labelAutoLoop = new QLabel(tr("Auto-Loop"), this);
        this->checkBoxAutoLoop = new QCheckBox(this);
        this->checkBoxAutoLoop->setLayoutDirection(Qt::RightToLeft);
        this->spinBoxAutoLoopDelay = new QSpinBox(this);
        this->spinBoxAutoLoopDelay->setMinimum(1);
        this->spinBoxAutoLoopDelay->setMaximum(3600);
        this->spinBoxAutoLoopDelay->setSuffix(tr(" sec"));
        this->spinBoxAutoLoopDelay->setValue(Group::DEFAULT_AUTO_LOOP_DELAY);

        QHBoxLayout* row = new QHBoxLayout();
        row->setContentsMargins(0, 0, 0, 0);
        row->addWidget(this->checkBoxAutoLoop);
        row->addWidget(this->spinBoxAutoLoopDelay, 1);

        int newRow = grid->rowCount();
        grid->addWidget(labelAutoLoop, newRow, 0);
        grid->addLayout(row, newRow, 1, 1, 2);

        QObject::connect(this->checkBoxAutoLoop, SIGNAL(stateChanged(int)), this, SLOT(autoLoopChanged(int)));
        QObject::connect(this->spinBoxAutoLoopDelay, SIGNAL(valueChanged(int)), this, SLOT(autoLoopDelayChanged(int)));

        buildWatchRows(grid);
    }
}

void InspectorGroupWidget::buildWatchRows(QGridLayout* grid)
{
    int row = grid->rowCount();

    grid->addWidget(new QLabel(tr("Watch Folder"), this), row, 0);
    this->checkBoxWatch = new QCheckBox(tr("Add new files from a media folder"), this);
    this->checkBoxWatch->setToolTip(tr("New clips, stills and audio files in a folder inside the server's media become items at\n"
                                       "the end of this group. Deleted items are not put back; a file that disappears leaves\n"
                                       "its item, marked missing."));
    grid->addWidget(this->checkBoxWatch, row++, 1, 1, 2);

    grid->addWidget(new QLabel(tr("Server"), this), row, 0);
    this->comboWatchServer = new QComboBox(this);
    grid->addWidget(this->comboWatchServer, row++, 1, 1, 2);

    grid->addWidget(new QLabel(tr("Folder"), this), row, 0);
    this->comboWatchFolder = new QComboBox(this);
    this->comboWatchFolder->setEditable(true);
    this->comboWatchFolder->setToolTip(tr("A folder inside the server's media folder, e.g. INCOMING. Its subfolders count too."));
    grid->addWidget(this->comboWatchFolder, row++, 1, 1, 2);

    grid->addWidget(new QLabel(tr("New items on"), this), row, 0);
    QHBoxLayout* place = new QHBoxLayout();
    place->setContentsMargins(0, 0, 0, 0);
    this->spinWatchChannel = new QSpinBox(this);
    this->spinWatchChannel->setRange(1, 99);
    this->spinWatchChannel->setPrefix(tr("channel "));
    this->spinWatchLayer = new QSpinBox(this);
    this->spinWatchLayer->setRange(0, 9999);
    this->spinWatchLayer->setPrefix(tr("layer "));
    place->addWidget(this->spinWatchChannel, 1);
    place->addWidget(this->spinWatchLayer, 1);
    grid->addLayout(place, row++, 1, 1, 2);

    this->checkBoxWatchExisting = new QCheckBox(tr("Also add the files already there"), this);
    this->checkBoxWatchExisting->setToolTip(tr("When the watch starts or its folder changes. Off: only files that arrive from now on."));
    grid->addWidget(this->checkBoxWatchExisting, row++, 1, 1, 2);

    this->labelWatchStatus = new QLabel(this);
    this->labelWatchStatus->setWordWrap(true);
    this->labelWatchStatus->setStyleSheet("color: rgba(160, 160, 160, 220);");
    grid->addWidget(this->labelWatchStatus, row++, 1, 1, 2);

    QObject::connect(this->checkBoxWatch, &QCheckBox::toggled, this, [this]() { watchChanged(); });
    QObject::connect(this->comboWatchServer, QOverload<int>::of(&QComboBox::activated), this, [this]() {
        fillWatchFolders();
        watchChanged();
    });
    QObject::connect(this->comboWatchFolder->lineEdit(), &QLineEdit::editingFinished, this, [this]() { watchChanged(); });
    QObject::connect(this->comboWatchFolder, QOverload<int>::of(&QComboBox::activated), this, [this]() { watchChanged(); });
    QObject::connect(this->spinWatchChannel, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { watchChanged(); });
    QObject::connect(this->spinWatchLayer, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() { watchChanged(); });
}

void InspectorGroupWidget::fillWatchFolders()
{
    const QString current = this->comboWatchFolder->currentText();
    QSignalBlocker block(this->comboWatchFolder);
    this->comboWatchFolder->clear();
    this->comboWatchFolder->addItems(WatchFolderRules::foldersOf(WatchFolders::libraryNames(this->comboWatchServer->currentText())));
    this->comboWatchFolder->setEditText(current);
}

void InspectorGroupWidget::loadWatch()
{
    if (this->checkBoxWatch == nullptr || this->command == nullptr)
        return;

    QSignalBlocker b1(this->checkBoxWatch), b2(this->comboWatchServer), b3(this->comboWatchFolder),
                   b4(this->spinWatchChannel), b5(this->spinWatchLayer);

    this->comboWatchServer->clear();
    for (const DeviceModel& device : DeviceManager::getInstance().getDeviceModels())
        this->comboWatchServer->addItem(device.getName());
    if (!this->command->getWatchServer().isEmpty())
    {
        if (this->comboWatchServer->findText(this->command->getWatchServer()) < 0)
            this->comboWatchServer->addItem(this->command->getWatchServer());
        this->comboWatchServer->setCurrentText(this->command->getWatchServer());
    }
    this->comboWatchFolder->setEditText(this->command->getWatchFolder());
    fillWatchFolders();

    const bool on = this->command->getWatchEnabled();
    this->checkBoxWatch->setChecked(on);
    this->spinWatchChannel->setValue(on ? this->command->getWatchChannel() : LibraryWidget::dropChannel());
    this->spinWatchLayer->setValue(on ? this->command->getWatchLayer() : LibraryWidget::dropVideolayer());
    this->checkBoxWatchExisting->setChecked(false);
    showWatchStatus();
}

void InspectorGroupWidget::watchChanged()
{
    if (this->command == nullptr)
        return;

    const QString server = this->comboWatchServer->currentText();
    const QString folder = WatchFolderRules::normaliseFolder(this->comboWatchFolder->currentText());
    const bool on = this->checkBoxWatch->isChecked();
    const bool starting = on && (!this->command->getWatchEnabled() || server != this->command->getWatchServer()
                                 || folder != this->command->getWatchFolder());

    // Starting, or a new place to watch: what is there now counts as had,
    // unless the operator asked for it too.
    if (starting && !this->checkBoxWatchExisting->isChecked())
    {
        QStringList seen = this->command->getWatchSeen();
        for (const QString& name : WatchFolders::libraryNames(server))
        {
            if (WatchFolderRules::inFolder(name, folder) && !seen.contains(name.toUpper()))
                seen.append(name.toUpper());
        }
        this->command->setWatchSeen(seen);
    }

    this->command->setWatchServer(server);
    this->command->setWatchFolder(folder);
    this->command->setWatchChannel(this->spinWatchChannel->value());
    this->command->setWatchLayer(this->spinWatchLayer->value());
    this->command->setWatchEnabled(on);
    showWatchStatus();

    if (starting)
        WatchFolders::getInstance().poll();
}

void InspectorGroupWidget::showWatchStatus()
{
    if (this->labelWatchStatus == nullptr || this->command == nullptr)
        return;

    const bool on = this->checkBoxWatch->isChecked();
    for (QWidget* w : QList<QWidget*>{ this->comboWatchServer, this->comboWatchFolder, this->spinWatchChannel,
                                       this->spinWatchLayer, this->checkBoxWatchExisting })
        w->setEnabled(on);

    if (!on)
        this->labelWatchStatus->setText(tr("Off."));
    else if (this->command->getWatchFolder().isEmpty())
        this->labelWatchStatus->setText(tr("Choose or type a folder."));
    else
        this->labelWatchStatus->setText(tr("Looking at %1 on %2 every %3 s. %4 file(s) handled so far.")
                                            .arg(this->command->getWatchFolder(), this->command->getWatchServer())
                                            .arg(WatchFolders::intervalSeconds()).arg(this->command->getWatchSeen().count()));
}

void InspectorGroupWidget::rundownItemSelected(const RundownItemSelectedEvent& event)
{
    this->command = nullptr;
    this->model = event.getLibraryModel();

    blockAllSignals(true);

    if (dynamic_cast<GroupCommand*>(event.getCommand()))
    {
        this->command = dynamic_cast<GroupCommand*>(event.getCommand());

        this->plainTextEditNotes->setPlainText(this->command->getNotes());

        if (this->enableOscInputControl)
        {
            this->labelAutoPlay->setVisible(true);
            this->checkBoxAutoPlay->setVisible(true);
            this->checkBoxAutoPlay->setChecked(this->command->getAutoPlay());

            this->labelLoop->setVisible(true);
            this->checkBoxLoop->setVisible(true);
            this->checkBoxLoop->setChecked(this->command->getLoop());
        }
        else
        {
            this->labelAutoPlay->setVisible(false);
            this->checkBoxAutoPlay->setVisible(false);
            this->checkBoxAutoPlay->setChecked(false);

            this->labelLoop->setVisible(false);
            this->checkBoxLoop->setVisible(false);
            this->checkBoxLoop->setChecked(false);
        }

        if (this->checkBoxAutoLoop != nullptr)
            this->checkBoxAutoLoop->setChecked(this->command->getAutoLoop());
        if (this->spinBoxAutoLoopDelay != nullptr)
            this->spinBoxAutoLoopDelay->setValue(this->command->getAutoLoopDelay());

        loadWatch();
    }

    blockAllSignals(false);
}

void InspectorGroupWidget::blockAllSignals(bool block)
{
    this->plainTextEditNotes->blockSignals(block);
    this->checkBoxAutoPlay->blockSignals(block);
    this->checkBoxLoop->blockSignals(block);
    if (this->checkBoxAutoLoop != nullptr)
        this->checkBoxAutoLoop->blockSignals(block);
    if (this->spinBoxAutoLoopDelay != nullptr)
        this->spinBoxAutoLoopDelay->blockSignals(block);
}

void InspectorGroupWidget::notesChanged()
{
    this->command->setNotes(this->plainTextEditNotes->toPlainText());
}

void InspectorGroupWidget::resetNotes(QString note)
{
    Q_UNUSED(note);

    this->plainTextEditNotes->setPlainText(Group::DEFAULT_NOTE);
    this->command->setNotes(this->plainTextEditNotes->toPlainText());
}

void InspectorGroupWidget::autoPlayChanged(int state)
{
    this->command->setAutoPlay((state == Qt::Checked) ? true : false);

    EventManager::getInstance().fireAutoPlayChangedEvent(AutoPlayChangedEvent(this->checkBoxAutoPlay->isChecked()));
}

void InspectorGroupWidget::loopChanged(int state)
{
    this->command->setLoop((state == Qt::Checked) ? true : false);
}

void InspectorGroupWidget::autoLoopChanged(int state)
{
    if (this->command == nullptr)
        return;
    this->command->setAutoLoop(state == Qt::Checked);
}

void InspectorGroupWidget::autoLoopDelayChanged(int delay)
{
    if (this->command == nullptr)
        return;
    this->command->setAutoLoopDelay(delay);
}


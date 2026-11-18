#include "InspectorNdiInputWidget.h"

#include "Global.h"

#include "DatabaseManager.h"
#include "EventManager.h"
#include "Events/Inspector/LabelChangedEvent.h"
#include "Models/DeviceModel.h"
#include "NdiSourceQuery.h"

#include <QtCore/QPointer>

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QToolButton>

namespace
{
    // The label a new item gets from the Library; while the label is still this
    // (or the previous source), choosing a source names the row after it.
    const QString DEFAULT_LABEL = "NDI Input";

    QLabel* fieldLabel(const QString& text, QWidget* parent)
    {
        QLabel* label = new QLabel(text, parent);
        label->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
        return label;
    }

    QComboBox* expandingCombo(QWidget* parent)
    {
        QComboBox* combo = new QComboBox(parent);
        combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        return combo;
    }
}

InspectorNdiInputWidget::InspectorNdiInputWidget(QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    QGridLayout* grid = new QGridLayout(this);
    grid->setContentsMargins(grid->contentsMargins().left(), grid->contentsMargins().top(),
                             grid->contentsMargins().right(), 20);

    this->comboSource = expandingCombo(this);
    this->comboSource->setEditable(true);
    this->comboSource->setInsertPolicy(QComboBox::NoInsert);
    this->comboSource->lineEdit()->setPlaceholderText("MACHINE (Source)");
    this->comboSource->setToolTip("The NDI source to play, as the server lists it: MACHINE (Source).\n"
                                  "Pick one the server found, or type a name for a source that is not up yet.");
    this->buttonFind = new QToolButton(this);
    this->buttonFind->setText("Find");
    this->buttonFind->setToolTip("Ask this item's server which NDI sources it can see");
    QHBoxLayout* sourceRow = new QHBoxLayout();
    sourceRow->setSpacing(4);
    sourceRow->addWidget(this->comboSource, 1);
    sourceRow->addWidget(this->buttonFind);

    this->labelSourceStatus = new QLabel(this);
    this->labelSourceStatus->setWordWrap(true);
    this->labelSourceStatus->setStyleSheet("color: #9a9a9a;");

    this->checkLowBandwidth = new QCheckBox("Low bandwidth", this);
    this->checkLowBandwidth->setToolTip("Receive the source's low-bandwidth stream (a smaller picture), for a\n"
                                        "multiviewer or a busy network. The server's LOW_BANDWIDTH option.");

    this->comboTransition = expandingCombo(this);
    this->spinDuration = new QSpinBox(this);
    this->spinDuration->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    this->spinDuration->setMaximum(999);
    this->comboTween = expandingCombo(this);
    this->comboDirection = expandingCombo(this);

    grid->addWidget(fieldLabel("Source", this), 0, 0);
    grid->addLayout(sourceRow, 0, 1, 1, 2);
    grid->addWidget(this->labelSourceStatus, 1, 1, 1, 2);
    grid->addWidget(this->checkLowBandwidth, 2, 1, 1, 2);
    grid->addWidget(fieldLabel("Transition", this), 3, 0);
    grid->addWidget(this->comboTransition, 3, 1, 1, 2);
    grid->addWidget(fieldLabel("Duration", this), 4, 0);
    grid->addWidget(this->spinDuration, 4, 1);
    grid->addWidget(fieldLabel("frm", this), 4, 2);
    grid->addWidget(fieldLabel("Tween", this), 5, 0);
    grid->addWidget(this->comboTween, 5, 1, 1, 2);
    grid->addWidget(fieldLabel("Direction", this), 6, 0);
    grid->addWidget(this->comboDirection, 6, 1, 1, 2);

    for (const TransitionModel& transition : DatabaseManager::getInstance().getTransition())
        this->comboTransition->addItem(transition.getValue());
    for (const TweenModel& tween : DatabaseManager::getInstance().getTween())
        this->comboTween->addItem(tween.getValue());
    for (const DirectionModel& direction : DatabaseManager::getInstance().getDirection())
        this->comboDirection->addItem(direction.getValue());

    // A typed name is taken when the field is left or Enter is pressed, not on
    // every key, so the row is not renamed letter by letter.
    QObject::connect(this->comboSource->lineEdit(), &QLineEdit::editingFinished, this, [this]() {
        sourceEdited(this->comboSource->currentText());
    });
    QObject::connect(this->comboSource, QOverload<int>::of(&QComboBox::activated), this, [this](int) {
        sourceEdited(this->comboSource->currentText());
    });
    QObject::connect(this->buttonFind, &QToolButton::clicked, this, [this]() { findSources(); });
    QObject::connect(this->checkLowBandwidth, &QCheckBox::toggled, this, [this](bool on) {
        if (this->command != nullptr)
            this->command->setLowBandwidth(on);
    });
    QObject::connect(this->comboTransition, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        if (this->command != nullptr)
            this->command->setTransition(text);
    });
    QObject::connect(this->spinDuration, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        if (this->command != nullptr)
            this->command->setTransitionDuration(value);
    });
    QObject::connect(this->comboTween, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        if (this->command != nullptr)
            this->command->setTween(text);
    });
    QObject::connect(this->comboDirection, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        if (this->command != nullptr)
            this->command->setDirection(text);
    });

    QObject::connect(&EventManager::getInstance(), SIGNAL(rundownItemSelected(const RundownItemSelectedEvent&)),
                     this, SLOT(rundownItemSelected(const RundownItemSelectedEvent&)));
}

void InspectorNdiInputWidget::rundownItemSelected(const RundownItemSelectedEvent& event)
{
    this->command = nullptr;
    this->model = event.getLibraryModel();

    NdiInputCommand* ndi = dynamic_cast<NdiInputCommand*>(event.getCommand());
    if (ndi == nullptr)
        return;

    blockAllSignals(true);

    this->command = ndi;
    const QString server = this->model != nullptr ? this->model->getDeviceName() : QString();
    fillSources(this->listed.value(server));
    this->comboSource->setEditText(this->command->getSource());
    this->checkLowBandwidth->setChecked(this->command->getLowBandwidth());
    this->comboTransition->setCurrentIndex(this->comboTransition->findText(this->command->getTransition()));
    this->spinDuration->setValue(this->command->getTransitionDuration());
    this->comboTween->setCurrentIndex(this->comboTween->findText(this->command->getTween()));
    this->comboDirection->setCurrentIndex(this->comboDirection->findText(this->command->getDirection()));

    blockAllSignals(false);

    if (server.isEmpty())
        this->labelSourceStatus->setText("Choose a server under Output to list its NDI sources.");
    else if (!this->listed.contains(server))
        findSources();
    else
        this->labelSourceStatus->setText(QString("%1 source(s) on %2. Find asks again.").arg(this->listed.value(server).size()).arg(server));
}

void InspectorNdiInputWidget::blockAllSignals(bool block)
{
    this->comboSource->blockSignals(block);
    this->comboSource->lineEdit()->blockSignals(block);
    this->checkLowBandwidth->blockSignals(block);
    this->comboTransition->blockSignals(block);
    this->spinDuration->blockSignals(block);
    this->comboTween->blockSignals(block);
    this->comboDirection->blockSignals(block);
}

void InspectorNdiInputWidget::fillSources(const QStringList& sources)
{
    const QString typed = this->comboSource->currentText();
    const bool blocked = this->comboSource->blockSignals(true);
    this->comboSource->lineEdit()->blockSignals(true);
    this->comboSource->clear();
    this->comboSource->addItems(sources);
    this->comboSource->setEditText(typed);
    this->comboSource->lineEdit()->blockSignals(blocked);
    this->comboSource->blockSignals(blocked);
}

void InspectorNdiInputWidget::findSources()
{
    if (this->model == nullptr || this->asking)
        return;

    const QString server = this->model->getDeviceName();
    if (server.isEmpty())
    {
        this->labelSourceStatus->setText("Choose a server under Output to list its NDI sources.");
        return;
    }

    const DeviceModel device = DatabaseManager::getInstance().getDeviceByName(server);
    if (device.getAddress().isEmpty())
    {
        this->labelSourceStatus->setText(QString("%1 is not in Settings > Servers.").arg(server));
        return;
    }

    this->asking = true;
    this->buttonFind->setEnabled(false);
    this->labelSourceStatus->setText(QString("Asking %1 for its NDI sources...").arg(server));

    NdiSourceQuery* query = NdiSourceQuery::ask(device.getAddress(), device.getPort(), this);
    QObject::connect(query, &NdiSourceQuery::finished, this, [this, server](const QStringList& sources, const QString& problem) {
        this->asking = false;
        this->buttonFind->setEnabled(true);

        if (!problem.isEmpty())
        {
            this->labelSourceStatus->setText(problem);
            return;
        }

        this->listed.insert(server, sources);
        if (this->model != nullptr && this->model->getDeviceName() == server)
            fillSources(sources);

        this->labelSourceStatus->setText(sources.isEmpty()
            ? QString("%1 sees no NDI sources. A name can still be typed.").arg(server)
            : QString("%1 source(s) on %2.").arg(sources.size()).arg(server));
    });
}

void InspectorNdiInputWidget::sourceEdited(const QString& text)
{
    if (this->command == nullptr)
        return;

    const QString source = text.trimmed();
    const QString previous = this->command->getSource();
    if (source == previous)
        return;

    this->command->setSource(source);

    // Name the row after its source while nobody has given it a name of its own.
    if (this->model != nullptr && !source.isEmpty())
    {
        const QString label = this->model->getLabel();
        if (label == DEFAULT_LABEL || label.isEmpty() || (!previous.isEmpty() && label == previous))
            EventManager::getInstance().fireLabelChangedEvent(LabelChangedEvent(source));
    }
}

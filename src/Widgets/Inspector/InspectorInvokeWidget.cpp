#include "InspectorInvokeWidget.h"

#include "EventManager.h"
#include "Events/Rundown/ExecutePlayoutCommandEvent.h"
#include "Playout.h"

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QToolButton>

InspectorInvokeWidget::InspectorInvokeWidget(QWidget* parent)
    : QWidget(parent),
      command(nullptr), lock(false)
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(9, 6, 9, 6);
    mainLayout->setSpacing(0);

    mainLayout->addSpacing(16);

    // Header: [+] and [-] buttons, right-aligned.
    QHBoxLayout* buttonsLayout = new QHBoxLayout();
    buttonsLayout->setSpacing(2);
    buttonsLayout->addStretch();

    QToolButton* addButton = new QToolButton(this);
    addButton->setFixedSize(40, 20);
    addButton->setToolTip("Add invoke row");
    addButton->setStyleSheet(
        "QToolButton { background-color: rgba(50, 50, 50, 200);"
        " border-radius: 3px; border: 1px solid rgba(70, 70, 70, 200); padding: 0px;"
        " image: url(:/Graphics/Images/Add.png); }"
        " QToolButton:hover { background-color: rgba(70, 70, 70, 200);"
        " image: url(:/Graphics/Images/AddHover.png); }");
    QObject::connect(addButton, &QToolButton::clicked, this, [this]() { addInvokeRow(); });
    buttonsLayout->addWidget(addButton);

    QToolButton* removeButton = new QToolButton(this);
    removeButton->setFixedSize(40, 20);
    removeButton->setToolTip("Remove last invoke row");
    removeButton->setStyleSheet(
        "QToolButton { background-color: rgba(50, 50, 50, 200);"
        " border-radius: 3px; border: 1px solid rgba(70, 70, 70, 200); padding: 0px;"
        " image: url(:/Graphics/Images/Remove.png); }"
        " QToolButton:hover { background-color: rgba(70, 70, 70, 200);"
        " image: url(:/Graphics/Images/RemoveHover.png); }");
    QObject::connect(removeButton, &QToolButton::clicked, this, &InspectorInvokeWidget::removeInvokeRow);
    buttonsLayout->addWidget(removeButton);

    mainLayout->addLayout(buttonsLayout);
    mainLayout->addSpacing(5);

    // Column header row.
    this->invokeHeaderWidget = new QWidget(this);
    this->invokeHeaderWidget->setAttribute(Qt::WA_TranslucentBackground);
    QHBoxLayout* headerLayout = new QHBoxLayout(this->invokeHeaderWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(0);

    static const char* headerStyleFirst =
        "background-color: rgba(35, 35, 35, 255);"
        " border-color: rgba(65, 65, 65, 255);"
        " border-width: 1px;"
        " border-style: solid; min-height: 14px; padding: 4px;";

    static const char* headerStyle =
        "background-color: rgba(35, 35, 35, 255);"
        " border-color: rgba(65, 65, 65, 255);"
        " border-top-width: 1px; border-right-width: 1px;"
        " border-bottom-width: 1px; border-left-width: 0px;"
        " border-style: solid; min-height: 14px; padding: 4px;";

    QLabel* functionHeader = new QLabel("Function", this->invokeHeaderWidget);
    functionHeader->setStyleSheet(headerStyleFirst);
    headerLayout->addWidget(functionHeader, 1);

    QLabel* callHeader = new QLabel("Call", this->invokeHeaderWidget);
    callHeader->setFixedWidth(37);
    callHeader->setAlignment(Qt::AlignCenter);
    callHeader->setStyleSheet(headerStyle);
    headerLayout->addWidget(callHeader, 0);

    QLabel* keyHeader = new QLabel("Key", this->invokeHeaderWidget);
    keyHeader->setFixedWidth(37);
    keyHeader->setAlignment(Qt::AlignCenter);
    keyHeader->setStyleSheet(headerStyle);
    headerLayout->addWidget(keyHeader, 0);

    this->invokeHeaderWidget->setVisible(false);
    mainLayout->addWidget(this->invokeHeaderWidget);

    // Rows container.
    this->invokeRowsLayout = new QVBoxLayout();
    this->invokeRowsLayout->setContentsMargins(0, 0, 0, 0);
    this->invokeRowsLayout->setSpacing(2);
    mainLayout->addLayout(this->invokeRowsLayout);

    // Radio button group.
    this->invokeRadioGroup = new QButtonGroup(this);
    QObject::connect(this->invokeRadioGroup, &QButtonGroup::idClicked,
                     this, &InspectorInvokeWidget::invokeRadioChanged);

    QObject::connect(&EventManager::getInstance(), SIGNAL(rundownItemSelected(const RundownItemSelectedEvent&)), this, SLOT(rundownItemSelected(const RundownItemSelectedEvent&)));
    QObject::connect(&EventManager::getInstance(), SIGNAL(repositoryRundown(const RepositoryRundownEvent&)), this, SLOT(repositoryRundown(const RepositoryRundownEvent&)));
}

void InspectorInvokeWidget::repositoryRundown(const RepositoryRundownEvent& event)
{
    this->lock = event.getRepositoryRundown();
}

void InspectorInvokeWidget::rundownItemSelected(const RundownItemSelectedEvent& event)
{
    this->command = nullptr;

    if (dynamic_cast<TemplateCommand*>(event.getCommand()))
    {
        this->command = dynamic_cast<TemplateCommand*>(event.getCommand());

        clearInvokeRows();
        const QStringList& invokes = this->command->getInvokes();
        for (const QString& inv : invokes)
            addInvokeRow(inv);
        if (this->invokeRadios.size() > 0)
        {
            int hotkeyIdx = qBound(0, this->command->getInvokeHotkeyIndex(), this->invokeRadios.size() - 1);
            this->invokeRadios[hotkeyIdx]->setChecked(true);
        }
    }
}

void InspectorInvokeWidget::clearInvokeRows()
{
    for (QPushButton* radio : this->invokeRadios)
        this->invokeRadioGroup->removeButton(radio);

    while (this->invokeRowsLayout->count() > 0)
    {
        QLayoutItem* item = this->invokeRowsLayout->takeAt(0);
        if (item->widget())
            delete item->widget();
        delete item;
    }

    this->invokeLineEdits.clear();
    this->invokePlayButtons.clear();
    this->invokeRadios.clear();

    this->invokeHeaderWidget->setVisible(false);

    emit contentChanged();
}

void InspectorInvokeWidget::addInvokeRow(const QString& text)
{
    static const char* radioBtnStyle =
        "QPushButton { background-color: rgba(50, 50, 50, 200); color: rgba(100, 100, 100, 200);"
        " border-radius: 3px; font-size: 11px; font-weight: bold;"
        " border: 1px solid rgba(70, 70, 70, 200); padding: 0px;"
        " min-width: 33px; max-width: 33px; max-height: 25px; }"
        " QPushButton:hover { background-color: rgba(70, 70, 70, 200); }"
        " QPushButton:checked { background-color: rgba(100, 100, 100, 200);"
        " color: white; border: 1px solid rgba(130, 130, 130, 200); }";

    QWidget* rowWidget = new QWidget();
    rowWidget->setAttribute(Qt::WA_TranslucentBackground);
    QHBoxLayout* rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(2);

    QLineEdit* lineEdit = new QLineEdit(rowWidget);
    lineEdit->setText(text);
    QObject::connect(lineEdit, &QLineEdit::textChanged, this, &InspectorInvokeWidget::invokeTextChanged);
    rowLayout->addWidget(lineEdit, 1);

    QPushButton* playButton = new QPushButton(QString::fromUtf8("\u25B6"), rowWidget);
    playButton->setFixedSize(33, 25);
    playButton->setToolTip("Invoke this label");
    playButton->setFocusPolicy(Qt::NoFocus);
    playButton->setStyleSheet(
        "QPushButton { background-color: rgba(50, 50, 50, 200); color: rgba(140, 140, 140, 200);"
        " border-radius: 3px; font-size: 12px; border: 1px solid rgba(70, 70, 70, 200);"
        " padding: 0px; min-width: 33px; max-width: 33px; max-height: 25px; }"
        " QPushButton:hover { background-color: rgba(70, 70, 70, 200); color: white; }");
    QObject::connect(playButton, &QPushButton::clicked, this, &InspectorInvokeWidget::invokePlayClicked);
    rowLayout->addWidget(playButton, 0);

    QPushButton* radio = new QPushButton("F7", rowWidget);
    radio->setToolTip("Set as default for F7 hotkey and OSC invoke");
    radio->setFocusPolicy(Qt::NoFocus);
    radio->setCheckable(true);
    radio->setFixedSize(33, 25);
    radio->setStyleSheet(radioBtnStyle);
    int id = this->invokeRadios.size();
    this->invokeRadioGroup->addButton(radio, id);
    rowLayout->addWidget(radio, 0);

    this->invokeRowsLayout->addWidget(rowWidget);

    this->invokeLineEdits.append(lineEdit);
    this->invokePlayButtons.append(playButton);
    this->invokeRadios.append(radio);

    if (this->invokeRadios.size() == 1)
        radio->setChecked(true);

    this->invokeHeaderWidget->setVisible(true);

    emit contentChanged();
}

void InspectorInvokeWidget::removeInvokeRow()
{
    if (this->invokeLineEdits.size() <= 1)
        return;

    int lastIdx = this->invokeLineEdits.size() - 1;
    bool wasChecked = this->invokeRadios[lastIdx]->isChecked();

    this->invokeRadioGroup->removeButton(this->invokeRadios[lastIdx]);

    QLayoutItem* item = this->invokeRowsLayout->takeAt(lastIdx);
    if (item->widget())
        delete item->widget();
    delete item;

    this->invokeLineEdits.removeLast();
    this->invokePlayButtons.removeLast();
    this->invokeRadios.removeLast();

    if (wasChecked && !this->invokeRadios.isEmpty())
        this->invokeRadios[0]->setChecked(true);

    syncInvokesToCommand();

    emit contentChanged();
}

void InspectorInvokeWidget::invokePlayClicked()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (!button || !this->command)
        return;

    int idx = this->invokePlayButtons.indexOf(button);
    if (idx < 0 || idx >= this->invokeLineEdits.size())
        return;

    QString invokeText = this->invokeLineEdits[idx]->text();
    this->command->setPendingInvokeOverride(invokeText);
    EventManager::getInstance().fireExecutePlayoutCommandEvent(
        ExecutePlayoutCommandEvent(Playout::PlayoutType::Invoke));
}

void InspectorInvokeWidget::invokeRadioChanged(int id)
{
    if (!this->command)
        return;

    this->command->setInvokeHotkeyIndex(id);
}

void InspectorInvokeWidget::invokeTextChanged(const QString& text)
{
    Q_UNUSED(text);
    syncInvokesToCommand();
}

void InspectorInvokeWidget::syncInvokesToCommand()
{
    if (!this->command)
        return;

    QStringList invokes;
    for (QLineEdit* le : this->invokeLineEdits)
        invokes.append(le->text());

    int hotkeyIdx = this->invokeRadioGroup->checkedId();
    if (hotkeyIdx < 0) hotkeyIdx = 0;

    this->command->setInvokes(invokes);
    this->command->setInvokeHotkeyIndex(hotkeyIdx);
}

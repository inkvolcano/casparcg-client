#include "InspectorInvokeWidget.h"

#include "../WheelGuard.h"

#include "DatabaseManager.h"
#include "EventManager.h"
#include "Events/StatusbarEvent.h"
#include "Events/Rundown/ExecutePlayoutCommandEvent.h"
#include "Models/DeviceModel.h"
#include "Playout.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QRegularExpression>
#include <QtCore/QSet>
#include <QtCore/QTextStream>

#include <functional>

#include <QtGui/QMouseEvent>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QToolButton>

namespace
{
    // Grip at the left of each invoke row: press and drag vertically to reorder.
    // Rows swap live as the cursor passes a neighbour (no drop target needed).
    class InvokeDragHandle : public QLabel
    {
        public:
            explicit InvokeDragHandle(QWidget* parent) : QLabel(parent)
            {
                setText(QString::fromUtf8("\xe2\xa0\xbf"));   // braille grip dots
                setFixedSize(14, 25);
                setAlignment(Qt::AlignCenter);
                setCursor(Qt::OpenHandCursor);
                setToolTip("Drag to reorder this invoke");
                setStyleSheet("color: rgba(130, 130, 130, 220); font-size: 13px;");
            }

            std::function<void(const QPoint&)> onDragTo;   // receives a global position

        protected:
            void mousePressEvent(QMouseEvent* event) override
            {
                if (event->button() == Qt::LeftButton)
                {
                    this->dragging = true;
                    setCursor(Qt::ClosedHandCursor);
                }
            }

            void mouseMoveEvent(QMouseEvent* event) override
            {
                if (this->dragging && this->onDragTo)
                    this->onDragTo(event->globalPosition().toPoint());
            }

            void mouseReleaseEvent(QMouseEvent*) override
            {
                this->dragging = false;
                setCursor(Qt::OpenHandCursor);
            }

        private:
            bool dragging = false;
    };
}

InspectorInvokeWidget::InspectorInvokeWidget(QWidget* parent)
    : QWidget(parent),
      command(nullptr), lock(false)
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(9, 6, 9, 6);
    mainLayout->setSpacing(0);

    mainLayout->addSpacing(16);

    // Header: [Import] plus [+] and [-] buttons, right-aligned.
    QHBoxLayout* buttonsLayout = new QHBoxLayout();
    buttonsLayout->setSpacing(2);
    buttonsLayout->addStretch();

    QPushButton* importButton = new QPushButton("Discover Functions", this);
    importButton->setFixedHeight(20);
    importButton->setFocusPolicy(Qt::NoFocus);
    importButton->setToolTip("Scan the template HTML for its functions and offer them as dropdown choices in the Function column");
    importButton->setStyleSheet(
        "QPushButton { background-color: rgba(50, 50, 50, 200); color: rgba(140, 140, 140, 200);"
        " border-radius: 3px; font-size: 10px; font-weight: bold;"
        " border: 1px solid rgba(70, 70, 70, 200); padding: 2px 8px; }"
        " QPushButton:hover { background-color: rgba(70, 70, 70, 200); color: white; }");
    QObject::connect(importButton, &QPushButton::clicked, this, &InspectorInvokeWidget::importInvokes);
    buttonsLayout->addWidget(importButton);

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

    QLabel* handleHeader = new QLabel("", this->invokeHeaderWidget);
    handleHeader->setFixedWidth(16);
    handleHeader->setStyleSheet(headerStyleFirst);
    headerLayout->addWidget(handleHeader, 0);

    QLabel* functionHeader = new QLabel("Function", this->invokeHeaderWidget);
    functionHeader->setStyleSheet(headerStyle);
    headerLayout->addWidget(functionHeader, 1);

    QLabel* labelHeader = new QLabel("Label", this->invokeHeaderWidget);
    labelHeader->setStyleSheet(headerStyle);
    headerLayout->addWidget(labelHeader, 1);

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
    this->model = event.getLibraryModel();

    if (dynamic_cast<TemplateCommand*>(event.getCommand()))
    {
        this->command = dynamic_cast<TemplateCommand*>(event.getCommand());

        // Refresh the dropdown choices for THIS item's template — discovered
        // functions must never leak from the previously selected item.
        this->discoveredFunctions = scanTemplateFunctions();

        clearInvokeRows();
        const QStringList& invokes = this->command->getInvokes();
        for (int i = 0; i < invokes.count(); i++)
            addInvokeRow(invokes[i], this->command->getInvokeLabelAt(i));
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

    this->invokeRowWidgets.clear();
    this->invokeDragHandles.clear();
    this->invokeCombos.clear();
    this->invokeLabelEdits.clear();
    this->invokePlayButtons.clear();
    this->invokeRadios.clear();

    this->invokeHeaderWidget->setVisible(false);

    emit contentChanged();
}

void InspectorInvokeWidget::addInvokeRow(const QString& text, const QString& label)
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

    InvokeDragHandle* handle = new InvokeDragHandle(rowWidget);
    handle->onDragTo = [this, handle](const QPoint& globalPos) {
        dragInvokeRowTo(handle, globalPos);
    };
    rowLayout->addWidget(handle, 0);

    QComboBox* combo = new QComboBox(rowWidget);
    combo->setEditable(true);
    combo->setInsertPolicy(QComboBox::NoInsert);
    combo->addItems(this->discoveredFunctions);
    combo->setCurrentText(text);
    WheelGuard::apply(combo);
    QObject::connect(combo, &QComboBox::editTextChanged, this, &InspectorInvokeWidget::invokeTextChanged);
    rowLayout->addWidget(combo, 1);

    // Operator-facing label: shown on Simple Mode sub-buttons instead of the function name.
    QLineEdit* labelEdit = new QLineEdit(rowWidget);
    labelEdit->setText(label);
    labelEdit->setPlaceholderText("Label");
    labelEdit->setToolTip("Operator label shown on Simple Mode buttons (invokes without a label are hidden there)");
    QObject::connect(labelEdit, &QLineEdit::textChanged, this, &InspectorInvokeWidget::invokeTextChanged);
    rowLayout->addWidget(labelEdit, 1);

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

    this->invokeRowWidgets.append(rowWidget);
    this->invokeDragHandles.append(handle);
    this->invokeCombos.append(combo);
    this->invokeLabelEdits.append(labelEdit);
    this->invokePlayButtons.append(playButton);
    this->invokeRadios.append(radio);

    if (this->invokeRadios.size() == 1)
        radio->setChecked(true);

    this->invokeHeaderWidget->setVisible(true);

    emit contentChanged();
}

void InspectorInvokeWidget::removeInvokeRow()
{
    if (this->invokeCombos.size() <= 1)
        return;

    int lastIdx = this->invokeCombos.size() - 1;
    bool wasChecked = this->invokeRadios[lastIdx]->isChecked();

    this->invokeRadioGroup->removeButton(this->invokeRadios[lastIdx]);

    QLayoutItem* item = this->invokeRowsLayout->takeAt(lastIdx);
    if (item->widget())
        delete item->widget();
    delete item;

    this->invokeRowWidgets.removeLast();
    this->invokeDragHandles.removeLast();
    this->invokeCombos.removeLast();
    this->invokeLabelEdits.removeLast();
    this->invokePlayButtons.removeLast();
    this->invokeRadios.removeLast();

    if (wasChecked && !this->invokeRadios.isEmpty())
        this->invokeRadios[0]->setChecked(true);

    syncInvokesToCommand();

    emit contentChanged();
}

// Live reorder while dragging a row's grip: find the row under the cursor and
// move the dragged row there. Rows never swap past the ends of the list.
void InspectorInvokeWidget::dragInvokeRowTo(QWidget* handle, const QPoint& globalPos)
{
    int from = this->invokeDragHandles.indexOf(handle);
    if (from < 0 || this->invokeRowWidgets.isEmpty())
        return;

    int to = -1;
    for (int i = 0; i < this->invokeRowWidgets.size(); i++)
    {
        QWidget* row = this->invokeRowWidgets[i];
        int y = row->mapFromGlobal(globalPos).y();
        if (y >= 0 && y < row->height())
        {
            to = i;
            break;
        }
    }

    // Cursor above the first row / below the last one: clamp to the ends.
    if (to < 0)
    {
        if (this->invokeRowWidgets.first()->mapFromGlobal(globalPos).y() < 0)
            to = 0;
        else if (this->invokeRowWidgets.last()->mapFromGlobal(globalPos).y() >= this->invokeRowWidgets.last()->height())
            to = this->invokeRowWidgets.size() - 1;
    }

    if (to >= 0 && to != from)
        moveInvokeRow(from, to);
}

void InspectorInvokeWidget::moveInvokeRow(int from, int to)
{
    if (from == to || from < 0 || to < 0
        || from >= this->invokeRowWidgets.size() || to >= this->invokeRowWidgets.size())
        return;

    QLayoutItem* item = this->invokeRowsLayout->takeAt(from);
    if (item == nullptr)
        return;
    this->invokeRowsLayout->insertItem(to, item);

    this->invokeRowWidgets.move(from, to);
    this->invokeDragHandles.move(from, to);
    this->invokeCombos.move(from, to);
    this->invokeLabelEdits.move(from, to);
    this->invokePlayButtons.move(from, to);
    this->invokeRadios.move(from, to);

    // The F7 radio group is keyed by row index — renumber, keeping the same row checked.
    int checkedIdx = -1;
    for (int i = 0; i < this->invokeRadios.size(); i++)
    {
        if (this->invokeRadios[i]->isChecked())
            checkedIdx = i;
        this->invokeRadioGroup->removeButton(this->invokeRadios[i]);
    }
    for (int i = 0; i < this->invokeRadios.size(); i++)
        this->invokeRadioGroup->addButton(this->invokeRadios[i], i);
    if (checkedIdx >= 0)
        this->invokeRadios[checkedIdx]->setChecked(true);

    syncInvokesToCommand();   // order is what gets persisted to the rundown XML
}

// Silent scan of the selected item's template HTML for raw JS function
// declarations. Returns an empty list when the template can't be resolved or
// read; outFilePath (optional) receives the resolved path when there is one.
QStringList InspectorInvokeWidget::scanTemplateFunctions(QString* outFilePath) const
{
    if (this->command.isNull() || this->model == nullptr)
        return {};

    QString deviceName = this->model->getDeviceName();
    if (deviceName.isEmpty())
        return {};

    QString templatePath = DatabaseManager::getInstance().getDeviceByName(deviceName).getTemplatePath();
    if (templatePath.isEmpty())
        return {};

    QString templateName = this->command->getTemplateName();
    if (templateName.isEmpty())
        return {};

    QString filePath = QDir(templatePath).filePath(templateName + ".html");
    if (outFilePath != nullptr)
        *outFilePath = filePath;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QString content = QTextStream(&file).readAll();
    file.close();

    // Detect raw JS function declarations in the template:
    //   function name(...)   window.name = function   const/let/var name = function/arrow
    // The webcg lifecycle functions are skipped — they are driven by the client's
    // own play/stop/update commands, not by CG INVOKE.
    static const QSet<QString> lifecycle = {
        "update", "play", "stop", "next", "remove", "init", "data"
    };

    QStringList discovered;
    auto collect = [&discovered](QRegularExpressionMatchIterator it) {
        while (it.hasNext())
        {
            QString name = it.next().captured(1);
            if (!lifecycle.contains(name) && !discovered.contains(name))
                discovered.append(name);
        }
    };

    collect(QRegularExpression("\\bfunction\\s+([A-Za-z_$][\\w$]*)\\s*\\(").globalMatch(content));
    collect(QRegularExpression("window\\.([A-Za-z_$][\\w$]*)\\s*=\\s*(?:async\\s+)?function").globalMatch(content));
    collect(QRegularExpression("(?:const|let|var)\\s+([A-Za-z_$][\\w$]*)\\s*=\\s*(?:async\\s+)?(?:function\\b|\\([^)]*\\)\\s*=>)").globalMatch(content));

    return discovered;
}

void InspectorInvokeWidget::importInvokes()
{
    if (this->command.isNull() || this->model == nullptr)
        return;

    QString filePath;
    QStringList discovered = scanTemplateFunctions(&filePath);

    if (discovered.isEmpty())
    {
        EventManager::getInstance().fireStatusbarEvent(StatusbarEvent(filePath.isEmpty()
            ? QString("Discover functions: no device/template path set for this item")
            : QString("No functions found in: %1").arg(filePath)));
        return;
    }

    // Offer the functions as dropdown choices on every row (existing text kept).
    this->discoveredFunctions = discovered;
    for (QComboBox* combo : this->invokeCombos)
    {
        QString current = combo->currentText();
        combo->blockSignals(true);
        combo->clear();
        combo->addItems(this->discoveredFunctions);
        combo->setCurrentText(current);
        combo->blockSignals(false);
    }

    EventManager::getInstance().fireStatusbarEvent(
        StatusbarEvent(QString("Discovered %1 functions in: %2 \xe2\x80\x94 pick them from the Function dropdowns").arg(discovered.count()).arg(filePath)));
}

void InspectorInvokeWidget::invokePlayClicked()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (!button || !this->command)
        return;

    int idx = this->invokePlayButtons.indexOf(button);
    if (idx < 0 || idx >= this->invokeCombos.size())
        return;

    QString invokeText = this->invokeCombos[idx]->currentText();
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
    for (QComboBox* combo : this->invokeCombos)
        invokes.append(combo->currentText());

    QStringList labels;
    for (QLineEdit* le : this->invokeLabelEdits)
        labels.append(le->text());

    int hotkeyIdx = this->invokeRadioGroup->checkedId();
    if (hotkeyIdx < 0) hotkeyIdx = 0;

    this->command->setInvokes(invokes);
    this->command->setInvokeLabels(labels);
    this->command->setInvokeHotkeyIndex(hotkeyIdx);
}

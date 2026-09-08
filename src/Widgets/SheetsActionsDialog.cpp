#include "SheetsActionsDialog.h"

#include <QtWidgets/QColorDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>

SheetsActionsDialog::SheetsActionsDialog(const QString& tabName, const QStringList& headers,
                                         const QStringList& templateNames,
                                         const SheetsTabActions& actions, QWidget* parent)
    : QDialog(parent), headers(headers), templateNames(templateNames), actions(actions)
{
    setWindowTitle(QString("Sheet Actions \xe2\x80\x94 %1").arg(tabName));
    resize(620, 420);

    QVBoxLayout* layout = new QVBoxLayout(this);

    this->treeWidget = new QTreeWidget(this);
    this->treeWidget->setColumnCount(6);
    this->treeWidget->setHeaderLabels({"Scope", "Label", "Template", "Ch/Layer", "Data", "Action"});
    this->treeWidget->setRootIsDecorated(false);
    this->treeWidget->header()->setStretchLastSection(false);
    this->treeWidget->header()->setSectionResizeMode(4, QHeaderView::Stretch);
    layout->addWidget(this->treeWidget, 1);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* addRowButton = new QPushButton("Add Row Button", this);
    QPushButton* addStandalone = new QPushButton("Add Standalone Button", this);
    QPushButton* editButton = new QPushButton("Edit", this);
    QPushButton* removeButton = new QPushButton("Remove", this);
    buttonLayout->addWidget(addRowButton);
    buttonLayout->addWidget(addStandalone);
    buttonLayout->addStretch();
    buttonLayout->addWidget(editButton);
    buttonLayout->addWidget(removeButton);
    layout->addLayout(buttonLayout);

    QLabel* hint = new QLabel(
        "Row buttons appear next to every data row; use {COLUMN} placeholders in Label and Data.\n"
        "Data format: key=value,key2={COLUMN} \xe2\x80\x94 sent to the template as componentData.", this);
    hint->setStyleSheet("color: rgba(170, 170, 170, 200); font-size: 10px;");
    layout->addWidget(hint);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttonBox);

    QObject::connect(addRowButton, &QPushButton::clicked, this, &SheetsActionsDialog::addRowButton);
    QObject::connect(addStandalone, &QPushButton::clicked, this, &SheetsActionsDialog::addStandaloneButton);
    QObject::connect(editButton, &QPushButton::clicked, this, &SheetsActionsDialog::editSelected);
    QObject::connect(removeButton, &QPushButton::clicked, this, &SheetsActionsDialog::removeSelected);
    QObject::connect(this->treeWidget, &QTreeWidget::itemDoubleClicked, this, &SheetsActionsDialog::editSelected);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    rebuildTree();
}

SheetsTabActions SheetsActionsDialog::getActions() const
{
    return this->actions;
}

void SheetsActionsDialog::rebuildTree()
{
    this->treeWidget->clear();

    auto addItem = [this](const QString& scope, const SheetsActionDef& def) {
        QTreeWidgetItem* item = new QTreeWidgetItem();
        item->setText(0, scope);
        item->setText(1, def.label);
        item->setText(2, def.templateName);
        item->setText(3, QString("%1/%2").arg(def.channel).arg(def.videolayer));
        item->setText(4, def.data);
        item->setText(5, def.action);
        if (!def.color.isEmpty())
            item->setBackground(1, QColor(def.color));
        this->treeWidget->addTopLevelItem(item);
    };

    foreach (const SheetsActionDef& def, this->actions.rowButtons)
        addItem("Row", def);
    foreach (const SheetsActionDef& def, this->actions.standalone)
        addItem("Standalone", def);

    for (int col = 0; col < 4; col++)
        this->treeWidget->resizeColumnToContents(col);
}

bool SheetsActionsDialog::editDefinition(SheetsActionDef& def)
{
    QDialog dialog(this);
    dialog.setWindowTitle("Edit Action");

    QFormLayout* form = new QFormLayout(&dialog);

    QLineEdit* labelEdit = new QLineEdit(def.label, &dialog);
    form->addRow("Label:", labelEdit);

    QPushButton* colorButton = new QPushButton(def.color.isEmpty() ? "Default" : def.color, &dialog);
    QString pickedColor = def.color;
    if (!pickedColor.isEmpty())
        colorButton->setStyleSheet(QString("background-color: %1;").arg(pickedColor));
    QObject::connect(colorButton, &QPushButton::clicked, &dialog, [&dialog, colorButton, &pickedColor]() {
        QColor initial = pickedColor.isEmpty() ? QColor(Qt::gray) : QColor(pickedColor);
        QColor color = QColorDialog::getColor(initial, &dialog, "Button Color");
        if (color.isValid())
        {
            pickedColor = color.name();
            colorButton->setText(pickedColor);
            colorButton->setStyleSheet(QString("background-color: %1;").arg(pickedColor));
        }
    });
    form->addRow("Color:", colorButton);

    // Editable combo listing the client's template library; free typing still allowed
    // for templates the library has not picked up yet.
    QComboBox* templateCombo = new QComboBox(&dialog);
    templateCombo->setEditable(true);
    templateCombo->setInsertPolicy(QComboBox::NoInsert);
    templateCombo->addItems(this->templateNames);
    templateCombo->setCurrentText(def.templateName);
    templateCombo->lineEdit()->setPlaceholderText("e.g. SEVILLE/card");
    form->addRow("Template:", templateCombo);

    QSpinBox* channelSpin = new QSpinBox(&dialog);
    channelSpin->setRange(1, 99);
    channelSpin->setValue(def.channel);
    form->addRow("Channel:", channelSpin);

    QSpinBox* layerSpin = new QSpinBox(&dialog);
    layerSpin->setRange(1, 999);
    layerSpin->setValue(def.videolayer);
    form->addRow("Video layer:", layerSpin);

    QLineEdit* dataEdit = new QLineEdit(def.data, &dialog);
    dataEdit->setPlaceholderText("name={NAME},number={NUMBER},card=yellow");
    form->addRow("Data:", dataEdit);

    // Placeholder helper: pick a column, it is appended to the data field.
    if (!this->headers.isEmpty())
    {
        QComboBox* placeholderCombo = new QComboBox(&dialog);
        placeholderCombo->addItem("Insert column placeholder...");
        placeholderCombo->addItems(this->headers);
        QObject::connect(placeholderCombo, QOverload<int>::of(&QComboBox::activated),
                         &dialog, [placeholderCombo, dataEdit](int index) {
            if (index <= 0)
                return;
            dataEdit->insert("{" + placeholderCombo->itemText(index) + "}");
            placeholderCombo->setCurrentIndex(0);
        });
        form->addRow("", placeholderCombo);
    }

    QComboBox* actionCombo = new QComboBox(&dialog);
    actionCombo->addItems({"play", "stop", "update", "toggle"});
    actionCombo->setCurrentText(def.action);
    form->addRow("Action:", actionCombo);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttonBox);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return false;
    if (labelEdit->text().trimmed().isEmpty())
        return false;

    def.label = labelEdit->text();
    def.color = pickedColor;
    def.templateName = templateCombo->currentText().trimmed();
    def.channel = channelSpin->value();
    def.videolayer = layerSpin->value();
    def.data = dataEdit->text().trimmed();
    def.action = actionCombo->currentText();
    return true;
}

void SheetsActionsDialog::addRowButton()
{
    SheetsActionDef def;
    if (editDefinition(def))
    {
        this->actions.rowButtons.append(def);
        rebuildTree();
    }
}

void SheetsActionsDialog::addStandaloneButton()
{
    SheetsActionDef def;
    if (editDefinition(def))
    {
        this->actions.standalone.append(def);
        rebuildTree();
    }
}

void SheetsActionsDialog::editSelected()
{
    int index = this->treeWidget->indexOfTopLevelItem(this->treeWidget->currentItem());
    if (index < 0)
        return;

    int rowCount = this->actions.rowButtons.count();
    SheetsActionDef& def = (index < rowCount)
        ? this->actions.rowButtons[index]
        : this->actions.standalone[index - rowCount];

    if (editDefinition(def))
        rebuildTree();
}

void SheetsActionsDialog::removeSelected()
{
    int index = this->treeWidget->indexOfTopLevelItem(this->treeWidget->currentItem());
    if (index < 0)
        return;

    int rowCount = this->actions.rowButtons.count();
    if (index < rowCount)
        this->actions.rowButtons.removeAt(index);
    else
        this->actions.standalone.removeAt(index - rowCount);

    rebuildTree();
}

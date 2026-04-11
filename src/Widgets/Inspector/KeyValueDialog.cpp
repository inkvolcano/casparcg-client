#include "KeyValueDialog.h"

#include <QtGui/QCloseEvent>

#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QMessageBox>

KeyValueDialog::KeyValueDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUi(this);

    this->comboBoxMode->addItem("Text", 0);
    this->comboBoxMode->addItem("Integer", 1);
    this->comboBoxMode->addItem("Decimal", 2);
    this->comboBoxMode->addItem("Boolean", 3);
    this->comboBoxMode->addItem("Color", 4);
    this->comboBoxMode->addItem("Cycle", 5);

    // Cycle values field is hidden by default (only shown when mode is Cycle).
    this->labelCycleValues->setVisible(false);
    this->lineEditCycleValues->setVisible(false);
    QObject::connect(this->comboBoxMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     this, &KeyValueDialog::modeChanged);

    this->textEditValue->installEventFilter(this);
}

bool KeyValueDialog::eventFilter(QObject* target, QEvent* event)
{
    if (event->type() == QEvent::KeyPress)
    {
        QKeyEvent* keyEvent = dynamic_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Enter)
        {
            QDialog::accept();
            return true;
        }
    }

    return QObject::eventFilter(target, event);
}

void KeyValueDialog::accept()
{
    if (this->lineEditKey->text().isEmpty())
        return;

    QDialog::accept();
}

const QString KeyValueDialog::getKey() const
{
    return this->lineEditKey->text();
}

const QString KeyValueDialog::getValue() const
{
    return this->textEditValue->toPlainText();
}

void KeyValueDialog::setKey(const QString& key)
{
    this->lineEditKey->setText(key);

    this->textEditValue->setFocus();
}

void KeyValueDialog::setValue(const QString& value)
{
    this->textEditValue->setPlainText(value);
    this->textEditValue->selectAll();
}

int KeyValueDialog::getMode() const
{
    return this->comboBoxMode->currentData().toInt();
}

void KeyValueDialog::setMode(int mode)
{
    int index = this->comboBoxMode->findData(mode);
    if (index >= 0)
        this->comboBoxMode->setCurrentIndex(index);
}

const QString KeyValueDialog::getCycleValues() const
{
    return this->lineEditCycleValues->text();
}

void KeyValueDialog::setCycleValues(const QString& cycleValues)
{
    this->lineEditCycleValues->setText(cycleValues);
}

void KeyValueDialog::modeChanged(int index)
{
    bool isCycle = (this->comboBoxMode->itemData(index).toInt() == 5);
    this->labelCycleValues->setVisible(isCycle);
    this->lineEditCycleValues->setVisible(isCycle);
}

void KeyValueDialog::setTitle(const QString& title)
{
    this->setWindowTitle(title);
}

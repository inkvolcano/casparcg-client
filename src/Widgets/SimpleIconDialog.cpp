#include "SimpleIconDialog.h"

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

namespace
{
    // Curated glyph set: playout, sport, status. All plain text — XML-portable.
    const char* ICONS[] = {
        "\xe2\x96\xb6", "\xe2\x96\xa0", "\xe2\x8f\xad", "\xe2\x8f\xb8", "\xe2\x86\xba", "\xe2\x9c\x96",
        "\xe2\x9a\xbd", "\xf0\x9f\x9f\xa8", "\xf0\x9f\x9f\xa5", "\xf0\x9f\xa7\xa4", "\xf0\x9f\x91\x95", "\xf0\x9f\x8f\x86",
        "\xf0\x9f\x93\x8a", "\xf0\x9f\x93\x8b", "\xf0\x9f\x93\xa3", "\xf0\x9f\x93\xba", "\xf0\x9f\x8e\xac", "\xf0\x9f\x8e\xa5",
        "\xe2\x8f\xb1", "\xe2\x8f\xb3", "\xf0\x9f\x94\x81", "\xf0\x9f\x94\x94", "\xe2\x9a\xa0", "\xe2\x9d\x97",
        "\xe2\x9c\x94", "\xe2\x9c\xa8", "\xe2\xad\x90", "\xf0\x9f\x94\xb4", "\xf0\x9f\x9f\xa2", "\xf0\x9f\x94\xb5",
        "\xf0\x9f\x85\xb0", "\xf0\x9f\x85\xb1", "\xe2\x84\xb9", "\xf0\x9f\x94\xa2", "\xf0\x9f\x93\x8d", "\xf0\x9f\x8e\xaf"
    };
    const int ICON_COUNT = sizeof(ICONS) / sizeof(ICONS[0]);
    const int ICONS_PER_ROW = 6;
}

SimpleIconDialog::SimpleIconDialog(const QString& currentIcon, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Select Icon");

    QVBoxLayout* layout = new QVBoxLayout(this);

    QGridLayout* grid = new QGridLayout();
    grid->setSpacing(4);

    for (int i = 0; i < ICON_COUNT; i++)
    {
        QString glyph = QString::fromUtf8(ICONS[i]);
        QPushButton* button = new QPushButton(glyph, this);
        button->setFixedSize(44, 44);
        button->setFocusPolicy(Qt::NoFocus);
        bool isCurrent = (glyph == currentIcon);
        button->setStyleSheet(QString(
            "QPushButton { background-color: rgba(50, 50, 50, 220); border-radius: 5px; font-size: 20px;"
            " border: %1; }"
            "QPushButton:hover { background-color: rgba(80, 80, 80, 220); border: 2px solid rgba(200, 200, 200, 200); }")
            .arg(isCurrent ? "2px solid #50c878" : "1px solid rgba(80, 80, 80, 200)"));

        QObject::connect(button, &QPushButton::clicked, this, [this, glyph]() {
            this->lineEditCustom->setText(glyph);
            accept();
        });

        grid->addWidget(button, i / ICONS_PER_ROW, i % ICONS_PER_ROW);
    }
    layout->addLayout(grid);

    // Custom text row: any short text/emoji can be an icon.
    QHBoxLayout* customRow = new QHBoxLayout();
    customRow->addWidget(new QLabel("Custom:", this));
    this->lineEditCustom = new QLineEdit(currentIcon, this);
    this->lineEditCustom->setMaxLength(4);
    this->lineEditCustom->setPlaceholderText("text or emoji");
    customRow->addWidget(this->lineEditCustom, 1);

    QPushButton* clearButton = new QPushButton("No Icon", this);
    clearButton->setFocusPolicy(Qt::NoFocus);
    QObject::connect(clearButton, &QPushButton::clicked, this, [this]() {
        this->lineEditCustom->clear();
        accept();
    });
    customRow->addWidget(clearButton);
    layout->addLayout(customRow);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);
}

QString SimpleIconDialog::getIcon() const
{
    return this->lineEditCustom->text().trimmed();
}
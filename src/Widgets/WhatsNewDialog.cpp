#include "WhatsNewDialog.h"

#include "DatabaseManager.h"
#include "Models/ConfigurationModel.h"

#include <QtCore/QFile>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTextBrowser>
#include <QtWidgets/QVBoxLayout>

WhatsNewDialog::WhatsNewDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("What's New");
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    resize(700, 740);

    QVBoxLayout* layout = new QVBoxLayout(this);

    // Top bar: checkbox on left, OK on right.
    QHBoxLayout* topLayout = new QHBoxLayout();

    this->checkBoxHideOnStartup = new QCheckBox("Don't show on startup", this);
    QString val = DatabaseManager::getInstance().getConfigurationByName("HideWhatsNew").getValue();
    this->checkBoxHideOnStartup->setChecked(val == "true");

    topLayout->addWidget(this->checkBoxHideOnStartup);
    topLayout->addStretch();

    QPushButton* buttonOk = new QPushButton("OK", this);
    buttonOk->setDefault(true);
    QObject::connect(buttonOk, &QPushButton::clicked, this, [this]() {
        DatabaseManager::getInstance().updateConfiguration(
            ConfigurationModel(0, "HideWhatsNew", this->checkBoxHideOnStartup->isChecked() ? "true" : "false"));
        accept();
    });

    topLayout->addWidget(buttonOk);
    layout->addLayout(topLayout);

    QTextBrowser* browser = new QTextBrowser(this);
    browser->setOpenExternalLinks(true);

    QFile file(":/Data/CHANGES.html");
    if (file.open(QFile::ReadOnly))
    {
        QString content = QString::fromUtf8(file.readAll());
        browser->setHtml(content);
    }
    else
    {
        browser->setPlainText("Could not load changes.");
    }

    layout->addWidget(browser);
}

void WhatsNewDialog::showOnStartupIfEnabled(QWidget* parent)
{
    QString val = DatabaseManager::getInstance().getConfigurationByName("HideWhatsNew").getValue();
    if (val == "true")
        return;

    WhatsNewDialog* dialog = new WhatsNewDialog(parent);
    dialog->exec();
    dialog->deleteLater();
}

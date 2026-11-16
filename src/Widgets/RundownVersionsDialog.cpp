#include "RundownVersionsDialog.h"

#include "DatabaseManager.h"

#include "EventManager.h"
#include "Global.h"
#include "RundownVersions.h"
#include "Rundown/RundownTreeWidget.h"

#include "Events/Rundown/OpenRundownEvent.h"
#include "Events/StatusbarEvent.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QUrl>

#include <QtGui/QDesktopServices>

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>

// Settings > Rundown versions kept (build 314), 10 unless set.
static int versionsKept()
{
    const int keep = DatabaseManager::getInstance().getConfigurationByName("RundownVersionsKept").getValue().toInt();
    return keep > 0 ? keep : RundownVersions::KEEP;
}

void RundownVersionsDialog::keepVersion(const QString& rundownPath, const QByteArray& replacement)
{
    QFile current(rundownPath);
    if (!current.exists() || !current.open(QIODevice::ReadOnly))
        return;
    const QByteArray before = current.readAll();
    current.close();

    // Saving what is already there keeps nothing: a version is a change.
    if (!replacement.isEmpty() && before == replacement)
        return;

    const QString folder = RundownVersions::folderFor(rundownPath);
    if (!QDir().mkpath(folder))
        return;

    const QString target = QDir(folder).filePath(RundownVersions::versionName(rundownPath, QDateTime::currentDateTime()));
    QFile out(target);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    out.write(before);
    out.close();

    // Only this rundown's own version files are ever counted or removed.
    for (const QString& name : RundownVersions::beyondKept(rundownPath, QDir(folder).entryList(QDir::Files), versionsKept()))
        QFile::remove(QDir(folder).filePath(name));
}

void RundownVersionsDialog::openFor(RundownTreeWidget* rundown, QWidget* parent)
{
    if (rundown == nullptr || rundown->getActiveRundown() == Rundown::DEFAULT_NAME)
    {
        QMessageBox::information(parent, "Rundown Versions", "This rundown has not been saved yet, so it has no versions.");
        return;
    }

    RundownVersionsDialog* dialog = new RundownVersionsDialog(rundown, parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

RundownVersionsDialog::RundownVersionsDialog(RundownTreeWidget* rundown, QWidget* parent)
    : QDialog(parent), rundown(rundown), rundownPath(rundown->getActiveRundown())
{
    setWindowTitle(QString("Rundown Versions - %1").arg(QFileInfo(this->rundownPath).fileName()));
    resize(560, 380);

    QVBoxLayout* layout = new QVBoxLayout(this);

    this->labelInfo = new QLabel(this);
    this->labelInfo->setWordWrap(true);
    layout->addWidget(this->labelInfo);

    this->list = new QTreeWidget(this);
    this->list->setColumnCount(2);
    this->list->setHeaderLabels(QStringList() << "Saved" << "Size");
    this->list->setRootIsDecorated(false);
    this->list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    this->list->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    QObject::connect(this->list, &QTreeWidget::currentItemChanged, this, [this]() {
        const bool chosen = !chosenPath().isEmpty();
        this->buttonOpen->setEnabled(chosen);
        this->buttonPutBack->setEnabled(chosen && !this->rundown.isNull());
    });
    layout->addWidget(this->list, 1);

    QHBoxLayout* buttons = new QHBoxLayout();
    this->buttonOpen = new QPushButton("Open in a New Tab", this);
    this->buttonOpen->setToolTip("Open this version on its own, to look at it or copy items from it. The rundown is not changed.");
    QObject::connect(this->buttonOpen, &QPushButton::clicked, this, [this]() {
        const QString path = chosenPath();
        if (!path.isEmpty())
            EventManager::getInstance().fireOpenRundownEvent(OpenRundownEvent(path));
    });
    buttons->addWidget(this->buttonOpen);
    this->buttonPutBack = new QPushButton("Put Back...", this);
    this->buttonPutBack->setToolTip("Make this version the rundown again. The rundown as it is now is kept as a version first.");
    QObject::connect(this->buttonPutBack, &QPushButton::clicked, this, [this]() { putBack(); });
    buttons->addWidget(this->buttonPutBack);
    buttons->addStretch(1);
    QPushButton* folder = new QPushButton("Open Folder", this);
    QObject::connect(folder, &QPushButton::clicked, this, [this]() {
        const QString path = RundownVersions::folderFor(this->rundownPath);
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).isDir() ? path : QFileInfo(this->rundownPath).absolutePath()));
    });
    buttons->addWidget(folder);
    QPushButton* close = new QPushButton("Close", this);
    QObject::connect(close, &QPushButton::clicked, this, &QDialog::reject);
    buttons->addWidget(close);
    layout->addLayout(buttons);

    fill();
}

QString RundownVersionsDialog::chosenPath() const
{
    const QTreeWidgetItem* item = this->list->currentItem();
    return item == nullptr ? QString() : item->data(0, Qt::UserRole).toString();
}

void RundownVersionsDialog::fill()
{
    this->list->clear();

    const QString folder = RundownVersions::folderFor(this->rundownPath);
    const QStringList versions = RundownVersions::versionsAmong(this->rundownPath, QDir(folder).entryList(QDir::Files));

    for (const QString& name : versions)
    {
        const QString path = QDir(folder).filePath(name);
        const QDateTime when = RundownVersions::versionTime(this->rundownPath, name);
        QTreeWidgetItem* item = new QTreeWidgetItem(this->list, QStringList()
            << when.toString("ddd d MMM yyyy, HH:mm:ss") << QString("%1 KB").arg(qMax<qint64>(1, QFileInfo(path).size() / 1024)));
        item->setData(0, Qt::UserRole, path);
        item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
    }

    this->labelInfo->setText(versions.isEmpty()
        ? QString("No versions yet. Each save that changes %1 keeps the one before it here, the newest %2.")
              .arg(QFileInfo(this->rundownPath).fileName()).arg(versionsKept())
        : QString("The %1 most recent saves of %2 before the current one, newest first. They are kept in %3.")
              .arg(versions.count()).arg(QFileInfo(this->rundownPath).fileName())
              .arg(QDir::toNativeSeparators(folder)));

    this->buttonOpen->setEnabled(false);
    this->buttonPutBack->setEnabled(false);
}

void RundownVersionsDialog::putBack()
{
    const QString chosen = chosenPath();
    if (chosen.isEmpty() || this->rundown.isNull())
        return;

    if (QMessageBox::question(this, "Put Back",
            QString("Make the version saved %1 the rundown again?\n\n"
                    "The rundown as it is saved now is kept as a version first. Changes not saved yet are lost.")
                .arg(this->list->currentItem()->text(0)),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    QFile source(chosen);
    if (!source.open(QIODevice::ReadOnly))
    {
        QMessageBox::warning(this, "Put Back", "That version could not be read.");
        return;
    }
    const QByteArray data = source.readAll();
    source.close();

    keepVersion(this->rundownPath, data);

    QFile target(this->rundownPath);
    if (!target.open(QIODevice::WriteOnly | QIODevice::Truncate) || target.write(data) != data.size())
    {
        QMessageBox::warning(this, "Put Back", QString("%1 could not be written.").arg(QDir::toNativeSeparators(this->rundownPath)));
        return;
    }
    target.close();

    this->rundown->reloadRundown();
    EventManager::getInstance().fireStatusbarEvent(StatusbarEvent("Version put back", 4000));
    fill();
}

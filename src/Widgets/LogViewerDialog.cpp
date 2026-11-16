#include "LogViewerDialog.h"

#include <QtCore/QDate>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QPointer>
#include <QtCore/QSignalBlocker>
#include <QtCore/QUrl>

#include <QtGui/QDesktopServices>
#include <QtGui/QFont>
#include <QtGui/QTextCursor>

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QVBoxLayout>

#include <algorithm>
#include <functional>

namespace
{
    const int SHOWN = 5000;
}

QString LogViewerDialog::logFolder()
{
    return QString("%1/.CasparCG/Client/Logs").arg(QDir::homePath());
}

void LogViewerDialog::openFor(QWidget* parent)
{
    static QPointer<LogViewerDialog> open;
    if (open != nullptr)
    {
        open->raise();
        open->activateWindow();
        return;
    }
    open = new LogViewerDialog(parent);
    open->setAttribute(Qt::WA_DeleteOnClose);
    open->show();
}

LogViewerDialog::LogViewerDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Client Log");
    resize(980, 600);

    QVBoxLayout* layout = new QVBoxLayout(this);

    QHBoxLayout* top = new QHBoxLayout();
    top->addWidget(new QLabel("Day", this));
    this->comboDay = new QComboBox(this);
    top->addWidget(this->comboDay);
    top->addSpacing(12);
    top->addWidget(new QLabel("Show", this));
    this->comboFilter = new QComboBox(this);
    this->comboFilter->addItem("Everything");
    this->comboFilter->addItem("Warnings and errors");
    this->comboFilter->addItem("Errors only");
    top->addWidget(this->comboFilter);
    top->addSpacing(12);
    this->editSearch = new QLineEdit(this);
    this->editSearch->setPlaceholderText("Search");
    this->editSearch->setClearButtonEnabled(true);
    top->addWidget(this->editSearch, 1);
    this->checkFollow = new QCheckBox("Follow", this);
    this->checkFollow->setChecked(true);
    this->checkFollow->setToolTip("Add new lines as the client writes them (today's log only).");
    top->addWidget(this->checkFollow);
    layout->addLayout(top);

    this->text = new QPlainTextEdit(this);
    this->text->setReadOnly(true);
    this->text->setLineWrapMode(QPlainTextEdit::NoWrap);
    QFont mono("Consolas");
    mono.setStyleHint(QFont::Monospace);
    this->text->setFont(mono);
    layout->addWidget(this->text, 1);

    QHBoxLayout* bottom = new QHBoxLayout();
    this->labelCount = new QLabel(this);
    bottom->addWidget(this->labelCount, 1);
    QPushButton* folder = new QPushButton("Open Folder", this);
    QObject::connect(folder, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(LogViewerDialog::logFolder()));
    });
    bottom->addWidget(folder);
    QPushButton* close = new QPushButton("Close", this);
    QObject::connect(close, &QPushButton::clicked, this, &QDialog::reject);
    bottom->addWidget(close);
    layout->addLayout(bottom);

    // Typing a search waits for a pause, so a big log is not filtered per key.
    this->searchTimer.setSingleShot(true);
    this->searchTimer.setInterval(250);
    QObject::connect(&this->searchTimer, &QTimer::timeout, this, [this]() { refreshView(); });
    QObject::connect(this->editSearch, &QLineEdit::textChanged, this, [this]() { this->searchTimer.start(); });
    QObject::connect(this->comboFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { refreshView(); });
    QObject::connect(this->comboDay, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() { load(); });

    this->followTimer.setInterval(2000);
    QObject::connect(&this->followTimer, &QTimer::timeout, this, [this]() { follow(); });
    this->followTimer.start();

    fillDays();
    load();
}

void LogViewerDialog::fillDays()
{
    const QSignalBlocker blocker(this->comboDay);
    this->comboDay->clear();

    QStringList names = QDir(logFolder()).entryList(QStringList("Client_*.log"), QDir::Files);
    std::sort(names.begin(), names.end(), std::greater<QString>());
    for (const QString& name : names)
    {
        const QDate day = QDate::fromString(name.mid(7, 10), "yyyy-MM-dd");
        const QString label = day == QDate::currentDate() ? QString("Today") : day.isValid() ? day.toString("ddd d MMM yyyy") : name;
        this->comboDay->addItem(label, QDir(logFolder()).filePath(name));
    }
}

LogLines::Filter LogViewerDialog::filter() const
{
    switch (this->comboFilter->currentIndex())
    {
        case 1: return LogLines::Filter::WarningsAndErrors;
        case 2: return LogLines::Filter::Errors;
        default: return LogLines::Filter::Everything;
    }
}

void LogViewerDialog::load()
{
    this->entries.clear();
    this->reader = LogLines::Reader();
    this->readTo = 0;
    this->path = this->comboDay->currentData().toString();

    if (this->path.isEmpty())
    {
        this->text->setPlainText(QString("No log files in %1").arg(QDir::toNativeSeparators(logFolder())));
        this->labelCount->clear();
        return;
    }

    follow();
}

void LogViewerDialog::follow()
{
    if (this->path.isEmpty())
        return;

    // Only today's log grows; another day's is read once.
    const bool today = QFileInfo(this->path).fileName() == QString("Client_%1.log").arg(QDate::currentDate().toString("yyyy-MM-dd"));
    if (this->readTo > 0 && (!today || !this->checkFollow->isChecked()))
        return;

    QFile file(this->path);
    if (!file.open(QIODevice::ReadOnly))
        return;
    if (file.size() < this->readTo)
    {
        // Shorter than before: started again. Read it from the top.
        this->entries.clear();
        this->reader = LogLines::Reader();
        this->readTo = 0;
    }
    if (file.size() == this->readTo)
        return;

    file.seek(this->readTo);
    const QByteArray piece = file.readAll();
    this->readTo += piece.size();

    this->entries << this->reader.feed(QString::fromUtf8(piece));
    if (!today)
        this->entries << this->reader.flush();

    refreshView();
}

void LogViewerDialog::refreshView()
{
    const LogLines::Filter chosen = filter();
    const QString search = this->editSearch->text();

    // The entry still being written is shown too, so following is not one behind.
    QList<LogLines::Entry> all = this->entries;
    LogLines::Entry writing;
    if (this->reader.peek(&writing))
        all << writing;

    QStringList lines;
    int matching = 0;
    for (int i = all.count() - 1; i >= 0; i--)
    {
        if (!LogLines::passes(all.at(i), chosen, search))
            continue;
        matching++;
        if (lines.count() < SHOWN)
            lines.prepend(LogLines::format(all.at(i)));
    }

    // Kept at the bottom when it was there, so following reads like a tail.
    QScrollBar* bar = this->text->verticalScrollBar();
    const bool atEnd = bar->value() >= bar->maximum() - 2;
    this->text->setPlainText(lines.join('\n'));
    if (atEnd)
        this->text->moveCursor(QTextCursor::End);

    this->labelCount->setText(matching > SHOWN
        ? QString("The newest %1 of %2 matching entries, of %3 in this log.").arg(SHOWN).arg(matching).arg(all.count())
        : QString("%1 matching entries, of %2 in this log.").arg(matching).arg(all.count()));
}

// A push, from the tool to a client, driven through the buttons.
//
// Everything either side of this had a test and the middle did not. Comparing a
// pack against a client, listing what would change, sending only the ticked rows,
// stamping each with the digest of what was read off disk, and the client checking
// that digest before it writes: none of it had ever run.
//
// It runs here as an operator runs it. The window is built but never shown, the
// fields are filled in, and the Compare and Push buttons are clicked. Nothing is
// invoked behind the interface, so the wiring is under test as much as the code.
//
// The receiving end is the real client server on a spare port with its templates in
// a temporary folder. The tool's own saved settings are redirected to a temporary
// file first, so the operator's list of clients is never touched.

#include "../src/Push/PushWindow.h"
#include "../src/Widgets/SheetCacheServer.h"
#include "../src/Widgets/TemplateInstaller.h"

#include <QtCore/QDir>
#include <QtCore/QElapsedTimer>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QSettings>
#include <QtCore/QTemporaryDir>
#include <QtCore/QTextStream>
#include <QtCore/QThread>

#include <QtNetwork/QTcpServer>

#include <QtWidgets/QApplication>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTableWidget>

static int failures = 0;
static int checks = 0;

static const char* TOKEN = "the-client-push-token";

static void expectTrue(bool actual, const QString& what)
{
    checks++;
    if (actual)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n";
}

static void pump(int ms)
{
    QElapsedTimer clock;
    clock.start();
    while (clock.elapsed() < ms)
    {
        QCoreApplication::processEvents();
        QThread::msleep(5);
    }
}

static QPushButton* buttonNamed(QWidget* window, const QString& text)
{
    foreach (QPushButton* button, window->findChildren<QPushButton*>())
    {
        if (button->text() == text)
            return button;
    }
    return nullptr;
}

/** The two tables are told apart by their width: clients have 4 columns, files 6. */
static QTableWidget* tableWithColumns(QWidget* window, int columns)
{
    foreach (QTableWidget* table, window->findChildren<QTableWidget*>())
    {
        if (table->columnCount() == columns)
            return table;
    }
    return nullptr;
}

/** Click a button and wait for the work behind it to finish. */
static void clickAndWait(QPushButton* button, QTableWidget* files, int msWait = 15000)
{
    button->click();

    QElapsedTimer clock;
    clock.start();
    while (clock.elapsed() < msWait)
    {
        QCoreApplication::processEvents();
        QThread::msleep(10);

        // Every one of these re-enables its button when it is done.
        if (button->isEnabled() && clock.elapsed() > 300)
            break;
    }

    Q_UNUSED(files)
    pump(200);
}

static bool writeFile(const QString& path, const QByteArray& body)
{
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    file.write(body);
    file.close();
    return true;
}

static QByteArray readFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QByteArray();

    QByteArray body = file.readAll();
    file.close();
    return body;
}

/** What the file list is showing, as "pack/path=state" lines. */
static QStringList statesFrom(QTableWidget* files)
{
    QStringList rows;
    for (int row = 0; row < files->rowCount(); row++)
    {
        rows.append(QString("%1/%2=%3")
            .arg(files->item(row, 2) ? files->item(row, 2)->text() : QString(),
                 files->item(row, 3) ? files->item(row, 3)->text() : QString(),
                 files->item(row, 4) ? files->item(row, 4)->text() : QString()));
    }
    rows.sort();
    return rows;
}

int main(int argc, char** argv)
{
    // The tool's settings live under a fixed name, so they are redirected to a
    // temporary file before the window is built. Without this a test run would
    // overwrite whatever list of clients the operator had saved.
    QTemporaryDir settingsDir;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());

    QApplication app(argc, argv);
    QTextStream out(stdout);

    QTemporaryDir sandbox;
    if (!sandbox.isValid() || !settingsDir.isValid())
    {
        out << "could not make a temporary folder\n";
        return 2;
    }

    // ---- the client that will receive the push ----
    int port = 0;
    {
        QTcpServer finder;
        finder.listen(QHostAddress::LocalHost, 0);
        port = finder.serverPort();
    }

    QString clientTemplates = QDir(sandbox.path()).filePath("client");
    QDir().mkpath(clientTemplates);

    qputenv("CASPARCG_TEST_SheetsHostCache", "false");
    qputenv("CASPARCG_TEST_SheetsCacheDirectory", QDir(sandbox.path()).filePath("cache").toUtf8());
    qputenv("CASPARCG_TEST_SheetsHostCachePort", QByteArray::number(port));
    qputenv("CASPARCG_TEST_TemplatePushEnabled", "true");
    qputenv("CASPARCG_TEST_TemplatePushToken", TOKEN);
    qputenv("CASPARCG_TEST_TemplatePushPath", clientTemplates.toUtf8());

    SheetCacheServer::getInstance().start();
    expectTrue(SheetCacheServer::getInstance().isRunning(), "a client is listening to be pushed to");

    if (!SheetCacheServer::getInstance().isRunning())
    {
        out << "nothing to push to\n";
        return 2;
    }

    // ---- what the dev machine holds ----
    QString devTemplates = QDir(sandbox.path()).filePath("dev");
    writeFile(QDir(devTemplates).filePath("SEVILLE/calendar.html"), "<h1>one</h1>");
    writeFile(QDir(devTemplates).filePath("SEVILLE/css/site.css"), "body{}");

    // Never sent, and refused if it were. This is the operator's own file.
    writeFile(QDir(devTemplates).filePath("SEVILLE/project.js"), "var apiKey='secret';");

    PushWindow window;

    QLineEdit* source = nullptr;
    foreach (QLineEdit* edit, window.findChildren<QLineEdit*>())
    {
        if (edit->placeholderText().contains("templates", Qt::CaseInsensitive))
            source = edit;
    }

    QListWidget* packs = window.findChildren<QListWidget*>().value(0);
    QTableWidget* targets = tableWithColumns(&window, 4);
    QTableWidget* files = tableWithColumns(&window, 6);

    expectTrue(source != nullptr, "the templates folder field was found");
    expectTrue(packs != nullptr, "the pack list was found");
    expectTrue(targets != nullptr, "the client table was found");
    expectTrue(files != nullptr, "the file table was found");

    if (!source || !packs || !targets || !files)
    {
        out << "the window is not laid out as this expects\n";
        return 2;
    }

    // ---- fill it in the way an operator would ----
    source->setText(devTemplates);
    emit source->editingFinished();
    pump(300);

    expectTrue(packs->count() == 1, "the pack was discovered in the folder");
    if (packs->count() > 0)
        packs->item(0)->setCheckState(Qt::Checked);

    if (targets->rowCount() == 0)
        buttonNamed(&window, "Add")->click();

    targets->item(0, 0)->setCheckState(Qt::Checked);
    targets->item(0, 1)->setText("Test client");
    targets->item(0, 2)->setText(QString("127.0.0.1:%1").arg(port));
    targets->item(0, 3)->setText(TOKEN);

    out << "Compare\n";

    clickAndWait(buttonNamed(&window, "Compare"), files);

    QStringList first = statesFrom(files);
    expectTrue(first.contains("SEVILLE/calendar.html=new"), "a file the client does not have is new");
    expectTrue(first.contains("SEVILLE/css/site.css=new"), "and so is a nested one");
    expectTrue(first.filter("project.js").isEmpty(), "project.js is never even offered");

    out << "\nPush\n";

    clickAndWait(buttonNamed(&window, "Push ticked"), files);

    expectTrue(readFile(QDir(clientTemplates).filePath("SEVILLE/calendar.html")) == QByteArray("<h1>one</h1>"),
               "the file arrived on the client with the right bytes");
    expectTrue(readFile(QDir(clientTemplates).filePath("SEVILLE/css/site.css")) == QByteArray("body{}"),
               "so did the nested one");
    expectTrue(!QFile::exists(QDir(clientTemplates).filePath("SEVILLE/project.js")),
               "and the operator's own project.js was not sent");

    QStringList afterPush = statesFrom(files);
    expectTrue(afterPush.contains("SEVILLE/calendar.html=sent"), "the row says it was sent");

    out << "\nCompare again\n";

    clickAndWait(buttonNamed(&window, "Compare"), files);

    QStringList second = statesFrom(files);
    expectTrue(second.contains("SEVILLE/calendar.html=unchanged"),
               "a file that matches is listed as unchanged rather than resent");
    expectTrue(second.contains("SEVILLE/css/site.css=unchanged"), "and so is the nested one");

    out << "\nAfter an edit on the dev machine\n";

    writeFile(QDir(devTemplates).filePath("SEVILLE/calendar.html"), "<h1>two</h1>");

    clickAndWait(buttonNamed(&window, "Compare"), files);

    QStringList third = statesFrom(files);
    expectTrue(third.contains("SEVILLE/calendar.html=changed"), "an edited file is listed as changed");
    expectTrue(third.contains("SEVILLE/css/site.css=unchanged"), "and the one beside it is not");

    clickAndWait(buttonNamed(&window, "Push ticked"), files);
    expectTrue(readFile(QDir(clientTemplates).filePath("SEVILLE/calendar.html")) == QByteArray("<h1>two</h1>"),
               "and pushing it replaces the old bytes");

    out << "\nWith the wrong token\n";

    targets->item(0, 3)->setText("not-the-token");
    clickAndWait(buttonNamed(&window, "Compare"), files);
    expectTrue(files->rowCount() == 0, "a refused compare lists nothing rather than guessing");

    // What was already installed is untouched by a failed attempt.
    expectTrue(readFile(QDir(clientTemplates).filePath("SEVILLE/calendar.html")) == QByteArray("<h1>two</h1>"),
               "and the client keeps what it already had");

    SheetCacheServer::getInstance().stop();

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    out.flush();

    return failures == 0 ? 0 : 1;
}

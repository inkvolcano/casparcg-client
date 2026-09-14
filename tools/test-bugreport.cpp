// What a bug report contains and, above all, what it must never contain. The
// database copy in a report would carry every token and key this client holds
// if the redaction rule missed one; these check the rule on names, the SQL
// that applies it, and the way files and folders are picked.

#include "../src/Common/BugReportRules.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QTextStream>

static int checks = 0;
static int failures = 0;

static void expectTrue(bool actual, const QString& what)
{
    checks++;
    if (actual)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n";
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    out << "Secrets\n";

    // Every secret setting this client stores today. If one is added and missed
    // here, it is missed in the report too.
    foreach (const QString& name, QStringList()
             << "RelayToken" << "RelayCheckInToken" << "RelayPushToken" << "UpdateToken"
             << "TemplatePushToken" << "SheetsApiKey" << "NdiPassword" << "RelaySecret")
        expectTrue(BugReportRules::isSecretSetting(name), name + " is a secret");

    foreach (const QString& name, QStringList()
             << "HotkeyPreview" << "MeterPeakHold" << "RelayUrl" << "RelayPacks" << "PreviewLegacyMode"
             << "SheetsCacheUrl" << "RundownRepository" << "FontSize")
        expectTrue(!BugReportRules::isSecretSetting(name), name + " is not");

    const QStringList sql = BugReportRules::redactionStatements();
    expectTrue(sql.count() == 2, "two statements: settings and devices");
    expectTrue(sql.at(0).contains("Configuration") && sql.at(0).contains("<redacted>"), "settings are overwritten, not deleted");
    foreach (const QString& word, QStringList() << "%Token%" << "%ApiKey%" << "%Password%" << "%Secret%")
        expectTrue(sql.at(0).contains(word), "the SQL matches " + word + ", the same as the rule");
    expectTrue(sql.at(1).contains("Device") && sql.at(1).contains("Password"), "device passwords are cleared");

    out << "\nFiles and folders\n";

    const QStringList logs = BugReportRules::logFileNames(QDate(2026, 9, 15));
    expectTrue(logs == (QStringList() << "Client_2026-09-15.log" << "Client_2026-09-14.log"),
               "today's log and yesterday's: " + logs.join(", "));
    expectTrue(BugReportRules::logFileNames(QDate(2026, 1, 1)).at(1) == "Client_2025-12-31.log",
               "yesterday crosses the year");
    expectTrue(BugReportRules::reportFolderName(QDateTime(QDate(2026, 9, 15), QTime(9, 5))) == "casparcg-bugreport-20260915-0905",
               "the folder is named by the minute");
    expectTrue(BugReportRules::isClientCrashFolder("AppCrash_casparcg-client._61fc5d42"), "the client's WER folder is recognised");
    expectTrue(!BugReportRules::isClientCrashFolder("AppCrash_casparcg.exe_1234"), "the server's is not");
    expectTrue(!BugReportRules::isClientCrashFolder("Kernel_0_cab_1"), "nor anything else");
    expectTrue(BugReportRules::isClientEvent("Faulting application name: casparcg-client.exe"), "a client fault is ours");
    expectTrue(!BugReportRules::isClientEvent("Faulting application name: chrome.exe"), "another program's is not");

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}

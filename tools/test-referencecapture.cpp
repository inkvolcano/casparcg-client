// "From Preview": NDI or a server still, by the setting and what is there.

#include "../src/Common/ReferenceCaptureRules.h"

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

using namespace ReferenceCaptureRules;

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream out(stdout);
    out << "Reference capture\n";

    expectTrue(settingFrom("") == Setting::Automatic && settingFrom("ndi") == Setting::Ndi && settingFrom("server") == Setting::Server,
               "the setting reads back, Automatic when unset");
    expectTrue(settingFrom(settingText(Setting::Server)) == Setting::Server, "and round-trips");

    Situation ndi;
    ndi.channel = 2;
    ndi.server = "Main";
    ndi.configRead = true;
    ndi.ndiDeclared = true;
    ndi.ndiName = "Channel 2";
    ndi.ndiAvailable = true;
    ndi.ndiSource = "PLAYOUT-1 (Channel 2)";
    ndi.mediaFolderSet = true;

    expectTrue(plan(Setting::Automatic, ndi).route == Route::Ndi, "Automatic takes NDI when the channel has an output and it is found");

    Situation noNdi = ndi;
    noNdi.ndiDeclared = false;
    noNdi.ndiSource.clear();
    expectTrue(plan(Setting::Automatic, noNdi).route == Route::Server, "no NDI output in the config: the server still");

    Situation remote = noNdi;
    remote.configRead = false;
    expectTrue(plan(Setting::Automatic, remote).route == Route::Server, "a config that cannot be read counts as no NDI output");

    Situation nothing = noNdi;
    nothing.mediaFolderSet = false;
    const Plan none = plan(Setting::Automatic, nothing);
    expectTrue(none.route == Route::None && none.why.contains("no NDI output") && none.why.contains("no media folder"),
               "neither: nothing, and both reasons said: " + none.why);

    Situation safe = ndi;
    safe.ndiAvailable = false;
    expectTrue(plan(Setting::Ndi, safe).route == Route::None && plan(Setting::Ndi, safe).why.contains("NDI is not available"),
               "NDI only, with NDI off: nothing, and why");
    expectTrue(plan(Setting::Automatic, safe).route == Route::Server, "Automatic then uses the server still");
    expectTrue(plan(Setting::Server, ndi).route == Route::Server, "Server only uses the still even with NDI there");
    expectTrue(plan(Setting::Server, nothing).route == Route::None, "and does nothing without a media folder");

    const QStringList sources = QStringList() << "PLAYOUT-1 (Channel 1)" << "PLAYOUT-1 (channel 2)" << "CAMERA-PC (Cam 1)";
    expectTrue(findSource(sources, "Channel 2") == "PLAYOUT-1 (channel 2)", "the source is found by its name in brackets, any case");
    expectTrue(findSource(sources, "Channel 3").isEmpty(), "and not when it is not there");
    expectTrue(findSource(QStringList() << "PLAYOUT-1 (CasparCG 1)" << "CAMERA-PC (Cam 1)", "") == "PLAYOUT-1 (CasparCG 1)",
               "an unnamed output is used when it is the only CasparCG one");
    expectTrue(findSource(QStringList() << "PLAYOUT-1 (CasparCG 1)" << "PLAYOUT-2 (CasparCG)", "").isEmpty(),
               "but not when there are two to choose from");

    expectTrue(stillCommand(2, stillName(42)) == "ADD 2 IMAGE casparcg-client-reference-42", "the server is asked for a still");

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}

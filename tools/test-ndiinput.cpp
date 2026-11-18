// The NDI Input item: what it sends the server, and how the server's NDI LIST
// reply is read. The command shapes were checked against a CasparCG 2.5 server
// in build 321 (INFO showed the source, MIX 50 and low_bandwidth as sent).

#include "../src/Common/NdiInputRules.h"

#include <QtCore/QTextStream>

static int checks = 0;
static int failures = 0;

static void expect(const QString& actual, const QString& expected, const QString& what)
{
    checks++;
    if (actual == expected)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n        got      " << actual << "\n        expected " << expected << "\n";
}

static void expectTrue(bool actual, const QString& what)
{
    checks++;
    if (actual)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n";
}

using namespace NdiInputRules;

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream out(stdout);
    out << "NDI Input\n";

    expect(command("PLAY", 2, 10, "STUDIO-PC (Camera 1)", "MIX", 20, "Linear", "RIGHT", false),
           "PLAY 2-10 [NDI] \"STUDIO-PC (Camera 1)\" MIX 20 Linear RIGHT", "play with a mix");
    expect(command("LOADBG", 1, 20, "STUDIO-PC (Camera 1)", "CUT", 1, "Linear", "RIGHT", true),
           "LOADBG 1-20 [NDI] \"STUDIO-PC (Camera 1)\" CUT 1 Linear RIGHT LOW_BANDWIDTH", "load, low bandwidth last");
    expect(command("PLAY", 1, 10, "  PC (Cam)  ", "", 0, "", "", false),
           "PLAY 1-10 [NDI] \"PC (Cam)\"", "no transition: nothing after the name; spaces around the name dropped");
    expect(command("PLAY", 1, 10, "PC (say \"hi\")", "", 0, "", "", false),
           "PLAY 1-10 [NDI] \"PC (say \\\"hi\\\")\"", "a quote inside the name is escaped");
    expect(quoted("A\\B"), "\"A\\\\B\"", "a backslash inside the name is escaped");

    // The reply exactly as the 2.5 server sent it.
    const QString reply = "200 NDI LIST OK\r\n"
                          "1 \"DESKTOP-16SHA93 (GFX channel 1)\" 169.254.157.11:5961\r\n"
                          "2 \"DESKTOP-16SHA93 (GFX channel 2)\" 169.254.157.11:5962\r\n"
                          "3 \"DESKTOP-16SHA93 (Remote Connection 1)\" ndi-app:DESKTOP-16SHA93%20(Remote%20Connection%201)\r\n"
                          "\r\n";
    const QStringList names = parseList(reply);
    expectTrue(names.size() == 3, "three sources");
    expect(names.value(0), "DESKTOP-16SHA93 (GFX channel 1)", "first source, without number or address");
    expect(names.value(2), "DESKTOP-16SHA93 (Remote Connection 1)", "an app source without an IP address");
    expectTrue(replyComplete(reply), "a list ends with an empty line");
    expectTrue(!replyComplete("200 NDI LIST OK\r\n1 \"A (B)\" 1.2.3.4:5\r\n"), "not complete before the empty line");
    expectTrue(replyComplete("501 NDI LIST FAILED\r\n"), "a refusal is one line");
    expectTrue(parseList("501 NDI LIST FAILED\r\n").isEmpty(), "a refusal lists nothing");
    expectTrue(parseList("200 NDI LIST OK\r\n\r\n").isEmpty(), "no sources on the network");
    expectTrue(parseList("200 NDI LIST OK\r\n1 \"A (B)\" x\r\n2 \"A (B)\" y\r\n\r\n").size() == 1, "a name listed twice is offered once");
    expectTrue(parseList("200 NDI LIST OK\r\nnoise \"A (B)\" x\r\n\r\n").isEmpty(), "a line without a number is not a source");
    expectTrue(parseList("200 NDI LIST OK\r\n1 \"\" x\r\n\r\n").isEmpty(), "an empty name is not a source");

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}

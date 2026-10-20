// casparcg.config editing: reading and writing it back without loss, changing
// settings by path, and predicting what stops the server or skips an output.

#include "../src/Common/ServerConfigSchema.h"

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

using namespace ServerConfigXml;
using namespace ServerConfigSchema;

static bool hasProblem(const QList<Problem>& problems, const QString& part, bool stops)
{
    for (const Problem& problem : problems)
    {
        if (problem.text.contains(part) && problem.stopsServer == stops)
            return true;
    }
    return false;
}

static QString describe(const QList<Problem>& problems)
{
    QStringList lines;
    for (const Problem& problem : problems)
        lines << QString("%1%2: %3").arg(problem.stopsServer ? "STOP " : "", problem.where, problem.text);
    return lines.join(" | ");
}

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream out(stdout);
    out << "Server config\n";

    // The shape of the config shipped with the server: settings, then the
    // reference of every option in a comment after the root element.
    const QString shipped =
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n\n<configuration>\n    <paths>\n        <media-path>media/</media-path>\n"
        "        <log-path disable=\"false\">log/</log-path>\n        <data-path>data/</data-path>\n        <template-path>template/</template-path>\n"
        "    </paths>\n    <lock-clear-phrase>secret</lock-clear-phrase>\n    <channels>\n        <channel>\n            <video-mode>720p5000</video-mode>\n"
        "            <consumers>\n                <screen />\n                <system-audio />\n            </consumers>\n        </channel>\n    </channels>\n"
        "    <controllers>\n        <tcp>\n            <port>5250</port>\n            <protocol>AMCP</protocol>\n        </tcp>\n    </controllers>\n"
        "    <amcp>\n        <media-server>\n            <host>localhost</host>\n            <port>8000</port>\n        </media-server>\n    </amcp>\n"
        "</configuration>\n\n<!--\n<log-level> info  [trace|debug|info|warning|error|fatal]</log-level>\n<channels><channel><video-mode>PAL</video-mode></channel></channels>\n-->\n";

    Document document = parse(shipped);
    expectTrue(document.ok, "the shipped config reads: " + document.error);
    expectTrue(document.after.count() == 1 && document.after.first()->text.contains("<log-level> info"), "the reference comment after the root is kept");
    expectTrue(value(document.root, "channels/channel/video-mode") == "720p5000", "a value by path");
    expectTrue(elements(find(document.root, "channels/channel/consumers")).count() == 2, "both consumers");
    expectTrue(attribute(find(document.root, "paths/log-path"), "disable") == "false", "an attribute");

    const QString written = write(document);
    const Document again = parse(written);
    expectTrue(again.ok && write(again) == written, "writing is stable: written once, it reads back to the same text");
    expectTrue(written.contains("<screen />") && written.contains("<media-path>media/</media-path>"), "empty and leaf elements written as they were");
    expectTrue(written.indexOf("</configuration>") < written.indexOf("<!--"), "the reference comment stays after the root");

    // This machine's own shape: tabs, NDI outputs, comments inside a channel.
    const QString local =
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<configuration>\n    <paths>\n        <media-path>C:/CasparCG/media/</media-path>\n"
        "        <log-path>log/</log-path>\n        <data-path>data/</data-path>\n        <template-path>C:/CasparCG/templates/</template-path>\n"
        "    </paths>\n       <channels>\n        <channel>\n            <video-mode>1080p2500</video-mode>\n            <consumers>\n"
        "\t\t\t\t  <ndi>\n                <name>Channel 1</name>\n                <allow-fields>true</allow-fields>\n            </ndi>\n"
        "                <!-- <decklink><device>1</device></decklink> -->\n                <screen />\n            </consumers>\n        </channel>\n"
        "        <channel>\n            <video-mode>1080p2500</video-mode>\n            <future-setting a=\"1\">x &amp; y</future-setting>\n"
        "        </channel>\n    </channels>\n    <controllers><tcp><port>5250</port><protocol>AMCP</protocol></tcp></controllers>\n</configuration>\n";
    Document mine = parse(local);
    expectTrue(mine.ok, "a hand-edited config reads");
    const QString mineWritten = write(mine);
    expectTrue(mineWritten.contains("<!-- <decklink><device>1</device></decklink> -->"), "a commented-out output is kept where it was");
    expectTrue(mineWritten.contains("<future-setting a=\"1\">x &amp; y</future-setting>"), "an element no editor knows is kept, escaped");
    expectTrue(value(find(mine.root, "channels"), "channel/consumers/ndi/name") == "Channel 1", "values inside an indented mess");

    // Setting by path.
    NodePtr decklink = makeElement("decklink");
    setValue(decklink, "subregion/src-x", "100");
    expectTrue(value(decklink, "subregion/src-x") == "100", "a value in a group creates the group");
    setValue(decklink, "subregion/src-x", "");
    expectTrue(child(decklink, "subregion") == nullptr, "clearing the last value of a group removes the group");
    setValue(decklink, "vanc/op47-line", "12", true);
    setValue(decklink, "vanc/op47-line", "", true);
    expectTrue(child(decklink, "vanc") != nullptr, "VANC stays on when its last line is cleared: <vanc/> is the setting");

    NodePtr root = document.root;
    setValue(root, "paths/log-path/@disabled", "true");
    expectTrue(attribute(find(root, "paths/log-path"), "disabled") == "true", "an attribute by path");
    setValue(root, "paths/log-path/@disabled", "");
    expectTrue(find(root, "paths/log-path") != nullptr && !hasAttribute(find(root, "paths/log-path"), "disabled"),
               "clearing an attribute keeps an element that still has a value");
    setValue(root, "log-level", "  debug ");
    expectTrue(value(root, "log-level") == "debug", "values are trimmed");
    setValue(root, "log-level", "");
    expectTrue(child(root, "log-level") == nullptr, "clearing a value leaves it out, never empty");

    NodePtr producer = makeElement("producer");
    setValue(producer, "@id", "10");
    setValue(producer, ".", "AMB LOOP");
    expectTrue(write(Document{true, QString(), 0, {}, producer, {}}).contains("<producer id=\"10\">AMB LOOP</producer>"), "a producer's layer and text");

    // Moving channels, past a comment.
    const Document ordered = parse("<configuration><channels><channel><video-mode>PAL</video-mode></channel><!-- note -->"
                                   "<channel><video-mode>NTSC</video-mode></channel></channels></configuration>");
    NodePtr channels = child(ordered.root, "channels");
    expectTrue(move(elements(channels).first(), 1), "a channel moves down");
    expectTrue(value(elements(channels).first(), "video-mode") == "NTSC", "and the other comes first");
    expectTrue(!move(elements(channels).first(), -1), "nothing moves above the first");

    // Versions.
    expectTrue(versionFromText("2.5.0 7a5e4e2 Stable") == Version::V25, "2.5 from what the server reports");
    expectTrue(versionFromText("2.4.3 fd9168c Stable") == Version::V24 && versionFromText("2.3.3 LTS") == Version::V24, "2.4 and older");
    expectTrue(versionFromText("2.6.0 fe19ac0 Dev") == Version::Master && versionFromText("CasparCG Server master 2026-10-15") == Version::Master
                   && versionFromText("") == Version::Master, "newer, master and unknown read as master");

    // What the server makes of it.
    const Document starter = parse(starterConfig());
    QList<Problem> problems = check(starter, Version::V25);
    expectTrue(starter.ok && problems.isEmpty(), "the starter config has nothing wrong: " + describe(problems));

    problems = check(parse(shipped), Version::V25);
    expectTrue(hasProblem(problems, "disabled=", false), "the shipped example's disable=\"false\" is pointed out: " + describe(problems));

    auto withChannel = [](const QString& channel) {
        QString text = starterConfig();
        text.replace("            <video-mode>1080i5000</video-mode>\n", channel);
        return parse(text);
    };

    problems = check(withChannel("<video-mode>dci1080p2500</video-mode>"), Version::V25);
    expectTrue(hasProblem(problems, "4kDCIp2500", true), "a dci mode from the shipped example stops the server: " + describe(problems));
    problems = check(withChannel("<video-mode>1080I5000</video-mode>"), Version::V25);
    expectTrue(problems.isEmpty(), "a mode in other case is accepted, as the server does");
    problems = check(withChannel("<video-mode />"), Version::V25);
    expectTrue(hasProblem(problems, "empty", true), "an empty video mode stops the server");
    problems = check(withChannel("<video-mode>PAL</video-mode><color-depth>12</color-depth>"), Version::V25);
    expectTrue(hasProblem(problems, "Colour depth", true), "colour depth other than 8 or 16 stops the server");
    problems = check(withChannel("<video-mode>PAL</video-mode><color-depth>16</color-depth>"), Version::V24);
    expectTrue(hasProblem(problems, "a 2.4 server ignores it", false), "a 2.5 setting on a 2.4 server is pointed out as ignored");

    problems = check(withChannel("<video-mode>PAL</video-mode><producers><producer>AMB LOOP</producer></producers>"), Version::V25);
    expectTrue(hasProblem(problems, "no layer", true), "a producer without a layer stops the server");

    problems = check(withChannel("<video-mode>PAL</video-mode><consumers><ndi /><ndi /><Screen /></consumers>"), Version::V25);
    expectTrue(hasProblem(problems, "replaces the first", false), "a second NDI output replaces the first");
    expectTrue(hasProblem(problems, "<Screen> is not an output", false), "consumer names are case-sensitive");

    problems = check(withChannel("<video-mode>PAL</video-mode><consumers><screen><device>1</device></screen>"
                                 "<screen><device>1</device><key-only>true</key-only></screen></consumers>"), Version::V25);
    expectTrue(problems.isEmpty(), "fill and key on one monitor are two outputs: " + describe(problems));

    problems = check(withChannel("<video-mode>PAL</video-mode><consumers><artnet><fixtures><fixture><type>rgbw</type>"
                                 "<start-address>500</start-address><fixture-count>4</fixture-count></fixture></fixtures></artnet></consumers>"), Version::V25);
    expectTrue(hasProblem(problems, "past 512", true), "Art-Net fixtures past DMX 512 are refused: " + describe(problems));
    problems = check(withChannel("<video-mode>PAL</video-mode><consumers><artnet><host>lights.local</host></artnet></consumers>"), Version::V25);
    expectTrue(hasProblem(problems, "<fixtures>", false) && hasProblem(problems, "not an IPv4 address", false),
               "an Art-Net output without fixtures, or with a host name, is skipped");

    problems = check(withChannel("<video-mode>PAL</video-mode><color-depth>16</color-depth><consumers><ndi /></consumers>"), Version::V25);
    expectTrue(hasProblem(problems, "8 bit only", false), "NDI on a 16-bit channel is skipped");

    QString text = starterConfig();
    text.replace("<protocol>AMCP</protocol>", "<protocol>AMCP</protocol><host>localhost</host>");
    problems = check(parse(text), Version::Master);
    expectTrue(hasProblem(problems, "not an IPv4 address", true), "a host name to listen on stops a master server");

    text = starterConfig();
    text.replace("<controllers>", "<accelerator>metal</accelerator><controllers>");
    expectTrue(hasProblem(check(parse(text), Version::Master), "Accelerator", true), "an unknown accelerator stops a master server");
    expectTrue(hasProblem(check(parse(text), Version::V25), "is read from version 2.6", false), "and is ignored by 2.5");

    text = starterConfig();
    text.replace("    <controllers>\n        <tcp>\n            <port>5250</port>\n            <protocol>AMCP</protocol>\n        </tcp>\n    </controllers>\n", "");
    expectTrue(hasProblem(check(parse(text), Version::V25), "no <controllers>", true), "no controllers stops the server");

    text = starterConfig();
    text.replace("<log-path>log/</log-path>", "");
    expectTrue(hasProblem(check(parse(text), Version::V25), "no <log-path>", true), "no log path stops the server");

    expectTrue(hasProblem(check(parse("<configuration><paths>"), Version::V25), "Not readable XML", true), "broken XML is said to be broken");

    // New items.
    Document fresh = parse(starterConfig());
    NodePtr channel = elements(child(fresh.root, "channels")).first();
    NodePtr p1 = newItem("producer", fresh.root, channel);
    appendChild(ensure(channel, "producers"), p1);
    NodePtr p2 = newItem("producer", fresh.root, channel);
    expectTrue(attribute(p1, "id") == "10" && attribute(p2, "id") == "20", "new producers get free layers");
    expectTrue(value(newItem("tcp", fresh.root), "port") == "5251", "a new AMCP controller gets a free port");
    appendChild(child(fresh.root, "channels"), newItem("channel", fresh.root));
    expectTrue(value(elements(child(fresh.root, "channels")).last(), "video-mode") == "1080i5000", "a new channel takes the last channel's mode");
    expectTrue(value(newItem("ndi", fresh.root, channel), "name") == "Channel 1", "a new NDI output is named after its channel");
    expectTrue(child(newItem("artnet", fresh.root), "fixtures") != nullptr, "a new Art-Net output has the <fixtures> it needs");

    expectTrue(videoModes().count() == 41 && !videoModes().contains("dci1080p2500"), "the 41 modes the server has, no dci ones");

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}

// The command that asks a server to send its output to the Live panel.
//
// The client decodes nothing here. It tells the *server* to encode a UDP mpegts
// stream and then plays that, so the whole cost of the Live panel lands on the
// playout machine, and this one string decides it.
//
// Two things are open upstream about it. #271 (2019): turning Live on costs about
// 40% of an i7, because it is libx264 encoding every frame at the channel's rate
// to make a 288-pixel-wide thumbnail. #316 (2024): against a newer server the
// stream never arrives, and VLC on the same machine cannot open it either — which
// means it was never produced properly, which points at the options.
//
// Neither can be fixed from here, because both need a real server to test on and
// guessing at ffmpeg options is how you break the people it currently works for.
// So the string became editable, and the single most important property of that
// change is that it altered nothing for anyone: with no override, the bytes on
// the wire are identical to what this client has always sent. That is what the
// first test pins, character for character.

#include "../src/Common/StreamCommand.h"

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QTextStream>

static int failures = 0;
static int checks = 0;

static void expectTrue(bool actual, const QString& what)
{
    checks++;
    if (actual)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n";
}

static void same(const QString& actual, const QString& wanted, const QString& what)
{
    checks++;
    if (actual == wanted)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what << "\n         wanted: " << wanted
                        << "\n            got: " << actual << "\n";
}

static void theOriginalIsKeptExactly()
{
    // Character for character, from CasparDevice::startStream before this was
    // made configurable — including the double space, which is in the original.
    const QString sized = StreamCommand::originalParameters(23, false, 288, 162);
    same(sized,
         "-format mpegts -codec:v libx264 -crf:v 23 -tune:v zerolatency "
         "-preset:v ultrafast -filter:v scale=288:162  "
         "-filter:a \"pan=stereo|c0=FL|c1=FR\"",
         "the sized form is unchanged");

    const QString full = StreamCommand::originalParameters(23, false, 0, 0);
    same(full,
         "-format mpegts -codec:v libx264 -crf:v 23 -tune:v zerolatency "
         "-preset:v ultrafast   -filter:a \"pan=stereo|c0=FL|c1=FR\"",
         "the full-size form is unchanged");
}

static void theDefaultIsTheMeasuredOne()
{
    // Build 318: at most 25 frames a second, a cheap scaler; the window 1280 wide.
    same(StreamCommand::defaultParameters(23, false, 288, 162),
         "-format mpegts -codec:v libx264 -crf:v 23 -tune:v zerolatency -preset:v ultrafast "
         "-x264-params:v repeat-headers=1 -g:v 25 "
         "-filter:v fps=25,scale=288:162:flags=fast_bilinear,format=yuv420p -filter:a \"pan=stereo|c0=FL|c1=FR\"",
         "the panel: 25 fps, fast_bilinear, its own size, 4:2:0 with repeated headers");
    same(StreamCommand::defaultParameters(23, false, 0, 0),
         "-format mpegts -codec:v libx264 -crf:v 23 -tune:v zerolatency -preset:v ultrafast "
         "-x264-params:v repeat-headers=1 -g:v 25 "
         "-filter:v fps=25,scale=1280:-2:flags=fast_bilinear,format=yuv420p -filter:a \"pan=stereo|c0=FL|c1=FR\"",
         "the window: 1280 wide, the channel's shape kept");

    // The Original preset, expanded, is the original string (bar the double space).
    const QList<StreamCommand::Preset> presets = StreamCommand::presets();
    bool found = false;
    for (const StreamCommand::Preset& preset : presets)
    {
        if (preset.name.startsWith("Original"))
        {
            found = true;
            same(StreamCommand::parametersFor(preset.parameters, 23, false, 288, 162),
                 StreamCommand::originalParameters(23, false, 288, 162).replace("  ", " "),
                 "the Original preset sends the original string");
        }
        expectTrue(!preset.parameters.contains(" -an"), "no preset drops the audio: on a 2.5 server that stream never arrives");
    }
    expectTrue(found, "the original string is offered as a preset");
}

static void theKeyChannelFormIsUnchangedToo()
{
    const QString sizedKey = StreamCommand::defaultParameters(23, true, 288, 162);
    expectTrue(sizedKey.contains("fps=25,alphaextract,format=pix_fmts=yuv422p,scale=288:162"),
               "a key channel still extracts alpha before scaling");

    const QString fullKey = StreamCommand::defaultParameters(23, true, 0, 0);
    expectTrue(fullKey.contains("fps=25,alphaextract,format=pix_fmts=yuv422p,scale=1280:-2"),
               "and in the window it extracts alpha too");

    expectTrue(StreamCommand::originalParameters(23, true, 0, 0).contains("-filter:v alphaextract"),
               "the original full-size key form is kept");
}

static void nothingOverriddenMeansTheDefault()
{
    // The property the whole change rests on: an untouched client sends the same
    // bytes it always did.
    same(StreamCommand::parametersFor("", 23, false, 288, 162),
         StreamCommand::defaultParameters(23, false, 288, 162),
         "an empty override is the default");
    same(StreamCommand::parametersFor("   ", 23, false, 288, 162),
         StreamCommand::defaultParameters(23, false, 288, 162),
         "and so is one of only spaces");
}

static void anOverrideReplacesEverythingAfterTheUrl()
{
    same(StreamCommand::parametersFor("-format mpegts -codec:v mjpeg", 23, false, 288, 162),
         "-format mpegts -codec:v mjpeg",
         "an override is used as written");

    // Not merged with the default: half of one set of ffmpeg options and half of
    // another is a stream nobody asked for.
    expectTrue(!StreamCommand::parametersFor("-codec:v mjpeg", 23, false, 288, 162).contains("libx264"),
               "and nothing of the default survives it");
}

static void thePlaceholdersFollowTheSettings()
{
    // A preset that hard-coded the size would ignore the panel's own quality and
    // compact-size settings, which is the difference between a preset and a
    // one-off somebody has to edit twice.
    const QString expanded = StreamCommand::parametersFor(
        "-crf:v {quality} -filter:v {key}scale={width}:{height}", 18, false, 320, 180);

    same(expanded, "-crf:v 18 -filter:v scale=320:180", "quality and size are filled in");

    const QString keyed = StreamCommand::parametersFor(
        "-filter:v {key}scale={width}:{height}", 18, true, 320, 180);
    same(keyed, "-filter:v alphaextract,format=pix_fmts=yuv422p,scale=320:180",
         "and the key filter appears only for a key channel");

    // The same preset on a non-key channel leaves nothing behind.
    expectTrue(!StreamCommand::parametersFor("-filter:v {key}scale=1:1", 18, false, 1, 1)
                   .contains("alphaextract"),
               "and vanishes cleanly when there is no key");
}

static void theCommandKeepsItsShape()
{
    // The server substitutes the client's own address for this placeholder, so it
    // has to be sent literally — replacing it here would send the stream nowhere.
    const QString add = StreamCommand::buildAdd(1, 9250, "-format mpegts");
    same(add, "ADD 1 STREAM udp://<client_ip_address>:9250?pkt_size=1316 -format mpegts",
         "the add command has its placeholder, its port and whole-packet datagrams");

    const QString remove = StreamCommand::buildRemove(1, 9250);
    same(remove, "REMOVE 1 STREAM udp://<client_ip_address>:9250?pkt_size=1316",
         "and the remove command names the same address");

    expectTrue(add.startsWith("ADD 1 STREAM udp://<client_ip_address>:9250"),
               "the address placeholder is never expanded here");
}

static void everyPresetIsUsable()
{
    const QList<StreamCommand::Preset> presets = StreamCommand::presets();

    expectTrue(presets.size() >= 3, "there is more than one thing to try");

    // The first is the shipped default, and it must be empty — meaning "use the
    // built-in string" — rather than a copy of it that could drift.
    expectTrue(presets.first().parameters.isEmpty(),
               "the first preset restores the default by being empty");

    foreach (const StreamCommand::Preset& preset, presets)
    {
        expectTrue(!preset.name.isEmpty(), "every preset has a name");
        expectTrue(!preset.note.isEmpty(),
                   QString("\"%1\" says what it trades").arg(preset.name));

        if (preset.parameters.isEmpty())
            continue;

        // A preset that named no muxer or no encoder would not produce a stream.
        expectTrue(preset.parameters.contains("-format mpegts"),
                   QString("\"%1\" still asks for mpegts").arg(preset.name));
        expectTrue(preset.parameters.contains("-codec:v"),
                   QString("\"%1\" still names a video encoder").arg(preset.name));

        // Every placeholder must be one expand() knows, or it would reach the
        // server as literal text.
        QString expanded = StreamCommand::parametersFor(preset.parameters, 23, false, 288, 162);
        expectTrue(!expanded.contains('{') && !expanded.contains('}'),
                   QString("\"%1\" leaves no placeholder behind").arg(preset.name));
    }
}

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream(stdout) << "Live stream command\n";

    theOriginalIsKeptExactly();
    theDefaultIsTheMeasuredOne();
    theKeyChannelFormIsUnchangedToo();
    nothingOverriddenMeansTheDefault();
    anOverrideReplacesEverythingAfterTheUrl();
    thePlaceholdersFollowTheSettings();
    theCommandKeepsItsShape();
    everyPresetIsUsable();

    QTextStream(stdout) << "\n" << (checks - failures) << " passed, " << failures << " failed\n";

    return failures == 0 ? 0 : 1;
}

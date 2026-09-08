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

static void theDefaultIsExactlyWhatWasAlwaysSent()
{
    // Character for character, from CasparDevice::startStream before this was
    // made configurable — including the double space, which is in the original.
    const QString sized = StreamCommand::defaultParameters(23, false, 288, 162);
    same(sized,
         "-format mpegts -codec:v libx264 -crf:v 23 -tune:v zerolatency "
         "-preset:v ultrafast -filter:v scale=288:162  "
         "-filter:a \"pan=stereo|c0=FL|c1=FR\"",
         "the sized form is unchanged");

    const QString full = StreamCommand::defaultParameters(23, false, 0, 0);
    same(full,
         "-format mpegts -codec:v libx264 -crf:v 23 -tune:v zerolatency "
         "-preset:v ultrafast   -filter:a \"pan=stereo|c0=FL|c1=FR\"",
         "the full-size form is unchanged");
}

static void theKeyChannelFormIsUnchangedToo()
{
    const QString sizedKey = StreamCommand::defaultParameters(23, true, 288, 162);
    expectTrue(sizedKey.contains("alphaextract,format=pix_fmts=yuv422p,scale=288:162"),
               "a key channel still extracts alpha before scaling");

    const QString fullKey = StreamCommand::defaultParameters(23, true, 0, 0);
    expectTrue(fullKey.contains("-filter:v alphaextract"),
               "and at full size it still extracts alpha");
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
    same(add, "ADD 1 STREAM udp://<client_ip_address>:9250 -format mpegts",
         "the add command has its placeholder and its port");

    const QString remove = StreamCommand::buildRemove(1, 9250);
    same(remove, "REMOVE 1 STREAM udp://<client_ip_address>:9250",
         "and the remove command matches it");

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

    theDefaultIsExactlyWhatWasAlwaysSent();
    theKeyChannelFormIsUnchangedToo();
    nothingOverriddenMeansTheDefault();
    anOverrideReplacesEverythingAfterTheUrl();
    thePlaceholdersFollowTheSettings();
    theCommandKeepsItsShape();
    everyPresetIsUsable();

    QTextStream(stdout) << "\n" << (checks - failures) << " passed, " << failures << " failed\n";

    return failures == 0 ? 0 : 1;
}

// Reading an EBU OGraf manifest.
//
// An OGraf graphic is a folder someone else built: the manifest, the module it
// names, and its resources. The client reads that manifest and then joins paths
// from it to a folder on this machine, so the parsing has to hold two lines at
// once — be liberal about what a valid manifest may contain, and refuse anything
// that could reach outside the graphic's own folder.
//
// The other half is what a graphic gets when nobody has typed anything. OGraf
// carries that as JSON Schema "default" values in the manifest, which is the
// standard version of the window.debugData convention this fork already reads
// for CasparCG templates. Getting the defaults right is the difference between a
// preview that shows a populated graphic and one that shows an empty box.
//
// The l3rd-name manifest used below is the real one from the EBU's own examples,
// not an invention.

#include "../src/Common/OgrafManifest.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QString>
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
    QTextStream(stdout) << "  FAIL  " << what
                        << "  (wanted \"" << wanted << "\", got \"" << actual << "\")\n";
}

static void sameInt(int actual, int wanted, const QString& what)
{
    checks++;
    if (actual == wanted)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what
                        << "  (wanted " << wanted << ", got " << actual << ")\n";
}

// The EBU's own l3rd-name example, trimmed to the fields that carry meaning.
static QByteArray realExample()
{
    return QByteArray(R"({
        "$schema": "https://ograf.ebu.io/v1/specification/json-schemas/graphics/schema.json",
        "name": "Lower 3rd - Name",
        "description": "Name lower third",
        "id": "l3rd-name",
        "main": "graphic.mjs",
        "thumbnails": [ { "file": "thumbnail.webp" } ],
        "version": "0",
        "customActions": [
            { "id": "highlight", "name": "Highlight", "schema": null }
        ],
        "supportsRealTime": true,
        "supportsNonRealTime": true,
        "schema": {
            "type": "object",
            "properties": {
                "name":  { "type": "string", "title": "Name",  "default": "John Doe" },
                "title": { "type": "string", "title": "Title", "default": "OGraf expert" }
            }
        }
    })");
}

static void aRealManifestReadsCorrectly()
{
    Ograf::Manifest manifest = Ograf::parse(realExample());

    expectTrue(manifest.valid, "the EBU's own example parses");
    same(manifest.id, "l3rd-name", "the id is read");
    same(manifest.name, "Lower 3rd - Name", "the name is read");
    same(manifest.main, "graphic.mjs", "the module to load is read");
    same(manifest.version, "0", "the version is read");
    expectTrue(manifest.supportsRealTime, "real-time support is read");
    expectTrue(manifest.supportsNonRealTime, "non-real-time support is read");

    sameInt(manifest.customActions.size(), 1, "the custom action is found");
    same(manifest.customActions.first().id, "highlight", "its id is read");
    same(manifest.customActions.first().name, "Highlight", "its name is read");

    sameInt(manifest.thumbnails.size(), 1, "the thumbnail is found");

    // Not declared in this manifest, and the spec's default is 1.
    sameInt(manifest.stepCount, 1, "an undeclared stepCount defaults to one");
}

static void defaultsComeOutOfTheSchema()
{
    Ograf::Manifest manifest = Ograf::parse(realExample());
    QJsonObject data = Ograf::defaultDataFor(manifest.schema);

    same(data.value("name").toString(), "John Doe", "a default is taken from the schema");
    same(data.value("title").toString(), "OGraf expert", "and so is the next one");
    sameInt(data.size(), 2, "and nothing else is invented");
}

static void anUndefaultedFieldIsLeftAlone()
{
    // The data is the graphic's own state model. A field its author chose not to
    // default is not ours to fill in: an empty string is a value, and putting one
    // there puts the graphic in a state nobody described.
    QByteArray json = R"({
        "id": "x", "main": "g.mjs",
        "schema": { "type": "object", "properties": {
            "withDefault": { "type": "string", "default": "yes" },
            "without":     { "type": "string" }
        } }
    })";

    QJsonObject data = Ograf::defaultDataFor(Ograf::parse(json).schema);

    expectTrue(data.contains("withDefault"), "the defaulted field is present");
    expectTrue(!data.contains("without"), "the undefaulted field is absent, not empty");
    sameInt(data.size(), 1, "exactly one field was produced");
}

static void everyTypeOfDefaultSurvives()
{
    QByteArray json = R"({
        "id": "x", "main": "g.mjs",
        "schema": { "type": "object", "properties": {
            "text":   { "type": "string",  "default": "hello" },
            "count":  { "type": "integer", "default": 7 },
            "ratio":  { "type": "number",  "default": 1.5 },
            "onAir":  { "type": "boolean", "default": true },
            "tags":   { "type": "array",   "default": ["a","b"] },
            "blank":  { "type": "string",  "default": "" },
            "zero":   { "type": "integer", "default": 0 },
            "off":    { "type": "boolean", "default": false }
        } }
    })";

    QJsonObject data = Ograf::defaultDataFor(Ograf::parse(json).schema);

    same(data.value("text").toString(), "hello", "a string default survives");
    sameInt(data.value("count").toInt(), 7, "an integer default survives");
    expectTrue(data.value("ratio").toDouble() == 1.5, "a number default survives");
    expectTrue(data.value("onAir").toBool(), "a boolean default survives");
    expectTrue(data.value("tags").isArray(), "an array default survives");

    // The falsy ones are the easy ones to lose: "" and 0 and false are all real
    // values a graphic may be relying on, and dropping them is a silent bug.
    expectTrue(data.contains("blank"), "an empty-string default is kept");
    expectTrue(data.contains("zero"), "a zero default is kept");
    expectTrue(data.contains("off"), "a false default is kept");
    sameInt(data.size(), 8, "all eight are present");
}

static void nestedObjectsAreWalked()
{
    QByteArray json = R"({
        "id": "x", "main": "g.mjs",
        "schema": { "type": "object", "properties": {
            "home": { "type": "object", "properties": {
                "name":  { "type": "string", "default": "Rovers" },
                "score": { "type": "integer", "default": 0 }
            } },
            "empty": { "type": "object", "properties": {
                "nothing": { "type": "string" }
            } }
        } }
    })";

    QJsonObject data = Ograf::defaultDataFor(Ograf::parse(json).schema);

    expectTrue(data.value("home").isObject(), "a nested object is produced");
    same(data.value("home").toObject().value("name").toString(), "Rovers", "its defaults are inside it");
    expectTrue(data.value("home").toObject().contains("score"), "including a zero");

    expectTrue(!data.contains("empty"), "an object with no defaults inside is not produced at all");
}

static void nothingCanClimbOutOfTheGraphicsFolder()
{
    // main and the thumbnails are joined to a folder on this machine. A manifest
    // is a file that arrives with a graphic from somewhere else, so these are
    // refused rather than sanitised — there is no legitimate reason for a
    // graphic's own module to live outside its own folder.
    const char* nasty[] = {
        R"({"id":"x","main":"../../../windows/system32/evil.js"})",
        R"({"id":"x","main":"/etc/passwd"})",
        R"({"id":"x","main":"C:/windows/evil.js"})",
        R"({"id":"x","main":"\\server\share\evil.js"})",
        R"({"id":"x","main":"sub/../../out.mjs"})",
    };

    for (const char* json : nasty)
    {
        Ograf::Manifest manifest = Ograf::parse(QByteArray(json));

        expectTrue(!manifest.valid,
                   QString("refused: %1").arg(QString(json).left(60)));
        expectTrue(!manifest.error.isEmpty(), "and said why");
    }

    // An id becomes part of how a graphic is addressed, and the spec says it may
    // hold anything but a forward slash.
    expectTrue(!Ograf::parse(R"({"id":"a/b","main":"g.mjs"})").valid, "an id with a slash is refused");
    expectTrue(Ograf::parse(R"({"id":"a-b_c.1","main":"g.mjs"})").valid, "an ordinary id is accepted");
}

static void anIncompleteManifestSaysWhatIsMissing()
{
    Ograf::Manifest noId = Ograf::parse(R"({"main":"g.mjs"})");
    expectTrue(!noId.valid, "a manifest with no id is refused");
    expectTrue(noId.error.contains("id"), "and the message names the id");

    Ograf::Manifest noMain = Ograf::parse(R"({"id":"x"})");
    expectTrue(!noMain.valid, "a manifest with no main is refused");
    expectTrue(noMain.error.contains("main"), "and the message names main");

    Ograf::Manifest broken = Ograf::parse("{ this is not json");
    expectTrue(!broken.valid, "unparseable JSON is refused");
    expectTrue(!broken.error.isEmpty(), "and says so rather than looking empty");

    Ograf::Manifest notAnObject = Ograf::parse("[1,2,3]");
    expectTrue(!notAnObject.valid, "a JSON array is not a manifest");

    // A graphic with no name is still usable, and the id is a better label than
    // an empty one.
    Ograf::Manifest noName = Ograf::parse(R"({"id":"my-graphic","main":"g.mjs"})");
    expectTrue(noName.valid, "a manifest with no name is still valid");
    same(noName.name, "my-graphic", "and falls back to the id as its label");
}

static void stepCountIsReadOrDefaulted()
{
    sameInt(Ograf::parse(R"({"id":"x","main":"g.mjs","stepCount":3})").stepCount, 3,
            "a declared stepCount is read");
    sameInt(Ograf::parse(R"({"id":"x","main":"g.mjs"})").stepCount, 1,
            "an absent stepCount is one");
    sameInt(Ograf::parse(R"({"id":"x","main":"g.mjs","stepCount":-1})").stepCount, -1,
            "-1 means dynamic and is preserved");
    sameInt(Ograf::parse(R"({"id":"x","main":"g.mjs","stepCount":"lots"})").stepCount, 1,
            "a stepCount that is not a number falls back to one");
}

static void manifestFilesAreRecognisedByName()
{
    expectTrue(Ograf::isManifestFileName("l3rd.ograf.json"), "an ordinary manifest is recognised");
    expectTrue(Ograf::isManifestFileName("My Graphic.OGRAF.JSON"), "case does not matter");

    expectTrue(!Ograf::isManifestFileName("graphic.json"), "a plain .json is not a manifest");
    expectTrue(!Ograf::isManifestFileName(".ograf.json"), "a bare suffix with no name is not a manifest");
    expectTrue(!Ograf::isManifestFileName("ograf.json"), "and neither is the suffix without the dot");
    expectTrue(!Ograf::isManifestFileName("l3rd.ograf.json.bak"), "nor a backup of one");
}

static void vendorExtensionsAreIgnoredRatherThanRejected()
{
    // The spec lets anyone add "v_"-prefixed fields. A manifest carrying another
    // vendor's extras must still load here.
    Ograf::Manifest manifest = Ograf::parse(
        R"({"id":"x","main":"g.mjs","v_someVendor":{"anything":true},"unknownField":42})");

    expectTrue(manifest.valid, "a manifest with vendor extensions still loads");
    same(manifest.id, "x", "and its own fields are unaffected");
}

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream(stdout) << "OGraf manifests\n";

    aRealManifestReadsCorrectly();
    defaultsComeOutOfTheSchema();
    anUndefaultedFieldIsLeftAlone();
    everyTypeOfDefaultSurvives();
    nestedObjectsAreWalked();
    nothingCanClimbOutOfTheGraphicsFolder();
    anIncompleteManifestSaysWhatIsMissing();
    stepCountIsReadOrDefaulted();
    manifestFilesAreRecognisedByName();
    vendorExtensionsAreIgnoredRatherThanRejected();

    QTextStream(stdout) << "\n" << (checks - failures) << " passed, " << failures << " failed\n";

    return failures == 0 ? 0 : 1;
}

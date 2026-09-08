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
#include <QtCore/QVector>

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

// ---------------------------------------------------------------- fields ----
//
// The Inspector's key/value table has typed editors, chosen by an int stored on
// each row. Until now the only thing that could set them was window.debugDataModes,
// a convention this fork invented. OGraf says the same thing in JSON Schema plus
// GDD's gddType, which is the version other tools already speak, so the mapping
// between the two is what makes a graphic from anywhere editable here.

static Ograf::Field fieldNamed(const QVector<Ograf::Field>& fields, const QString& key)
{
    foreach (const Ograf::Field& field, fields)
    {
        if (field.key == key)
            return field;
    }

    return Ograf::Field();
}

static QVector<Ograf::Field> fieldsFromSchema(const char* json)
{
    return Ograf::fieldsFor(Ograf::parse(QByteArray(json)).schema);
}

static void everyGddTypeReachesTheRightEditor()
{
    QVector<Ograf::Field> fields = fieldsFromSchema(R"({
        "id": "x", "main": "g.mjs",
        "schema": { "type": "object", "properties": {
            "headline":  { "type": "string",  "gddType": "single-line" },
            "body":      { "type": "string",  "gddType": "multi-line" },
            "logo":      { "type": "string",  "gddType": "file-path/image-path" },
            "tint":      { "type": "string",  "gddType": "color-rrggbb" },
            "tintAlpha": { "type": "string",  "gddType": "color-rrggbbaa" },
            "score":     { "type": "integer" },
            "holdMs":    { "type": "number",  "gddType": "duration-ms" },
            "opacity":   { "type": "number",  "gddType": "percentage" },
            "ratio":     { "type": "number" },
            "onAir":     { "type": "boolean" },
            "align":     { "type": "string",  "gddType": "select", "enum": ["left","center","right"] }
        } }
    })");

    sameInt(fields.size(), 11, "every property became a field");

    sameInt(fieldNamed(fields, "headline").mode, Ograf::Mode::Text, "a single-line string is text");
    sameInt(fieldNamed(fields, "body").mode, Ograf::Mode::Text, "a multi-line string is text");
    sameInt(fieldNamed(fields, "logo").mode, Ograf::Mode::Text, "a file path is text");
    sameInt(fieldNamed(fields, "tint").mode, Ograf::Mode::Color, "color-rrggbb is a colour");
    sameInt(fieldNamed(fields, "tintAlpha").mode, Ograf::Mode::Color, "color-rrggbbaa is a colour");
    sameInt(fieldNamed(fields, "score").mode, Ograf::Mode::Integer, "an integer is an integer");
    sameInt(fieldNamed(fields, "holdMs").mode, Ograf::Mode::Integer, "duration-ms is an integer");
    sameInt(fieldNamed(fields, "opacity").mode, Ograf::Mode::Decimal, "a percentage is a decimal");
    sameInt(fieldNamed(fields, "ratio").mode, Ograf::Mode::Decimal, "a number is a decimal");
    sameInt(fieldNamed(fields, "onAir").mode, Ograf::Mode::Boolean, "a boolean is a boolean");
    sameInt(fieldNamed(fields, "align").mode, Ograf::Mode::Cycle, "a select is a cycle");

    same(fieldNamed(fields, "align").cycleValues, "left|center|right",
         "and its choices are carried in the order the schema lists them");
}

static void anEnumIsAChoiceEvenWithoutGdd()
{
    // A plain JSON Schema enum, from a tool that never heard of GDD, is still a
    // fixed set of choices and deserves the cycle editor rather than a text box.
    QVector<Ograf::Field> fields = fieldsFromSchema(R"({
        "id": "x", "main": "g.mjs",
        "schema": { "type": "object", "properties": {
            "side": { "type": "string", "enum": ["home","away"] }
        } }
    })");

    sameInt(fieldNamed(fields, "side").mode, Ograf::Mode::Cycle, "a bare enum is a cycle");
    same(fieldNamed(fields, "side").cycleValues, "home|away", "with its choices");
}

static void numericChoicesSurviveAsChoices()
{
    QVector<Ograf::Field> fields = fieldsFromSchema(R"({
        "id": "x", "main": "g.mjs",
        "schema": { "type": "object", "properties": {
            "period": { "type": "integer", "gddType": "select", "enum": [1, 2, 3] }
        } }
    })");

    // The type says integer but the enum says choice, and choice is the stronger
    // statement: there are three valid values, not any integer.
    sameInt(fieldNamed(fields, "period").mode, Ograf::Mode::Cycle, "a numeric select is still a cycle");
    same(fieldNamed(fields, "period").cycleValues, "1|2|3", "and its numbers are rendered plainly");
}

static void aChoiceContainingAPipeIsDroppedRatherThanSplitting()
{
    // The table stores the choices as one pipe-separated string. A value with a
    // pipe in it would silently become two choices, neither of which the graphic
    // would accept.
    QVector<Ograf::Field> fields = fieldsFromSchema(R"({
        "id": "x", "main": "g.mjs",
        "schema": { "type": "object", "properties": {
            "layout": { "type": "string", "enum": ["a|b", "c", "d"] }
        } }
    })");

    same(fieldNamed(fields, "layout").cycleValues, "c|d", "the pipe-carrying choice is dropped");
}

static void defaultsAreRenderedForATextTable()
{
    QVector<Ograf::Field> fields = fieldsFromSchema(R"({
        "id": "x", "main": "g.mjs",
        "schema": { "type": "object", "properties": {
            "name":    { "type": "string",  "default": "John Doe" },
            "onAir":   { "type": "boolean", "default": true },
            "offAir":  { "type": "boolean", "default": false },
            "score":   { "type": "integer", "default": 3 },
            "ratio":   { "type": "number",  "default": 1.5 },
            "whole":   { "type": "number",  "default": 2 },
            "zero":    { "type": "integer", "default": 0 },
            "blank":   { "type": "string",  "default": "" },
            "none":    { "type": "string" }
        } }
    })");

    same(fieldNamed(fields, "name").value, "John Doe", "a string default is itself");

    // The table sends text to a template, so a boolean has to read as "true",
    // not as "1" — a template testing for the string would never match.
    same(fieldNamed(fields, "onAir").value, "true", "a true default renders as true");
    same(fieldNamed(fields, "offAir").value, "false", "a false default renders as false");

    same(fieldNamed(fields, "score").value, "3", "an integer renders plainly");
    same(fieldNamed(fields, "ratio").value, "1.5", "a fraction keeps its point");

    // JSON has one number type, so an integer default arrives as a double. A
    // field showing "2" rather than "2.0" is what an operator expects.
    same(fieldNamed(fields, "whole").value, "2", "a whole number loses its decimal point");

    // The falsy ones are the easy ones to lose.
    expectTrue(fieldNamed(fields, "zero").hasDefault, "a zero default counts as a default");
    same(fieldNamed(fields, "zero").value, "0", "and renders as zero");
    expectTrue(fieldNamed(fields, "blank").hasDefault, "an empty-string default counts as a default");
    same(fieldNamed(fields, "blank").value, "", "and renders as empty");

    expectTrue(!fieldNamed(fields, "none").hasDefault, "a field with no default says so");
    same(fieldNamed(fields, "none").value, "", "and carries nothing");
}

static void titlesBecomeLabelsAndKeysAreTheFallback()
{
    QVector<Ograf::Field> fields = fieldsFromSchema(R"({
        "id": "x", "main": "g.mjs",
        "schema": { "type": "object", "properties": {
            "f0":   { "type": "string", "title": "Presenter name" },
            "bare": { "type": "string" }
        } }
    })");

    same(fieldNamed(fields, "f0").label, "Presenter name", "a title becomes the label");
    same(fieldNamed(fields, "bare").label, "bare", "and the key stands in when there is none");
}

static void nestedObjectsFlattenOntoRows()
{
    QVector<Ograf::Field> fields = fieldsFromSchema(R"({
        "id": "x", "main": "g.mjs",
        "schema": { "type": "object", "properties": {
            "home": { "type": "object", "properties": {
                "name":  { "type": "string",  "default": "Rovers" },
                "score": { "type": "integer", "default": 0 }
            } },
            "clock": { "type": "string", "default": "00:00" }
        } }
    })");

    // The key/value table is flat, so a group of fields becomes rows with dotted
    // keys rather than one row holding an object nobody can edit.
    sameInt(fields.size(), 3, "the group became rows, not a single row");
    same(fieldNamed(fields, "home.name").value, "Rovers", "a nested field keeps its default");
    sameInt(fieldNamed(fields, "home.score").mode, Ograf::Mode::Integer, "and its type");
    same(fieldNamed(fields, "home.name").label, "name", "its label is the leaf, not the path");
    same(fieldNamed(fields, "clock").value, "00:00", "a sibling at the top level is unaffected");
}

static void anObjectWithNoPropertiesIsStillARow()
{
    // Nothing to descend into. A row the operator can put JSON in beats losing
    // the field without saying so.
    QVector<Ograf::Field> fields = fieldsFromSchema(R"({
        "id": "x", "main": "g.mjs",
        "schema": { "type": "object", "properties": {
            "extras": { "type": "object" }
        } }
    })");

    sameInt(fields.size(), 1, "the opaque object still produced a field");
    same(fieldNamed(fields, "extras").key, "extras", "under its own name");
}

static void aSchemaWithNothingInItProducesNothing()
{
    sameInt(fieldsFromSchema(R"({"id":"x","main":"g.mjs"})").size(), 0,
            "a manifest with no schema has no fields");
    sameInt(fieldsFromSchema(R"({"id":"x","main":"g.mjs","schema":{"type":"object"}})").size(), 0,
            "a schema with no properties has no fields");
    sameInt(fieldsFromSchema(R"({"id":"x","main":"g.mjs","schema":{"type":"object","properties":{}}})").size(), 0,
            "an empty properties object has no fields");
}

static void theRealExampleProducesTheRightTwoRows()
{
    QVector<Ograf::Field> fields = Ograf::fieldsFor(Ograf::parse(realExample()).schema);

    sameInt(fields.size(), 2, "the EBU example has two editable fields");
    same(fieldNamed(fields, "name").value, "John Doe", "the name default");
    same(fieldNamed(fields, "name").label, "Name", "and its title");
    same(fieldNamed(fields, "title").value, "OGraf expert", "the title default");
    sameInt(fieldNamed(fields, "name").mode, Ograf::Mode::Text, "both are plain text");
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

    everyGddTypeReachesTheRightEditor();
    anEnumIsAChoiceEvenWithoutGdd();
    numericChoicesSurviveAsChoices();
    aChoiceContainingAPipeIsDroppedRatherThanSplitting();
    defaultsAreRenderedForATextTable();
    titlesBecomeLabelsAndKeysAreTheFallback();
    nestedObjectsFlattenOntoRows();
    anObjectWithNoPropertiesIsStillARow();
    aSchemaWithNothingInItProducesNothing();
    theRealExampleProducesTheRightTwoRows();

    QTextStream(stdout) << "\n" << (checks - failures) << " passed, " << failures << " failed\n";

    return failures == 0 ? 0 : 1;
}

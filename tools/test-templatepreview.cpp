// What the Preview panel hands a CasparCG template: the data string, as the
// template's own update() will read it, and how the page is fitted to the panel.
//
// The strings are checked the way a template reads them: JSON has to parse back
// to the fields, XML has to carry each field with its value encoded, and the
// JavaScript literal has to survive quotes, backslashes and line breaks.

#include "../src/Common/TemplatePreviewData.h"

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

    using namespace TemplatePreviewData;

    const QList<QPair<QString, QString>> fields = {
        { "f0", "S\xc3\xa9ville & \"Co\"" },
        { "f1", "line one\nline two" },
        { "label", "<b>Keynote</b>" },
    };

    out << "JSON\n";
    const QString json = dataString(fields, true, false, 1);
    const QJsonObject parsed = QJsonDocument::fromJson(json.toUtf8()).object();
    expectTrue(!parsed.isEmpty(), "the JSON string parses as an object, as a template's JSON.parse will");
    expectTrue(parsed.value("f0").toString() == QString::fromUtf8("S\xc3\xa9ville & \"Co\""), "a value with quotes and accents comes back exactly");
    expectTrue(parsed.value("f1").toString() == "line one\nline two", "and one with a line break");
    expectTrue(!json.contains("\\\"Co\\\\\""), "without the extra escaping AMCP adds on air");
    expectTrue(QJsonDocument::fromJson(dataString(fields, true, true, 1).toUtf8()).object().value("label").toString() == "<B>KEYNOTE</B>",
               "uppercase data is uppercased, as on air");

    out << "\nXML\n";
    const QString xml = dataString(fields, false, false, 1);
    expectTrue(xml.startsWith("<templateData>") && xml.endsWith("</templateData>"), "XML is a templateData document");
    expectTrue(xml.contains("<componentData id=\"f0\"><data id=\"text\" value=\"S\xc3\xa9ville &amp; &quot;Co&quot;\"/></componentData>"),
               "each field is a componentData with its value encoded, in plain quotes");
    expectTrue(xml.contains("value=\"line one&#10;line two\""), "keeping line breaks encodes them");
    expectTrue(dataString(fields, false, false, 0).contains("value=\"line oneline two\""), "stripping line breaks removes them");
    expectTrue(dataString(fields, false, false, 2).contains("value=\"line one&lt;br&gt;line two\""), "innerHTML turns them into an encoded <br>");
    expectTrue(!xml.contains("\\\""), "no AMCP escaping");
    expectTrue(dataString({}, false, false, 1) == "<templateData></templateData>", "no fields is an empty document");

    out << "\nSample data\n";
    expectTrue(QJsonDocument::fromJson(debugDataAsJson("{ \"hours\": \"0\", \"minutes\": \"15\" }").toUtf8()).object().value("minutes").toString() == "15",
               "a double-quoted debugData object becomes a JSON string");
    expectTrue(QJsonDocument::fromJson(debugDataAsJson("{ 'f0': 'Name', 'f1': 'Title' }").toUtf8()).object().value("f1").toString() == "Title",
               "a single-quoted one too");
    expectTrue(debugDataAsJson("{ f0: someVariable }").isEmpty(), "something that is not data is nothing");

    out << "\nJavaScript\n";
    const QString nasty = QString::fromUtf8("a \"quote\", a 'quote', a \\backslash\\, a\nbreak, </script> and \xe2\x80\xa8");
    const QString literal = jsString(nasty);
    expectTrue(literal.startsWith('"') && literal.endsWith('"'), "a literal is quoted");
    expectTrue(!literal.contains('\n') && !literal.contains(QChar(0x2028)), "with no raw line break or line separator inside");
    expectTrue(QJsonDocument::fromJson(("[" + literal + "]").toUtf8()).array().at(0).toString() == nasty,
               "and it reads back to exactly the string");

    out << "\nFitting the page\n";
    Fit f = fit(480, 270, 1920, 1080);
    expectTrue(qAbs(f.scale - 0.25) < 1e-9 && f.x == 0 && f.y == 0, "a 16:9 panel shows the whole frame at a quarter");
    f = fit(480, 400, 1920, 1080);
    expectTrue(qAbs(f.scale - 0.25) < 1e-9 && f.x == 0 && qAbs(f.y - 65.0) < 1e-9, "a tall panel letterboxes, centred");
    f = fit(300, 270, 1920, 1080);
    expectTrue(qAbs(f.scale - 0.15625) < 1e-9 && qAbs(f.x) < 1e-9, "a narrow panel scales below the view's 25 % zoom limit");
    f = fit(0, 270, 1920, 1080);
    expectTrue(f.scale == 1.0, "a panel with no size yet leaves the page alone");
    const QString script = fitScript(fit(480, 400, 1920, 1080), 1920, 1080);
    expectTrue(script.contains("width: 1920px; height: 1080px") && script.contains("translate(0.00px, 65.00px) scale(0.25000)"),
               "the script lays the page out at its design size and scales it into place");

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}

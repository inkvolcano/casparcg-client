// A real test of the path rules, run rather than reasoned about.
//
// These four functions are the security boundary of template distribution. Every
// route into a client - a direct push, a relay, a GitHub repository - ends at
// installFile, and installFile writes HTML that CasparCG will execute. If a path
// can escape its pack, someone who reaches any of those routes can write anywhere
// the client can write.
//
// So they get a table of adversarial inputs and an actual verdict, not a careful
// reading. Build and run it with tools/test-paths.bat.

#include "../src/Widgets/TemplateInstaller.h"

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QTextStream>

static int failures = 0;
static int checks = 0;

static void check(bool actual, bool expected, const QString& what)
{
    checks++;
    if (actual == expected)
        return;

    failures++;
    QTextStream(stdout) << "  FAIL  " << what
                        << "  (expected " << (expected ? "allowed" : "refused")
                        << ", got " << (actual ? "allowed" : "refused") << ")\n";
}

static void allowed(const QString& path)
{
    check(TemplateInstaller::isSafeRelativePath(path), true, "path allowed: " + path);
}

static void refused(const QString& path, const QString& why)
{
    check(TemplateInstaller::isSafeRelativePath(path), false, why + ": " + path);
}

int main()
{
    QTextStream out(stdout);
    out << "Path rules\n";

    // ---- what a real pack looks like ----
    allowed("calendar.html");
    allowed("css/site.css");
    allowed("js/lib/thing.min.js");
    allowed("images/team (away).png");
    allowed("fonts/Roboto-Bold.woff2");
    allowed("a/b/c/d/e/f/g.html");          // 7 segments, the limit is 8

    // ---- escaping the pack ----
    refused("../evil.html", "parent");
    refused("../../evil.html", "parent twice");
    refused("a/../../evil.html", "parent after a segment");
    refused("..", "bare parent");
    refused(".", "bare current");
    refused("a/./b.html", "current segment");
    refused("/etc/passwd", "absolute posix");
    refused("\\windows\\system32\\a.dll", "absolute windows");
    refused("C:/windows/system32/a.dll", "drive letter");
    refused("C:a.html", "drive relative");
    refused("//server/share/a.html", "unc");
    refused("a//b.html", "empty segment");
    refused("", "empty path");

    // A backslash is a separator on the machine this lands on, so it is read as one
    // rather than as an ordinary character. That is what makes a Windows-style path
    // install to the right place instead of being refused outright.
    allowed("css\\site.css");
    allowed("js\\lib\\thing.js");

    // And it does not become a way out: the segments are checked after the swap.
    refused("..\\evil.html", "backslash parent");
    refused("a\\..\\..\\evil.html", "backslash parent twice");

    // ---- Windows will not treat these as files ----
    refused("con.html", "device name");
    refused("CON", "device name bare");
    refused("nul.js", "device name");
    refused("lpt1.css", "device name");
    refused("aux", "device name");
    refused("com9.html", "device name");
    allowed("console.html");                 // not a device, just starts like one
    allowed("connection/aux-panel.js");       // nor is this

    // Windows strips these, so two different paths would become one file.
    refused("trailing.", "trailing dot");
    refused("trailing ", "trailing space");
    refused("dir./file.html", "trailing dot on a directory");

    // ---- an alternate data stream is a second file hidden behind the first ----
    refused("a.html:hidden.exe", "NTFS stream");
    refused("a.html::$DATA", "NTFS stream raw");

    // ---- characters that do not belong in a filename ----
    refused("a<b.html", "redirection character");
    refused("a>b.html", "redirection character");
    refused("a|b.html", "pipe");
    refused("a\"b.html", "quote");
    refused("a?b.html", "wildcard");
    refused("a*b.html", "wildcard");
    refused(QString("a") + QChar(0x0000) + "b.html", "embedded null");
    refused(QString("evil") + QChar(0x202E) + "cod.txt", "right-to-left override");
    refused("a\nb.html", "newline");
    refused("a\tb.html", "tab");

    // ---- depth ----
    refused("a/b/c/d/e/f/g/h/i.html", "too deep");

    out << "\nProtected files\n";

    // The two files each client owns. Case does not matter; a client's own API key
    // and Sheets buttons must survive every push and every pull.
    check(TemplateInstaller::isProtected("project.js"), true, "project.js");
    check(TemplateInstaller::isProtected("PROJECT.JS"), true, "PROJECT.JS");
    check(TemplateInstaller::isProtected("Project.js"), true, "Project.js");
    check(TemplateInstaller::isProtected("extensions.json"), true, "extensions.json");
    check(TemplateInstaller::isProtected("Extensions.JSON"), true, "Extensions.JSON");

    // Deeper down it is template code, not the connection file, and travels normally.
    check(TemplateInstaller::isProtected("webcg/project.js"), false, "webcg/project.js");
    check(TemplateInstaller::isProtected("js/extensions.json"), false, "js/extensions.json");

    // Near misses that must not be caught by the rule.
    check(TemplateInstaller::isProtected("project.json"), false, "project.json");
    check(TemplateInstaller::isProtected("projects.js"), false, "projects.js");
    check(TemplateInstaller::isProtected("extension.json"), false, "extension.json");

    out << "\nGit blob digests\n";

    // The values git itself produces. This is what a GitHub tree is compared
    // against, so if it drifts, every file reads as changed on every poll.
    struct { const char* content; const char* sha; } known[] = {
        { "",                "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391" },
        { "<h1>hello</h1>",  "653debaa84cba3d7fe1daf443b17b74864ff56cf" },
    };

    for (const auto& one : known)
    {
        QString actual = TemplateInstaller::gitBlobSha(QByteArray(one.content));
        checks++;
        if (actual != QString(one.sha))
        {
            failures++;
            out << "  FAIL  digest of \"" << one.content << "\"\n"
                << "        expected " << one.sha << "\n"
                << "        got      " << actual << "\n";
        }
    }

    // A digest must cover the bytes, not just their length: two files of the same
    // size that differ have to differ here, or a changed template is never fetched.
    checks++;
    if (TemplateInstaller::gitBlobSha("AAAA") == TemplateInstaller::gitBlobSha("BBBB"))
    {
        failures++;
        out << "  FAIL  same digest for different content of the same length\n";
    }

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    out.flush();

    return failures == 0 ? 0 : 1;
}

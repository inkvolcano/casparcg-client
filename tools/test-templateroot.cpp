// What the push tool is pointed at, and whether it is pointed at anything.
//
// This suite exists because of a bug that looked cosmetic and was not. The
// templates folder is a text field whose placeholder shows an example path.
// Leave it empty and the old code did QDir(""), which Qt resolves to the
// program's working directory — and which reports that it exists. So the tool
// listed the folders next to its own executable as template packs: platforms,
// imageformats, qmltooling, resources. Qt's own runtime, offered as something to
// push to a playout machine.
//
// The listing was the half you could see. Every later step used the same empty
// root, and QDir("").filePath("platforms") is "platforms" — a relative path,
// resolved against the working directory again. Comparing walked Qt's DLLs, and
// pushing would have uploaded them into a client's templates folder.
//
// So the cases that matter here are the ones where an answer of "here" is wrong:
// empty, whitespace, and relative.

#include "../src/Common/TemplateRoot.h"

#include <QtCore/QDir>
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

// A folder that certainly exists, whatever machine this runs on.
static QString realFolder()
{
    return QDir::tempPath();
}

static void nothingSetIsNotTheFolderWeHappenToBeIn()
{
    // The bug, stated directly. Qt says QDir("") exists, because it is the
    // working directory. That must not become a templates root.
    TemplateRoot::Root root = TemplateRoot::resolve("");

    expectTrue(!root.usable, "an empty field is not a folder");
    expectTrue(!root.problem.isEmpty(), "and says so");
    expectTrue(root.path.isEmpty(), "and offers no path");

    expectTrue(!TemplateRoot::resolve("   ").usable, "and neither is one of only spaces");
    expectTrue(!TemplateRoot::resolve("\t\n").usable, "or of only whitespace");
}

static void aRelativePathIsTheSameTrap()
{
    // "templates" resolves against wherever the tool was started from. Nobody
    // typing a templates folder means that, and Browse never produces it.
    expectTrue(!TemplateRoot::resolve("templates").usable, "a bare name is refused");
    expectTrue(!TemplateRoot::resolve("./templates").usable, "and so is an explicit relative path");
    expectTrue(!TemplateRoot::resolve("../templates").usable, "and one that climbs");

    expectTrue(TemplateRoot::resolve("templates").problem.contains("relative"),
               "and the message says why");
}

static void aRealFolderIsAccepted()
{
    TemplateRoot::Root root = TemplateRoot::resolve(realFolder());

    expectTrue(root.usable, "a real absolute folder is usable");
    expectTrue(root.problem.isEmpty(), "with nothing to complain about");
    expectTrue(!root.path.isEmpty(), "and a path to use");

    // Surrounding space comes free with pasting.
    expectTrue(TemplateRoot::resolve("  " + realFolder() + "  ").usable,
               "a pasted path with spaces around it still works");
}

static void aFolderThatIsNotThereSaysSo()
{
    const QString missing = QDir(realFolder()).filePath("no-such-templates-folder-xyz");

    TemplateRoot::Root root = TemplateRoot::resolve(missing);

    expectTrue(!root.usable, "a folder that does not exist is refused");
    expectTrue(root.problem.contains("does not exist"), "and the message says which way it is wrong");
}

static void everyPackPathStaysInsideTheRoot()
{
    TemplateRoot::Root root = TemplateRoot::resolve(realFolder());
    expectTrue(root.usable, "the root under test is usable");

    // The ordinary case.
    const QString ok = TemplateRoot::packPath(root, "SEVILLE");
    expectTrue(!ok.isEmpty(), "an ordinary pack name resolves");
    expectTrue(ok.startsWith(QDir::cleanPath(root.path)), "inside the root");

    // Pack names are read back from a saved setting as well as from a directory
    // listing, so a name that climbs has to be refused rather than trusted.
    QStringList nasty;
    nasty << ".." << "." << "" << "   "
          << "../evil" << "../../evil"
          << "sub/evil" << "sub\\evil"
          << "C:/windows" << "/etc";

    foreach (const QString& pack, nasty)
    {
        expectTrue(TemplateRoot::packPath(root, pack).isEmpty(),
                   QString("pack \"%1\" is refused").arg(pack));
    }
}

static void anUnusableRootYieldsNoPackPathAtAll()
{
    // This is what stops compare and push acting on the working directory. Even
    // if a pack name survived from a previous session, there is nowhere to put it.
    TemplateRoot::Root empty = TemplateRoot::resolve("");

    expectTrue(TemplateRoot::packPath(empty, "platforms").isEmpty(),
               "no root means no pack path, even for a plausible name");
    expectTrue(TemplateRoot::packPath(empty, "SEVILLE").isEmpty(),
               "and not for a real one either");

    TemplateRoot::Root relative = TemplateRoot::resolve("templates");
    expectTrue(TemplateRoot::packPath(relative, "SEVILLE").isEmpty(),
               "and a relative root gives none");
}

static void theQtRuntimeCaseSpecifically()
{
    // The exact names from the screenshot that started this. With no root set,
    // not one of them may produce a path to walk or upload.
    TemplateRoot::Root none = TemplateRoot::resolve("");

    QStringList qtFolders;
    qtFolders << "generic" << "iconengines" << "imageformats" << "multimedia"
              << "networkinformation" << "platforminputcontexts" << "platforms"
              << "plugins" << "position" << "qml" << "qmltooling" << "resources";

    foreach (const QString& folder, qtFolders)
    {
        expectTrue(TemplateRoot::packPath(none, folder).isEmpty(),
                   QString("\"%1\" cannot be pushed from an unset root").arg(folder));
    }
}

static void aPackPathIsCleanedRatherThanLeftOdd()
{
    TemplateRoot::Root root = TemplateRoot::resolve(realFolder());

    const QString path = TemplateRoot::packPath(root, "SEVILLE");
    same(path, QDir::cleanPath(QDir(root.path).filePath("SEVILLE")),
         "the pack path is the cleaned join of root and name");
}

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream(stdout) << "Push tool templates folder\n";

    nothingSetIsNotTheFolderWeHappenToBeIn();
    aRelativePathIsTheSameTrap();
    aRealFolderIsAccepted();
    aFolderThatIsNotThereSaysSo();
    everyPackPathStaysInsideTheRoot();
    anUnusableRootYieldsNoPackPathAtAll();
    theQtRuntimeCaseSpecifically();
    aPackPathIsCleanedRatherThanLeftOdd();

    QTextStream(stdout) << "\n" << (checks - failures) << " passed, " << failures << " failed\n";

    return failures == 0 ? 0 : 1;
}

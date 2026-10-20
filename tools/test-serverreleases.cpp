// The CasparCG Server builds the client offers: GitHub releases, the nightly
// listings, the folder a build goes into, and the config backup name.

#include "../src/Common/ServerReleases.h"

#include <QtCore/QSet>
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

int main(int argc, char* argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    QTextStream out(stdout);
    out << "Server releases\n";

    using namespace ServerReleases;

    expectTrue(versionFromTag("v2.5.1-stable") == "2.5.1", "a stable tag");
    expectTrue(versionFromTag("v2.4.0-rc1") == "2.4.0 rc1", "a release candidate");
    expectTrue(versionFromTag("v2.3.3-lts-stable") == "2.3.3 lts", "an LTS release");

    // Shaped like the GitHub API's answer, trimmed.
    const QByteArray json = R"json([
      { "tag_name": "v2.5.1-stable", "name": "v2.5.1 Stable", "draft": false, "prerelease": false,
        "published_at": "2026-09-16T16:29:03Z",
        "assets": [ { "name": "casparcg-server-2.5_2.5.1.stable-noble1_amd64.deb", "size": 1, "browser_download_url": "https://x/deb" },
                    { "name": "casparcg-server-v2.5.1-windows.zip", "size": 295641939,
                      "browser_download_url": "https://github.com/CasparCG/server/releases/download/v2.5.1-stable/casparcg-server-v2.5.1-windows.zip",
                      "digest": "sha256:C0756F53F49908FBA94F17A43B83D45FD275201E24AB1676530D7BB26832EFC6" } ] },
      { "tag_name": "v2.4.0-rc1", "name": "v2.4.0 RC 1", "draft": false, "prerelease": true,
        "published_at": "2024-01-09T21:54:14Z",
        "assets": [ { "name": "casparcg-server-v2.4.0-rc1-windows.zip", "size": 200, "browser_download_url": "https://x/rc1.zip" } ] },
      { "tag_name": "v9.9.9", "name": "draft", "draft": true, "prerelease": false, "published_at": "2026-10-01T00:00:00Z",
        "assets": [ { "name": "casparcg-server-v9.9.9-windows.zip", "size": 1, "browser_download_url": "https://x/draft.zip" } ] },
      { "tag_name": "v2.0.7", "name": "", "draft": false, "prerelease": false, "published_at": "2019-01-01T00:00:00Z",
        "assets": [ { "name": "source.tar.gz", "size": 1, "browser_download_url": "https://x/src" } ] }
    ])json";
    const QList<Build> releases = parseGitHubReleases(json);
    expectTrue(releases.count() == 2, QString("drafts and releases without a Windows zip are left out: %1").arg(releases.count()));
    expectTrue(!releases.isEmpty() && releases.first().version == "2.5.1" && releases.first().kind == Kind::Stable, "newest first, stable");
    expectTrue(!releases.isEmpty() && releases.first().url.endsWith("casparcg-server-v2.5.1-windows.zip"), "the Windows zip, not the .deb");
    expectTrue(!releases.isEmpty() && releases.first().sha256 == "c0756f53f49908fba94f17a43b83d45fd275201e24ab1676530d7bb26832efc6",
               "GitHub's SHA-256, lower case");
    expectTrue(releases.count() == 2 && releases.at(1).kind == Kind::PreRelease && releases.at(1).sha256.isEmpty(),
               "a pre-release, with no checksum when GitHub gave none");

    // Rows exactly as the Apache listing on casparcg.com writes them.
    const QString root =
        "<pre>      <a href=\"?C=N;O=A\">Name</a>  <hr>      <a href=\"/builds/\">Parent Directory</a>   -\n"
        "      <a href=\"master/\">master/</a>                 2026-10-15 13:48    -   \n"
        "      <a href=\"v2.5.x/\">v2.5.x/</a>                 2026-10-14 15:54    -   \n";
    expectTrue(parseNightlyBranches(root) == QStringList() << "master" << "v2.5.x", "the branch folders, not the parent");

    QString index;
    for (int i = 0; i < 7; i++)
        index += QString("      <a href=\"casparcg-server-%1-windows.zip\">casparcg-server-%1-windows.zip</a>  2026-10-%2 13:48  296M  \n")
                     .arg(QString("%1fe19ac0e36bf1d3f080f06ef161dafc4d73cac").arg(i)).arg(10 + i, 2, 10, QChar('0'));
    index += "      <a href=\"casparcg-server-2.6.0~master-noble+fe19ac0.zip\">casparcg-server-2.6.0~master-noble+fe19ac0.zip</a>  2026-10-15 13:46  129M  \n";
    const QList<Build> nightlies = parseNightlyIndex(index, "master", QString(NIGHTLY_ROOT) + "master/");
    expectTrue(nightlies.count() == NIGHTLIES_PER_BRANCH, QString("the newest five, Windows only: %1").arg(nightlies.count()));
    expectTrue(!nightlies.isEmpty() && nightlies.first().date.date() == QDate(2026, 10, 16), "newest first");
    expectTrue(!nightlies.isEmpty() && nightlies.first().commit == "6fe19ac", "with its short commit");
    expectTrue(!nightlies.isEmpty() && nightlies.first().url.startsWith("https://casparcg.com/builds/CasparCG%20Server/master/casparcg-server-"),
               "and its address");
    expectTrue(listingSize("296M") == 296LL * 1024 * 1024 && listingSize("1.5G") > 1500000000LL && listingSize("x") == 0, "listing sizes");

    Build stable = releases.isEmpty() ? Build() : releases.first();
    expectTrue(folderName(stable) == "CasparCG Server 2.5.1", "a release's folder");
    expectTrue(!nightlies.isEmpty() && folderName(nightlies.first()) == "CasparCG Server master 2026-10-16 6fe19ac", "a nightly's folder");
    Build odd;
    odd.version = "2.5:1/x?";
    expectTrue(folderName(odd) == "CasparCG Server 2.5_1_x_", "nothing Windows refuses in a folder name");

    QSet<QString> taken = { "C:/CasparCG/CasparCG Server 2.5.1", "C:/CasparCG/CasparCG Server 2.5.1 (2)" };
    auto exists = [&taken](const QString& path) { return taken.contains(path); };
    expectTrue(uniqueFolder("C:/CasparCG", "CasparCG Server 2.4.3", exists) == "C:/CasparCG/CasparCG Server 2.4.3", "a free name is used as it is");
    expectTrue(uniqueFolder("C:/CasparCG/", "CasparCG Server 2.5.1", exists) == "C:/CasparCG/CasparCG Server 2.5.1 (3)",
               "an existing install is never written over");

    expectTrue(configBackupName(QDate(2026, 10, 20)) == "casparcg.config.2026-10-20.bak", "the dated backup");
    const QDateTime when(QDate(2026, 10, 20), QTime(14, 32, 5));
    expectTrue(configBackupName(when, [](const QString&) { return false; }) == "casparcg.config.2026-10-20.bak", "the first backup of the day");
    expectTrue(configBackupName(when, [](const QString& name) { return name == "casparcg.config.2026-10-20.bak"; })
                   == "casparcg.config.2026-10-20_143205.bak", "a second one the same day is not written over the first");

    expectTrue(orderBranches(QStringList() << "v2.4.x" << "master" << "v2.5.x") == QStringList() << "master" << "v2.5.x" << "v2.4.x",
               "master first, then the newest release branch");

    expectTrue(isRelativeConfigPath("media/") && isRelativeConfigPath("./data") && !isRelativeConfigPath("C:/CasparCG/media/")
                   && !isRelativeConfigPath("//nas/media") && !isRelativeConfigPath("d:\\x") && !isRelativeConfigPath(""),
               "which config paths are relative");

    // The paths section of the config shipped with 2.5, and of this machine's own.
    const QString shipped =
        "<configuration>\n    <paths>\n        <media-path>media/</media-path>\n        <log-path disable=\"false\">log/</log-path>\n"
        "        <data-path>data/</data-path>\n        <template-path>template/</template-path>\n    </paths>\n"
        "    <!-- <media-path>x/</media-path> is fine in a comment -->\n</configuration>\n";
    const CarriedConfig carried = carryConfig(shipped, "C:\\CasparCG\\CasparCG Server");
    expectTrue(carried.xml.contains("<media-path>C:/CasparCG/CasparCG Server/media/</media-path>"), "a relative media path points at the old folder");
    expectTrue(carried.xml.contains("<template-path>C:/CasparCG/CasparCG Server/template/</template-path>")
                   && carried.xml.contains("<data-path>C:/CasparCG/CasparCG Server/data/</data-path>"), "and the template and data paths");
    expectTrue(carried.xml.contains("<log-path disable=\"false\">log/</log-path>"), "the log path stays with the new server");
    expectTrue(carried.xml.contains("<!-- <media-path>x/</media-path>"), "an example in a comment is not a setting");
    expectTrue(carried.changed.count() == 3 && carried.changed.first() == "media-path: media/ -> C:/CasparCG/CasparCG Server/media/",
               QString("each change is named: %1").arg(carried.changed.join(" | ")));

    const QString local =
        "<paths>\n        <media-path>C:/CasparCG/media/</media-path>\n        <log-path>log/</log-path>\n"
        "        <template-path>C:/CasparCG/templates/</template-path>\n</paths>";
    const CarriedConfig kept = carryConfig(local, "C:/CasparCG/CasparCG Server/");
    expectTrue(kept.xml == local && kept.changed.isEmpty(), "absolute paths and everything else are left exactly as written");

    out << "\n" << (checks - failures) << " passed, " << failures << " failed\n";
    return failures == 0 ? 0 : 1;
}

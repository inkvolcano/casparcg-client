#pragma once

// CasparCG Server builds the client can download and install, from the two
// places CasparCG publishes them:
//
//   - GitHub releases of CasparCG/server: stable releases and pre-releases, each
//     with a Windows zip and the SHA-256 GitHub publishes for it;
//   - https://casparcg.com/builds/CasparCG%20Server/<branch>/: the nightly builds
//     of every branch (master, v2.5.x, ...), a plain directory listing of zips
//     named casparcg-server-<commit>-windows.zip. No checksum is published for
//     these; the download is checked against the size the server reports.
//
// Every Windows zip unpacks flat - casparcg.exe and casparcg.config at the top -
// checked against 2.4.3, 2.5.1 and a master nightly (2026-10-20).
//
// The user's choices (2026-10-20): a new folder per version, beside the current
// one; keep the current casparcg.config, with a dated backup written first; the
// newest few nightlies of every branch. tools/test-serverreleases checks this.

#include <QtCore/QDate>
#include <QtCore/QDateTime>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QList>
#include <QtCore/QPair>
#include <QtCore/QRegularExpression>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <algorithm>
#include <functional>

namespace ServerReleases
{
    static const char* const GITHUB_API = "https://api.github.com/repos/CasparCG/server/releases?per_page=40";
    static const char* const NIGHTLY_ROOT = "https://casparcg.com/builds/CasparCG%20Server/";
    static const int NIGHTLIES_PER_BRANCH = 5;

    enum class Kind { Stable, PreRelease, Nightly };

    struct Build
    {
        Kind kind = Kind::Stable;
        QString version;     // "2.5.1" for a release; the branch for a nightly
        QString title;       // what the list shows
        QString branch;      // nightlies only
        QString commit;      // nightlies only, short
        QDateTime date;
        qint64 size = 0;     // bytes; approximate for a nightly (the index rounds)
        QString url;
        QString sha256;      // releases only; empty when GitHub gave none
        QString notes;       // releases only: the release text, Markdown
    };

    inline QString kindText(Kind kind)
    {
        switch (kind)
        {
            case Kind::Stable: return "Stable";
            case Kind::PreRelease: return "Pre-release";
            default: return "Nightly";
        }
    }

    // "v2.5.1-stable" -> "2.5.1"; "v2.4.0-rc1" -> "2.4.0 rc1"; "v2.3.3-lts-stable" -> "2.3.3 lts".
    inline QString versionFromTag(const QString& tag)
    {
        QString text = tag.trimmed();
        if (text.startsWith('v') || text.startsWith('V'))
            text = text.mid(1);
        text.replace(QRegularExpression("-stable$", QRegularExpression::CaseInsensitiveOption), QString());
        text.replace('-', ' ');
        return text.trimmed();
    }

    // The Windows zip of every non-draft release, newest first.
    inline QList<Build> parseGitHubReleases(const QByteArray& json)
    {
        QList<Build> builds;
        const QJsonArray releases = QJsonDocument::fromJson(json).array();
        for (const QJsonValue& value : releases)
        {
            const QJsonObject release = value.toObject();
            if (release.value("draft").toBool())
                continue;

            for (const QJsonValue& assetValue : release.value("assets").toArray())
            {
                const QJsonObject asset = assetValue.toObject();
                const QString name = asset.value("name").toString();
                if (!name.toLower().endsWith(".zip") || !name.toLower().contains("windows"))
                    continue;

                Build build;
                build.kind = release.value("prerelease").toBool() ? Kind::PreRelease : Kind::Stable;
                build.version = versionFromTag(release.value("tag_name").toString());
                build.title = release.value("name").toString().trimmed();
                if (build.title.isEmpty())
                    build.title = build.version;
                build.date = QDateTime::fromString(release.value("published_at").toString(), Qt::ISODate);
                build.size = qint64(asset.value("size").toDouble());
                build.url = asset.value("browser_download_url").toString();
                build.notes = release.value("body").toString();

                const QString digest = asset.value("digest").toString();
                if (digest.startsWith("sha256:"))
                    build.sha256 = digest.mid(7).toLower();

                builds.append(build);
                break;
            }
        }

        std::sort(builds.begin(), builds.end(), [](const Build& a, const Build& b) { return a.date > b.date; });
        return builds;
    }

    // The branch folders of the nightly root listing: "master", "v2.5.x", ...
    inline QStringList parseNightlyBranches(const QString& html)
    {
        static const QRegularExpression folder("<a href=\"([A-Za-z0-9._~-]+)/\">");
        QStringList branches;
        QRegularExpressionMatchIterator it = folder.globalMatch(html);
        while (it.hasNext())
        {
            const QString name = it.next().captured(1);
            if (!branches.contains(name))
                branches.append(name);
        }
        return branches;
    }

    // "296M" -> bytes. The listing rounds, so this is only for showing a size and
    // for a sanity check against what the download reports.
    inline qint64 listingSize(const QString& text)
    {
        static const QRegularExpression size("^([0-9.]+)([KMG]?)$");
        const QRegularExpressionMatch m = size.match(text.trimmed());
        if (!m.hasMatch())
            return 0;
        double value = m.captured(1).toDouble();
        const QString unit = m.captured(2);
        if (unit == "K") value *= 1024.0;
        else if (unit == "M") value *= 1024.0 * 1024.0;
        else if (unit == "G") value *= 1024.0 * 1024.0 * 1024.0;
        return qint64(value);
    }

    // The newest Windows nightlies of one branch, newest first.
    inline QList<Build> parseNightlyIndex(const QString& html, const QString& branch, const QString& branchUrl,
                                          int keep = NIGHTLIES_PER_BRANCH)
    {
        static const QRegularExpression row(
            "<a href=\"(casparcg-server-([0-9a-f]{7,40})-windows\\.zip)\">[^<]*</a>\\s+(\\d{4}-\\d\\d-\\d\\d \\d\\d:\\d\\d)\\s+([0-9.]+[KMG]?)");

        QList<Build> builds;
        QRegularExpressionMatchIterator it = row.globalMatch(html);
        while (it.hasNext())
        {
            const QRegularExpressionMatch m = it.next();
            Build build;
            build.kind = Kind::Nightly;
            build.branch = branch;
            build.version = branch;
            build.commit = m.captured(2).left(7);
            build.date = QDateTime::fromString(m.captured(3), "yyyy-MM-dd HH:mm");
            build.size = listingSize(m.captured(4));
            build.url = branchUrl + m.captured(1);
            build.title = QString("%1 nightly %2 (%3)").arg(branch, build.date.toString("yyyy-MM-dd"), build.commit);
            builds.append(build);
        }

        std::sort(builds.begin(), builds.end(), [](const Build& a, const Build& b) { return a.date > b.date; });
        if (builds.count() > keep)
            builds = builds.mid(0, keep);
        return builds;
    }

    // A folder name for a build, safe on Windows: "CasparCG Server 2.5.1",
    // "CasparCG Server master 2026-10-15 fe19ac0".
    inline QString folderName(const Build& build)
    {
        QString name = build.kind == Kind::Nightly
            ? QString("CasparCG Server %1 %2 %3").arg(build.branch, build.date.toString("yyyy-MM-dd"), build.commit)
            : QString("CasparCG Server %1").arg(build.version);

        name.replace(QRegularExpression("[<>:\"/\\\\|?*]"), "_");
        return name.simplified();
    }

    // A folder that does not exist yet: the name itself, else "name (2)", "name (3)" ...
    inline QString uniqueFolder(const QString& parent, const QString& name, const std::function<bool(const QString&)>& exists)
    {
        const QString base = parent.endsWith('/') || parent.endsWith('\\') ? parent + name : parent + "/" + name;
        if (!exists(base))
            return base;

        for (int i = 2; i < 1000; i++)
        {
            const QString candidate = QString("%1 (%2)").arg(base).arg(i);
            if (!exists(candidate))
                return candidate;
        }
        return QString();
    }

    // master first, then the release branches newest first: v2.5.x before v2.4.x.
    inline QStringList orderBranches(QStringList branches)
    {
        std::sort(branches.begin(), branches.end(), [](const QString& a, const QString& b) {
            if (a == "master" || b == "master")
                return a == "master" && b != "master";
            return QString::compare(a, b, Qt::CaseInsensitive) > 0;
        });
        return branches;
    }

    // The dated copy written before a config is carried over or replaced. A
    // second backup on the same day takes the time too, so neither is lost.
    inline QString configBackupName(const QDate& date)
    {
        return QString("casparcg.config.%1.bak").arg(date.toString("yyyy-MM-dd"));
    }

    inline QString configBackupName(const QDateTime& when, const std::function<bool(const QString&)>& exists)
    {
        const QString daily = configBackupName(when.date());
        if (!exists(daily))
            return daily;
        return QString("casparcg.config.%1.bak").arg(when.toString("yyyy-MM-dd_HHmmss"));
    }

    // Whether a path in casparcg.config is relative, and so resolved against the
    // server's own folder: "media/" is, "C:/CasparCG/media/" and "//nas/media" are not.
    inline bool isRelativeConfigPath(const QString& path)
    {
        const QString text = path.trimmed();
        if (text.isEmpty())
            return false;
        if (text.startsWith('/') || text.startsWith('\\'))
            return false;
        return !QRegularExpression("^[A-Za-z]:").match(text).hasMatch();
    }

    struct CarriedConfig
    {
        QString xml;
        QStringList changed;   // "media-path: media/ -> C:/CasparCG/CasparCG Server/media/"
    };

    // The current config as the new folder gets it. A server reads a relative
    // media, template, data or font path from its own folder, so carried over
    // unchanged those would point inside the new, empty install: they are made to
    // point at the old folder instead, where the files are. The log path is left
    // alone - a new log folder for a new server is what anyone would expect.
    // Only those values change; the rest of the file stays as written, comments
    // and spacing included.
    inline CarriedConfig carryConfig(const QString& xml, const QString& oldFolder)
    {
        CarriedConfig carried;

        QString base = QString(oldFolder).replace('\\', '/');
        if (!base.endsWith('/'))
            base += '/';

        static const QRegularExpression element(
            "<(media-path|template-path|data-path|font-path)(\\s[^>]*)?>([^<]*)</\\1>");

        // The shipped config lists examples in comments; those are not settings.
        QList<QPair<int, int>> comments;
        for (int from = xml.indexOf("<!--"); from >= 0; from = xml.indexOf("<!--", from))
        {
            const int end = xml.indexOf("-->", from + 4);
            comments.append(qMakePair(from, end < 0 ? int(xml.size()) : end + 3));
            if (end < 0)
                break;
            from = end + 3;
        }
        auto inComment = [&comments](int position) {
            for (const QPair<int, int>& range : comments)
            {
                if (position >= range.first && position < range.second)
                    return true;
            }
            return false;
        };

        QString out;
        int last = 0;
        QRegularExpressionMatchIterator it = element.globalMatch(xml);
        while (it.hasNext())
        {
            const QRegularExpressionMatch m = it.next();
            const QString value = m.captured(3);
            if (!isRelativeConfigPath(value) || inComment(m.capturedStart(0)))
                continue;

            QString relative = value.trimmed();
            relative.replace('\\', '/');
            while (relative.startsWith("./"))
                relative = relative.mid(2);
            const QString absolute = base + relative;

            out += xml.mid(last, m.capturedStart(3) - last);
            out += absolute;
            last = m.capturedEnd(3);
            carried.changed.append(QString("%1: %2 -> %3").arg(m.captured(1), value.trimmed(), absolute));
        }
        out += xml.mid(last);

        carried.xml = out;
        return carried;
    }

    // Where the shipped config of the new version is kept when the old one is
    // carried over, so its new options can still be looked up.
    inline QString shippedConfigName()
    {
        return QString("casparcg.config.shipped");
    }
}

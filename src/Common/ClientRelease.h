#pragma once

// Which build is this, which build is published, and is that one newer.
//
// The client has never been able to answer any of those. A venue runs whatever
// was copied onto it, the title bar says "build 206" to nobody in particular, and
// finding out what an estate is running means visiting every machine. Templates
// got a distribution route; the thing that plays them did not.
//
// This is the arithmetic half of fixing that, kept away from the network so it can
// be tested without one. Three jobs:
//
//   parse      a version out of whatever a release tag happens to be called
//   compare    two of them, numerically, so 2.3.10 is not older than 2.3.9
//   choose     the one asset out of a release that belongs on this machine
//
// The build number is the part that matters here. Upstream's semantic version has
// moved three times in as many years while this fork is on its two-hundredth
// build, so "is there something newer" is almost always a question about the
// trailing number, and a comparison that ignored it would answer never.
//
// Header-only, like PanelFit.h and CheckInTarget.h, so the rules can be compiled
// and tested on their own.

#include <QtCore/QString>
#include <QtCore/QStringList>

namespace ClientRelease
{
    struct Version
    {
        bool valid = false;

        int major = 0;
        int minor = 0;
        int revision = 0;

        // This fork's own counter. Absent from an upstream tag, which is why it
        // defaults to zero rather than to anything that could look newer.
        int build = 0;

        QString text;   // what it was parsed from, for showing back
    };

    // Everything a release tag has been called here or upstream:
    //
    //   v2.3.1              upstream's shape
    //   2.3.1               the same without the v
    //   v2.3.1-206          this fork, build in the tag
    //   v2.3.1-build206     spelled out
    //   build-206           build only, for a fork release that moved nothing else
    //   2.3.1 build 206     what the title bar says, so a person can paste it
    //
    // Anything else is invalid rather than guessed at: a tag nobody can parse
    // should read as "no idea", not as version zero, which would look ancient and
    // make every client think it was behind.
    inline Version parse(const QString& raw)
    {
        Version version;
        version.text = raw.trimmed();

        if (version.text.isEmpty())
            return version;

        // Reduce to numbers separated by single spaces, so one scan handles every
        // shape above. Anything that is not a digit becomes a separator.
        QString flattened;
        foreach (const QChar& c, version.text)
            flattened.append(c.isDigit() ? c : QChar(' '));

        const QStringList numbers = flattened.split(' ', Qt::SkipEmptyParts);
        if (numbers.isEmpty())
            return version;

        // "build-206" and "build 206": one number, and it is the build rather than
        // a major version, which is the difference between a fork release reading
        // as build 206 and as version 206.0.0.
        const bool buildOnly = numbers.size() == 1
            && version.text.contains("build", Qt::CaseInsensitive);

        if (buildOnly)
        {
            version.valid = true;
            version.build = numbers.at(0).toInt();

            return version;
        }

        version.valid = true;
        version.major = numbers.at(0).toInt();

        if (numbers.size() > 1)
            version.minor = numbers.at(1).toInt();
        if (numbers.size() > 2)
            version.revision = numbers.at(2).toInt();
        if (numbers.size() > 3)
            version.build = numbers.at(3).toInt();

        return version;
    }

    // Numerically, component by component. The string comparison this replaces
    // would have called 2.3.10 older than 2.3.9, and build 99 newer than build 100,
    // which is the whole reason this is a function rather than an operator on two
    // QStrings somewhere in a dialog.
    inline bool isNewer(const Version& candidate, const Version& running)
    {
        // An unparsable tag is never newer. Offering an update on the strength of a
        // tag nobody understood is how an estate ends up installing something
        // nobody meant to publish.
        if (!candidate.valid)
            return false;

        if (!running.valid)
            return false;

        if (candidate.major != running.major)
            return candidate.major > running.major;
        if (candidate.minor != running.minor)
            return candidate.minor > running.minor;
        if (candidate.revision != running.revision)
            return candidate.revision > running.revision;

        return candidate.build > running.build;
    }

    inline QString describe(const Version& version)
    {
        if (!version.valid)
            return QString("unknown");

        if (version.major == 0 && version.minor == 0 && version.revision == 0)
            return QString("build %1").arg(version.build);

        return version.build > 0
            ? QString("%1.%2.%3 build %4").arg(version.major).arg(version.minor)
                  .arg(version.revision).arg(version.build)
            : QString("%1.%2.%3").arg(version.major).arg(version.minor).arg(version.revision);
    }

    // What this build of the client is running on, in the words the release assets
    // use. Compiled in rather than detected, because the answer is a property of
    // the binary asking the question and not of the machine it happens to be on.
    inline QString platformKey()
    {
#if defined(Q_OS_WIN)
        return "windows";
#elif defined(Q_OS_MACOS)
    #if defined(Q_PROCESSOR_ARM)
        return "macos-arm64";
    #else
        return "macos-x86_64";
    #endif
#else
        return "linux";
#endif
    }

    // The one asset in a release that belongs on this platform.
    //
    // Names come from the workflows and look like
    // casparcg-client-v2.3.1-windows.zip, -macos-arm64.dmg, -ubuntu22.deb. Matching
    // on the platform word rather than only the extension keeps a checksums file or
    // a source archive from being mistaken for a package.
    inline QString assetFor(const QString& platform, const QStringList& names)
    {
        foreach (const QString& name, names)
        {
            const QString lower = name.toLower();

            // Never a candidate on any platform: it is what verifies the others.
            if (lower.contains("sha256") || lower.endsWith(".txt") || lower.endsWith(".asc"))
                continue;

            if (platform == "windows" && lower.contains("windows") && lower.endsWith(".zip"))
                return name;

            if (platform == "linux" && lower.endsWith(".deb"))
                return name;

            // arm64 before x86_64, and both spelled out, so a universal-looking name
            // cannot be handed to the wrong architecture.
            if (platform == "macos-arm64" && lower.contains("arm64") && lower.endsWith(".dmg"))
                return name;

            if (platform == "macos-x86_64" && lower.contains("x86_64") && lower.endsWith(".dmg"))
                return name;
        }

        return QString();
    }

    // A line of a SHA256SUMS file: the hash, two spaces, the file name.
    //
    // The releases API publishes a size and nothing else to verify against, and a
    // size is not a checksum - two different builds of this client are the same
    // size often enough that it would pass. So the release carries a sums file and
    // this reads the one line that matters out of it.
    //
    // Returned lower case, because the comparison against a locally computed hash
    // should not be able to fail on capitalisation.
    inline QString sha256For(const QString& assetName, const QString& sumsFile)
    {
        foreach (const QString& line, sumsFile.split('\n'))
        {
            const QString trimmed = line.trimmed();
            if (trimmed.isEmpty() || trimmed.startsWith('#'))
                continue;

            const int split = trimmed.indexOf(' ');
            if (split <= 0)
                continue;

            const QString hash = trimmed.left(split).trimmed();
            // Everything after the hash, less the "*" that marks binary mode and any
            // path the sums file was written with.
            QString named = trimmed.mid(split).trimmed();
            if (named.startsWith('*'))
                named = named.mid(1);
            named = named.section('/', -1).section('\\', -1);

            if (named != assetName)
                continue;

            // A hash of the wrong length is a malformed file rather than a match.
            // Accepting one would mean comparing against something that can never
            // equal a real digest, which fails closed but for the wrong reason.
            if (hash.length() != 64)
                return QString();

            foreach (const QChar& c, hash)
            {
                if (!c.isDigit() && !(c.toLower() >= 'a' && c.toLower() <= 'f'))
                    return QString();
            }

            return hash.toLower();
        }

        return QString();
    }
}

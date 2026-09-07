#pragma once

#include "Shared.h"

#include <QtCore/QByteArray>
#include <QtCore/QJsonObject>
#include <QtCore/QMap>
#include <QtCore/QString>

// Installing a template pack that was pushed from a dev machine.
//
// This is the receiving half of the push tool: the pusher walks a pack folder and
// sends one file at a time, and each one lands here. Everything that decides what
// may be written, and where, lives in this one place rather than being spread
// through the request router.
//
// Three rules, and none of them is negotiable from the wire:
//
//   * It is off unless switched on, and every request carries a token. A template
//     is HTML that CasparCG executes, so an open install endpoint on a playout
//     machine is a way to run code on it.
//   * Nothing escapes the pack folder. Paths are taken apart segment by segment
//     and the result is checked against the folder it must sit under.
//   * project.js and extensions.json are never written. The client itself edits
//     the first (API key, the local flag) and the Sheets panel writes the second,
//     so a push that replaced them would quietly undo operator settings.
class WIDGETS_EXPORT TemplateInstaller
{
    public:
        static bool isEnabled();
        static QString token();

        // Where packs live: the configured path, or the template path of the first
        // device that has one, so a normal setup needs nothing configured.
        static QString templatesRoot();

        // Held apart from the push because the operator, not the dev machine, owns
        // what is in them.
        static bool isProtected(const QString& relativePath);

        // Who this client is, for a pusher that may be looking at it across a
        // network and has only an address to go on.
        static QJsonObject identify();

        // A wrong token is the shape a probe takes. Repeated ones from the same
        // address are refused outright for a while, so an exposed endpoint cannot
        // be worked through at speed.
        static bool isThrottled(const QString& peer);
        static void noteBadToken(const QString& peer);
        static void noteGoodToken(const QString& peer);

        // Every file in a pack with the digest to compare it by, keyed on the
        // relative path. Git blob digests when gitStyle is set, so a pack can be
        // compared against a GitHub tree without fetching anything.
        static QMap<QString, QString> packDigests(const QString& pack, bool gitStyle);

        // sha1("blob <size>\0" + content), which is how Git names a file's contents
        // and therefore what a tree listing gives back.
        static QString gitBlobSha(const QByteArray& content);

        // The packs on this machine, with enough of a fingerprint to compare.
        static QJsonObject listPacks();
        static QJsonObject describePack(const QString& pack, bool* found = nullptr);

        // Writes one file into a pack. Answers the HTTP status to send back, so the
        // reason a file was refused survives all the way to the pusher's log.
        // expectedSha1 is optional. When given, the body is checked against it and
        // nothing is written unless it matches: a template that changed in transit
        // is the last thing to put where CasparCG will run it.
        static int installFile(const QString& pack, const QString& relativePath,
                               const QByteArray& body, QString* error,
                               const QString& expectedSha1 = QString());

        // A single path segment, no separators, no dots-only names.
        static bool isSafeSegment(const QString& segment);

        // A relative path that stays inside its pack once resolved.
        static bool isSafeRelativePath(const QString& relativePath);
};

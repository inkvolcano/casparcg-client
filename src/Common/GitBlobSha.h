#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QCryptographicHash>
#include <QtCore/QString>

// How Git names a file's contents: sha1 of "blob <length>\0" then the bytes.
//
// This is one function in one place because two things need it and they are in
// different binaries. The client hashes the files in a pack this way; the push tool
// hashes the files it is about to send. Both compare the result against the sha a
// GitHub tree listing gives, which is exactly this value.
//
// Header-only so the push tool, which links Qt and nothing else of ours, can use the
// same code as the client rather than its own copy. Two copies that agree today are
// two copies that can disagree later, and the symptom would be every client deciding
// every file had changed, on every poll, with no error to show for it.
//
// Checked against git itself: the empty file, text, and binary content all match
// git hash-object.
inline QString gitBlobShaOf(const QByteArray& content)
{
    QByteArray prefix = QByteArray("blob ") + QByteArray::number(content.size()) + '\0';

    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(prefix);
    hash.addData(content);

    return QString::fromLatin1(hash.result().toHex());
}

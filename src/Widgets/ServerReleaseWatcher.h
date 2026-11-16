#pragma once

#include "Shared.h"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QTimer>

QT_BEGIN_NAMESPACE
class QNetworkAccessManager;
QT_END_NAMESPACE

// Is a newer stable CasparCG Server out? For the Server Status panel's server
// menus, which mark a server running an older one.
//
// Only tells: it never downloads or installs anything - that is still Download
// Server Build..., chosen by a person. It reads the GitHub release list once a
// minute after the client starts and then once a day, anonymously, and can be
// turned off from the server menu (setting ServerReleaseCheck, on unless set to
// "false") for machines with no internet or none allowed.
class WIDGETS_EXPORT ServerReleaseWatcher : public QObject
{
    Q_OBJECT

    public:
        explicit ServerReleaseWatcher(QObject* parent = nullptr);

        // "2.5.1", or empty until a check has found one.
        QString latestStable() const { return this->latest; }

        static bool enabled();
        static void setEnabled(bool on);

        // Where server releases and nightly builds are listed (build 314): the
        // official places unless Settings names a mirror with the same layout.
        static QString releasesUrl();
        static QString nightliesUrl();

        Q_SIGNAL void changed();

    private:
        QNetworkAccessManager* network = nullptr;
        QTimer timer;
        QString latest;

        void check();
};

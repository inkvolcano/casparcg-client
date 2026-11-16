#pragma once

#include "Shared.h"

#include "ServerConfigXml.h"

#include <QtCore/QString>

// A server's own casparcg.config, read from beside its executable (the
// server path set for it in Settings > Servers). Read-only: for finding what
// the server was told - its NDI outputs, its scanner port - without asking it.
class WIDGETS_EXPORT ServerConfigFile
{
    public:
        // ok is false when the server has no path here, or no readable config.
        static ServerConfigXml::Document load(const QString& deviceName);

        // The scanner port for a server (build 314): from its casparcg.config
        // when that can be read, else Settings (ScannerPort), else 8000.
        // Remembered for a minute, so a lookup per click reads no file.
        static int scannerPort(const QString& deviceName);

    private:
        ServerConfigFile() {}
};

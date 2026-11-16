#pragma once

#include "Shared.h"

class QWidget;

// Starts the client again (build 316): closes the main window the ordinary way -
// so unsaved rundowns are asked about - and only once it has closed, starts a
// small script that waits for this process to be gone and then runs the client.
// Windows only; elsewhere it says so and does nothing.
class WIDGETS_EXPORT ClientRestart
{
    public:
        // false when the window stayed open (Cancel on a save question) or the
        // script could not be started.
        static bool restart(QWidget* mainWindow);

    private:
        ClientRestart() {}
};

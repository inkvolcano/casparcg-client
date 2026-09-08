#include "PushWindow.h"

#include <QtWidgets/QApplication>

// A separate, small tool: it talks to clients over HTTP and shares no code with
// them, so it can be run from a dev machine that has no CasparCG on it at all.
int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    application.setOrganizationName("CasparCG");
    application.setApplicationName("TemplatePush");

    PushWindow window;
    window.show();

    return application.exec();
}

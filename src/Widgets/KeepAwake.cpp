#include "KeepAwake.h"

#include "DatabaseManager.h"

#if defined(Q_OS_WIN)
#include <windows.h>
#endif

bool KeepAwake::isEnabled()
{
    return DatabaseManager::getInstance().getConfigurationByName("KeepAwake").getValue() != "false";
}

void KeepAwake::setEnabled(bool on)
{
    DatabaseManager::getInstance().updateConfiguration(ConfigurationModel(0, "KeepAwake", on ? "true" : "false"));
    apply();
}

void KeepAwake::apply()
{
#if defined(Q_OS_WIN)
    // Held for as long as this thread - the client's main thread - lives, so it
    // goes away with the client whatever way it ends.
    if (isEnabled())
        SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
    else
        SetThreadExecutionState(ES_CONTINUOUS);
#endif
}

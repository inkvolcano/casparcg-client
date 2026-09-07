// Test doubles for the database, so the path rules can be tested on their own.
//
// TemplateInstaller reads three settings from the database and does everything else
// with no dependencies at all. The functions under test - isSafeRelativePath,
// isProtected, gitBlobSha - never touch it, but they live in the same object file,
// so the linker still wants the symbols.
//
// These provide them and nothing else. If a test ever depends on one of these
// returning something real, that test is testing the wrong thing.
//
// ConfigurationModel is the real one, linked in: it is a plain value type with no
// database behind it, and a stub would only be a second thing to keep in step.

#include "DatabaseManager.h"
#include "Models/ConfigurationModel.h"
#include "Models/DeviceModel.h"

#include <QtCore/QList>
#include <QtCore/QString>

DatabaseManager& DatabaseManager::getInstance()
{
    // Never constructed and never read from. Every method reached through it is
    // stubbed below and ignores its object, so there is nothing here to touch.
    static char storage[sizeof(void*)] = { 0 };
    return *reinterpret_cast<DatabaseManager*>(storage);
}

ConfigurationModel DatabaseManager::getConfigurationByName(const QString& name)
{
    // Any setting a test needs, supplied from the environment as
    // CASPARCG_TEST_<Name>. Nothing is read from a real database, so a test can
    // point the installer at a temporary folder and run the server on a spare port
    // without touching anything the application uses.
    QString value = qEnvironmentVariable(("CASPARCG_TEST_" + name).toUtf8().constData());
    return ConfigurationModel(0, name, value);
}

QList<DeviceModel> DatabaseManager::getDevice()
{
    return QList<DeviceModel>();
}

void DatabaseManager::updateConfiguration(const ConfigurationModel& configuration)
{
    // Settings come from the environment in a test, so a write has nowhere to go
    // and nothing depends on it having gone there.
    Q_UNUSED(configuration)
}

DeviceModel DatabaseManager::getDeviceByName(const QString& name)
{
    Q_UNUSED(name)
    return DeviceModel(0, QString(), QString(), 0, QString(), QString(), QString(),
                       QString(), QString(), 0, QString(), 0, 0);
}

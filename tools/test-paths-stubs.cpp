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

const QString& DeviceModel::getTemplatePath() const
{
    static const QString empty;
    return empty;
}

DatabaseManager& DatabaseManager::getInstance()
{
    // Never constructed and never read from. Every method reached through it is
    // stubbed below and ignores its object, so there is nothing here to touch.
    static char storage[sizeof(void*)] = { 0 };
    return *reinterpret_cast<DatabaseManager*>(storage);
}

ConfigurationModel DatabaseManager::getConfigurationByName(const QString& name)
{
    // The one setting the install tests need to be real: where packs are written.
    // Taken from the environment so the test can point it at a temporary folder and
    // then check that nothing landed outside it.
    if (name == "TemplatePushPath")
        return ConfigurationModel(0, name, qEnvironmentVariable("CASPARCG_TEST_TEMPLATES"));

    return ConfigurationModel(0, name, QString());
}

QList<DeviceModel> DatabaseManager::getDevice()
{
    return QList<DeviceModel>();
}

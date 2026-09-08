#include <QtWidgets/QApplication>

// Runner functions defined in each test .cpp file.
int runXmlEncodeDecodeTest(int argc, char* argv[]);
int runChannelColorTest(int argc, char* argv[]);
int runKeyValueModelTest(int argc, char* argv[]);
int runDeviceModelTest(int argc, char* argv[]);
int runOscSubscriptionRegistryTest(int argc, char* argv[]);
int runCloneGroupRegistryTest(int argc, char* argv[]);
int runTriggerBankRegistryTest(int argc, char* argv[]);
int runEventManagerTest(int argc, char* argv[]);
int runCommandSerializationTest(int argc, char* argv[]);
int runGatewayCommandTest(int argc, char* argv[]);
int runTemplateCommandTest(int argc, char* argv[]);
int runDeviceManagerLockingTest(int argc, char* argv[]);
int runShellCommandTest(int argc, char* argv[]);
int runAutoSaveTest(int argc, char* argv[]);

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    int status = 0;
    status |= runXmlEncodeDecodeTest(argc, argv);
    status |= runChannelColorTest(argc, argv);
    status |= runKeyValueModelTest(argc, argv);
    status |= runDeviceModelTest(argc, argv);
    status |= runOscSubscriptionRegistryTest(argc, argv);
    status |= runCloneGroupRegistryTest(argc, argv);
    status |= runTriggerBankRegistryTest(argc, argv);
    status |= runEventManagerTest(argc, argv);
    status |= runCommandSerializationTest(argc, argv);
    status |= runGatewayCommandTest(argc, argv);
    status |= runTemplateCommandTest(argc, argv);
    status |= runDeviceManagerLockingTest(argc, argv);
    status |= runShellCommandTest(argc, argv);
    status |= runAutoSaveTest(argc, argv);

    return status;
}

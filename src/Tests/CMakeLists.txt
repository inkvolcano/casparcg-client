# CasparCG Client Unit Tests

find_package(Qt6 REQUIRED COMPONENTS Test)

qt_add_executable(casparcg_tests
    main.cpp
    XmlEncodeDecodeTest.cpp
    ChannelColorTest.cpp
    KeyValueModelTest.cpp
    DeviceModelTest.cpp
    OscSubscriptionRegistryTest.cpp
    CloneGroupRegistryTest.cpp
    TriggerBankRegistryTest.cpp
    EventManagerTest.cpp
    CommandSerializationTest.cpp
    GatewayCommandTest.cpp
    TemplateCommandTest.cpp
    DeviceManagerLockingTest.cpp
    ShellCommandTest.cpp
    AutoSaveTest.cpp
    OscPacketGuardTest.cpp
)
add_external_dependencies(casparcg_tests)

target_include_directories(casparcg_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../Core
    ${CMAKE_CURRENT_SOURCE_DIR}/../Common
    ${CMAKE_CURRENT_SOURCE_DIR}/../Caspar
    ${CMAKE_CURRENT_SOURCE_DIR}/../Osc
    ${CMAKE_CURRENT_SOURCE_DIR}/../Gpi
    ${CMAKE_CURRENT_SOURCE_DIR}/../Widgets
    ${CMAKE_CURRENT_SOURCE_DIR}/../Widgets/Rundown
)

target_link_libraries(casparcg_tests PRIVATE
    core
    common
    caspar
    gpi
    osc
    widgets

    Qt::Core
    Qt::Gui
    Qt::Sql
    Qt::Widgets
    Qt::Test
)

enable_testing()
add_test(NAME CasparCGTests COMMAND casparcg_tests)

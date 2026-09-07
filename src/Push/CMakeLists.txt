# The push tool. Standalone on purpose: only Qt, no link against the client's own
# libraries, so it builds and runs on a machine that has none of the rest.
qt_add_executable(templatepush WIN32
    Main.cpp
    PushWindow.cpp
    PushWindow.h
)

set_target_properties(templatepush PROPERTIES OUTPUT_NAME "CasparCG Template Push")

target_link_libraries(templatepush PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Widgets
)

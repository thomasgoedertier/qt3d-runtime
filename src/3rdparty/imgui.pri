INCLUDEPATH += $$PWD/imgui

# gcc 4.9 (aarch64; from Android NDK r13b) complains about "'spc.stbtt_pack_context::nodes' may be used uninitialized in this function"
gcc:!clang: QMAKE_CXXFLAGS_WARN_ON += -Wno-error=maybe-uninitialized

SOURCES += \
    $$PWD/imgui/imgui.cpp \
    $$PWD/imgui/imgui_draw.cpp

SOURCES += \
    $$PWD/imgui/imgui_demo.cpp

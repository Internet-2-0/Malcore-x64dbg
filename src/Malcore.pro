#-------------------------------------------------
#
# Project created by QtCreator 2015-07-07T21:06:57
#
#-------------------------------------------------

##
## Pre-defined global variables
##

CONFIG(debug, debug|release) {
    DIR_SUFFIX = d
} else {
    DIR_SUFFIX =
}

!contains(QMAKE_HOST.arch, x86_64) {
    X64_BIN_DIR = ../bin/x32$${DIR_SUFFIX}/plugins # Relative BIN path, 32-bit
    X64_GEN_DIR = ../gui_build/out32$${DIR_SUFFIX} # QMake temporary generated files, placed inside the build folder. (OBJ, UI, MOC)
    QMAKE_EXTENSION_SHLIB = dp32
} else {
    X64_BIN_DIR = ../bin/x64$${DIR_SUFFIX}/plugins # Relative BIN path, 64-bit
    X64_GEN_DIR = ../gui_build/out64$${DIR_SUFFIX} # QMake temporary generated files, placed inside the build folder. (OBJ, UI, MOC)
    QMAKE_EXTENSION_SHLIB = dp64
}

##
## QMake output directories
##
DESTDIR = $${X64_BIN_DIR}

QT       += core gui widgets network

#generate debug symbols in release mode
QMAKE_CFLAGS_RELEASE += -Zi
QMAKE_LFLAGS_RELEASE += /DEBUG

# http://www.hexblog.com/?p=991
QMAKE_CXXFLAGS += -DQT_NO_UNICODE_LITERAL

TARGET = Malcore
TEMPLATE = lib
LIBS += -luser32 -lshlwapi

!contains(QMAKE_HOST.arch, x86_64) {
    LIBS += -lx32dbg -lx32bridge -L"$$PWD/pluginsdk"
} else {
    LIBS += -lx64dbg -lx64bridge -L"$$PWD/pluginsdk"
}

SOURCES +=\
    pluginmain.cpp \
    QtPlugin.cpp \
    PluginMainWindow.cpp \
    LoginDialog.cpp

HEADERS += \
    pluginmain.h \
    QtPlugin.h \
    PluginMainWindow.h \
    LoginDialog.h \
    MalcoreReport.h \
    pluginsdk/dbghelp/dbghelp.h \
    pluginsdk/DeviceNameResolver/DeviceNameResolver.h \
    pluginsdk/jansson/jansson.h \
    pluginsdk/jansson/jansson_config.h \
    pluginsdk/jansson/jansson_x64dbg.h \
    pluginsdk/lz4/lz4.h \
    pluginsdk/lz4/lz4file.h \
    pluginsdk/lz4/lz4hc.h \
    pluginsdk/TitanEngine/TitanEngine.h \
    pluginsdk/XEDParse/XEDParse.h \
    pluginsdk/_dbgfunctions.h \
    pluginsdk/_plugin_types.h \
    pluginsdk/_plugins.h \
    pluginsdk/_scriptapi.h \
    pluginsdk/_scriptapi_assembler.h \
    pluginsdk/_scriptapi_debug.h \
    pluginsdk/_scriptapi_gui.h \
    pluginsdk/_scriptapi_memory.h \
    pluginsdk/_scriptapi_misc.h \
    pluginsdk/_scriptapi_module.h \
    pluginsdk/_scriptapi_pattern.h \
    pluginsdk/_scriptapi_register.h \
    pluginsdk/_scriptapi_stack.h \
    pluginsdk/_scriptapi_argument.h \
    pluginsdk/_scriptapi_bookmark.h \
    pluginsdk/_scriptapi_comment.h \
    pluginsdk/_scriptapi_flag.h \
    pluginsdk/_scriptapi_function.h \
    pluginsdk/_scriptapi_label.h \
    pluginsdk/_scriptapi_symbol.h \
    pluginsdk/bridgegraph.h \
    pluginsdk/bridgelist.h \
    pluginsdk/bridgemain.h \

FORMS += \
    LoginDialog.ui \
    PluginMainWindow.ui

RESOURCES += \
    resource.qrc

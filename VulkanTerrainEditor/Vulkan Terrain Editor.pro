# ----------------------------------------------------
# This file is generated by the Qt Visual Studio Tools.
# ------------------------------------------------------

TEMPLATE = app
TARGET = Vulkan Terrain Editor
DESTDIR = ./x64/Debug
QT += core opengl network gui widgets openglextensions concurrent
CONFIG += debug
DEFINES += _UNICODE WIN64 QT_DLL QT_NETWORK_LIB QT_OPENGL_LIB QT_OPENGLEXTENSIONS_LIB QT_WIDGETS_LIB
LIBS += -lopengl32 \
    -lglu32

target.path = $$OUT_PWD

INSTALLS += target

include(Vulkan Terrain Editor.pri)

HEADERS += \
    Common/Shader.h

SOURCES += \
    Common/Shader.cpp

TEMPLATE = lib
QT += core-private gui-private
TARGET = QtD3D12Window

load(qt_module)

DEFINES += QD3D12_BUILD_DLL

SOURCES += $$PWD/qd3d12window.cpp

HEADERS += $$PWD/qd3d12window.h \
           $$PWD/qd3d12windowglobal.h

LIBS += -ldxgi -ld3d12

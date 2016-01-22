TEMPLATE = app
QT += d3d12window widgets

SOURCES = main.cpp window.cpp
HEADERS = window.h

LIBS = -ld3d12

CS = tdr.hlsl

shader.input = CS
shader.header = tdr.h
shader.entry = timeout
shader.type = cs_5_0

HLSL_SHADERS = shader
load(hlsl)

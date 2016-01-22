TEMPLATE = app
QT += d3d12window widgets

SOURCES = main.cpp window.cpp
HEADERS = window.h

LIBS = -ld3d12

VSPS = shader.hlsl

vshader.input = VSPS
vshader.header = shader_vs.h
vshader.entry = VS_Simple
vshader.type = vs_5_0

pshader.input = VSPS
pshader.header = shader_ps.h
pshader.entry = PS_Simple
pshader.type = ps_5_0

HLSL_SHADERS = vshader pshader
load(hlsl)

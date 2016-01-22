TEMPLATE = app
QT += d3d12window widgets

SOURCES = main.cpp window.cpp
HEADERS = window.h

LIBS = -ld3d12

VSPS = shader.hlsl

vshader1.input = VSPS
vshader1.header = shader_vs_off.h
vshader1.entry = VS_Offscreen
vshader1.type = vs_5_0

pshader1.input = VSPS
pshader1.header = shader_ps_off.h
pshader1.entry = PS_Offscreen
pshader1.type = ps_5_0

vshader2.input = VSPS
vshader2.header = shader_vs_on.h
vshader2.entry = VS_Onscreen
vshader2.type = vs_5_0

pshader2.input = VSPS
pshader2.header = shader_ps_on.h
pshader2.entry = PS_Onscreen
pshader2.type = ps_5_0

HLSL_SHADERS = vshader1 pshader1 vshader2 pshader2
load(hlsl)

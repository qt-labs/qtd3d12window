TEMPLATE = app
QT += d3d12window widgets

SOURCES = main.cpp window.cpp
HEADERS = window.h
RESOURCES = hellogpumipmap.qrc

LIBS = -ld3d12

VSPS = shader.hlsl
CS = mipmapgen.hlsl

vshader.input = VSPS
vshader.header = shader_vs.h
vshader.entry = VS_Texture
vshader.type = vs_5_0

pshader.input = VSPS
pshader.header = shader_ps.h
pshader.entry = PS_Texture
pshader.type = ps_5_0

cshader.input = CS
cshader.header = shader_cs.h
cshader.entry = CS_Generate4MipMaps
cshader.type = cs_5_0

HLSL_SHADERS = vshader pshader cshader
load(hlsl)

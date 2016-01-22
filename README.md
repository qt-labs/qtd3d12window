QtD3D12Window is a Qt 5.6 module providing a QD3D12Window class
similar to QOpenGLWindow, a handy qmake rule for offline HLSL shader
compilation via fxc, and a number of basic examples.

In the .pro file, add:

    QT += d3d12window

Then use it like QOpenGLWindow:

    class Window : public QD3D12Window
    {
    public:
        void initializeD3D() { create command list etc. }
        void resizeD3D(const QSize &size) { ... }
        void paintD3D() { populate and execute command list }
        void afterPresent() { wait as necessary }
    };

Use QWidget::createWindowContainer() to embed into widget-based UIs.

To use the qmake rule to generate headers from shaders at build time,
copy hlsl.prf to mkspecs/features folder of the Qt SDK.

    VSPS = shader.hlsl

    vshader.input = VSPS
    vshader.header = shader_vs.h
    vshader.entry = VS_MyShader
    vshader.type = vs_5_0

    pshader.input = VSPS
    pshader.header = shader_ps.h
    pshader.entry = PS_MyShader
    pshader.type = ps_5_0

    HLSL_SHADERS = vshader pshader
    load(hlsl)

Examples in order of increasing complexity:

1. hellowindow - Bringing up a window and clearing the backbuffer

2. hellodevicereset - Handle device removed errors and make the application able to survive a driver update, shader timeout, etc.

3. hellotriangle - Basic rendering, shader and pipeline setup, constant buffers

4. hellotexture - Texturing, mipmaps

5. hellooffscreen - Rendering into offscreen render targets, then using them from the pixel shader

       hellooffscreen_opengl - OpenGL version, for comparing performance, overhead, etc.

6. hellomultisample - Rendering with MSAA

7. hellocompressedtexture - Compressed textures loaded from DDS files

8. hellogpumipmap - Mipmap generation via compute shaders

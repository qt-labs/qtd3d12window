/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the QtD3D12Window module
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "window.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QOpenGLFramebufferObject>
#include <QOpenGLShaderProgram>
#include <QOpenGLFunctions>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>

static const int OFFSCREEN_WIDTH = 512;
static const int OFFSCREEN_HEIGHT = 512;

static const float offscreenClearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
static const float onscreenClearColor[] = { 0.4f, 0.5f, 0.5f, 1.0f };

Window::Window()
{
    QSurfaceFormat fmt;
    fmt.setDepthBufferSize(24);
    setFormat(fmt);
}

Window::~Window()
{
    makeCurrent();

    delete offscreen.fbo;
    delete offscreen.prog;
    delete offscreen.vbo;
    delete offscreen.vao;

    delete onscreen.prog;
    delete onscreen.vbo;
    delete onscreen.vao;
}

void Window::initializeGL()
{
    initializeOffscreen();
    initializeOnscreen();

    QOpenGLFunctions *f = context()->functions();
    f->glEnable(GL_DEPTH_TEST);
    f->glEnable(GL_CULL_FACE);
}

void Window::initializeOffscreen()
{
    offscreen.fbo = new QOpenGLFramebufferObject(OFFSCREEN_WIDTH, OFFSCREEN_HEIGHT, QOpenGLFramebufferObject::CombinedDepthStencil);

    offscreen.prog = new QOpenGLShaderProgram;
    offscreen.prog->addShaderFromSourceFile(QOpenGLShader::Vertex, QStringLiteral(":/shader_offscreen.vert"));
    offscreen.prog->addShaderFromSourceFile(QOpenGLShader::Fragment, QStringLiteral(":/shader_offscreen.frag"));
    offscreen.prog->bindAttributeLocation("position", 0);
    offscreen.prog->bindAttributeLocation("color", 1);
    offscreen.prog->link();

    offscreen.modelviewLoc = offscreen.prog->uniformLocation("modelview");
    offscreen.projectionLoc = offscreen.prog->uniformLocation("projection");

    offscreen.vao = new QOpenGLVertexArrayObject;
    offscreen.vao->create();
    QOpenGLVertexArrayObject::Binder vaoBinder(offscreen.vao);

    offscreen.vbo = new QOpenGLBuffer;
    offscreen.vbo->create();
    offscreen.vbo->bind();

    const float vertices[] = {
        0.0f, 0.707f, 0.0f, /* color */ 1.0f, 0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, 0.0f,             0.0f, 1.0f, 0.0f, 1.0f,
        0.5f, -0.5f, 0.0f,              0.0f, 0.0f, 1.0f, 1.0f
    };

    const int vertexCount = 3;
    offscreen.vbo->allocate(sizeof(GLfloat) * vertexCount * 7);
    offscreen.vbo->write(0, vertices, sizeof(GLfloat) * vertexCount * 7);

    if (offscreen.vao->isCreated())
        setupOffscreenVertexAttribs();

    offscreen.projection.perspective(60.0f, OFFSCREEN_WIDTH / float(OFFSCREEN_HEIGHT), 0.1f, 100.0f);
}

void Window::initializeOnscreen()
{
    onscreen.prog = new QOpenGLShaderProgram;
    onscreen.prog->addShaderFromSourceFile(QOpenGLShader::Vertex, QStringLiteral(":/shader_onscreen.vert"));
    onscreen.prog->addShaderFromSourceFile(QOpenGLShader::Fragment, QStringLiteral(":/shader_onscreen.frag"));
    onscreen.prog->bindAttributeLocation("position", 0);
    onscreen.prog->bindAttributeLocation("texcoord", 1);
    onscreen.prog->link();

    onscreen.modelviewLoc = onscreen.prog->uniformLocation("modelview");
    onscreen.projectionLoc = onscreen.prog->uniformLocation("projection");

    onscreen.vao = new QOpenGLVertexArrayObject;
    onscreen.vao->create();
    QOpenGLVertexArrayObject::Binder vaoBinder(onscreen.vao);

    onscreen.vbo = new QOpenGLBuffer;
    onscreen.vbo->create();
    onscreen.vbo->bind();

    GLfloat v[] = {
        -0.5, 0.5, 0.5, 0.5,-0.5,0.5,-0.5,-0.5,0.5,
        0.5, -0.5, 0.5, -0.5,0.5,0.5,0.5,0.5,0.5,
        -0.5, -0.5, -0.5, 0.5,-0.5,-0.5,-0.5,0.5,-0.5,
        0.5, 0.5, -0.5, -0.5,0.5,-0.5,0.5,-0.5,-0.5,

        0.5, -0.5, -0.5, 0.5,-0.5,0.5,0.5,0.5,-0.5,
        0.5, 0.5, 0.5, 0.5,0.5,-0.5,0.5,-0.5,0.5,
        -0.5, 0.5, -0.5, -0.5,-0.5,0.5,-0.5,-0.5,-0.5,
        -0.5, -0.5, 0.5, -0.5,0.5,-0.5,-0.5,0.5,0.5,

        0.5, 0.5,  -0.5, -0.5, 0.5,  0.5,  -0.5,  0.5,  -0.5,
        -0.5,  0.5,  0.5,  0.5,  0.5,  -0.5, 0.5, 0.5,  0.5,
        -0.5,  -0.5, -0.5, -0.5, -0.5, 0.5,  0.5, -0.5, -0.5,
        0.5, -0.5, 0.5,  0.5,  -0.5, -0.5, -0.5,  -0.5, 0.5
    };
    GLfloat texCoords[] = {
        0.0f,0.0f, 1.0f,1.0f, 1.0f,0.0f,
        1.0f,1.0f, 0.0f,0.0f, 0.0f,1.0f,
        1.0f,1.0f, 1.0f,0.0f, 0.0f,1.0f,
        0.0f,0.0f, 0.0f,1.0f, 1.0f,0.0f,

        1.0f,1.0f, 1.0f,0.0f, 0.0f,1.0f,
        0.0f,0.0f, 0.0f,1.0f, 1.0f,0.0f,
        0.0f,0.0f, 1.0f,1.0f, 1.0f,0.0f,
        1.0f,1.0f, 0.0f,0.0f, 0.0f,1.0f,

        0.0f,1.0f, 1.0f,0.0f, 1.0f,1.0f,
        1.0f,0.0f, 0.0f,1.0f, 0.0f,0.0f,
        1.0f,0.0f, 1.0f,1.0f, 0.0f,0.0f,
        0.0f,1.0f, 0.0f,0.0f, 1.0f,1.0f
    };

    const int vertexCount = 36;
    onscreen.vbo->allocate(sizeof(GLfloat) * vertexCount * 5);
    onscreen.vbo->write(0, v, sizeof(GLfloat) * vertexCount * 3);
    onscreen.vbo->write(sizeof(GLfloat) * vertexCount * 3, texCoords, sizeof(GLfloat) * vertexCount * 2);

    if (onscreen.vao->isCreated())
        setupOnscreenVertexAttribs();

    onscreen.projection.perspective(60.0f, width() / float(height()), 0.1f, 100.0f);
}

void Window::resizeGL(int, int)
{
    onscreen.projection.setToIdentity();
    onscreen.projection.perspective(60.0f, width() / float(height()), 0.1f, 100.0f);
}

void Window::paintGL()
{
    paintOffscreen();
    paintOnscreen();

    update();
}

void Window::setupOffscreenVertexAttribs()
{
    QOpenGLFunctions *f = context()->functions();
    offscreen.vbo->bind();
    offscreen.prog->enableAttributeArray(0);
    offscreen.prog->enableAttributeArray(1);
    f->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(GLfloat), 0);
    f->glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(GLfloat), (const void *)(3 * sizeof(GLfloat)));
    offscreen.vbo->release();
}

void Window::paintOffscreen()
{
    QOpenGLFunctions *f = context()->functions();

    offscreen.fbo->bind();
    offscreen.prog->bind();
    QOpenGLVertexArrayObject::Binder vaoBinder(offscreen.vao);
    if (!offscreen.vao->isCreated())
        setupOffscreenVertexAttribs();

    QMatrix4x4 modelview;
    modelview.translate(0, 0, -2);
    modelview.rotate(offscreen.rotationAngle, 0, 0, 1);
    offscreen.rotationAngle += 1;
    offscreen.prog->setUniformValue(offscreen.modelviewLoc, modelview);
    offscreen.prog->setUniformValue(offscreen.projectionLoc, offscreen.projection);

    f->glViewport(0, 0, OFFSCREEN_WIDTH, OFFSCREEN_HEIGHT);
    f->glClearColor(offscreenClearColor[0], offscreenClearColor[1], offscreenClearColor[2], offscreenClearColor[3]);
    f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    f->glFrontFace(GL_CCW);
    f->glDrawArrays(GL_TRIANGLES, 0, 3);

    offscreen.fbo->release();
}

void Window::setupOnscreenVertexAttribs()
{
    QOpenGLFunctions *f = context()->functions();
    onscreen.vbo->bind();
    onscreen.prog->enableAttributeArray(0);
    onscreen.prog->enableAttributeArray(1);
    f->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    f->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (const void *)(36 * 3 * sizeof(GLfloat)));
    onscreen.vbo->release();
}

void Window::paintOnscreen()
{
    QOpenGLFunctions *f = context()->functions();

    onscreen.prog->bind();
    QOpenGLVertexArrayObject::Binder vaoBinder(onscreen.vao);
    if (!onscreen.vao->isCreated())
        setupOnscreenVertexAttribs();

    f->glBindTexture(GL_TEXTURE_2D, offscreen.fbo->texture());

    QMatrix4x4 modelview;
    modelview.translate(0, 0, -2);
    modelview.rotate(onscreen.rotationAngle, 1, 0.5, 0);
    onscreen.rotationAngle += 1;
    onscreen.prog->setUniformValue(onscreen.modelviewLoc, modelview);
    onscreen.prog->setUniformValue(onscreen.projectionLoc, onscreen.projection);

    f->glViewport(0, 0, width(), height());
    f->glClearColor(onscreenClearColor[0], onscreenClearColor[1], onscreenClearColor[2], onscreenClearColor[3]);
    f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    f->glFrontFace(GL_CW);
    f->glDrawArrays(GL_TRIANGLES, 0, 36);
}

void Window::readbackAndSave()
{
    QImage img = offscreen.fbo->toImage();
    QString fn = QFileDialog::getSaveFileName(Q_NULLPTR, QStringLiteral("Save PNG"), QString(), QStringLiteral("PNG files (*.png)"));
    if (!fn.isEmpty()) {
        img.save(fn);
        QMessageBox::information(Q_NULLPTR, QStringLiteral("Saved"), QStringLiteral("Offscreen render target read back and saved to ") + fn);
    }
}

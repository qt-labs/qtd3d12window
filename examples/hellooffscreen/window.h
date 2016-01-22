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

#include <QD3D12Window>
#include <QMatrix4x4>

class Window : public QD3D12Window
{
public:
    Window();
    ~Window();

    void initializeD3D() Q_DECL_OVERRIDE;
    void resizeD3D(const QSize &size) Q_DECL_OVERRIDE;
    void paintD3D() Q_DECL_OVERRIDE;
    void afterPresent() Q_DECL_OVERRIDE;

public slots:
    void readbackAndSave();

private:
    void initializeOffscreen();
    void initializeOnscreen();
    void paintOffscreen();
    void paintOnscreen();

    Fence *f;
    ComPtr<ID3D12GraphicsCommandList> commandList;
    ComPtr<ID3D12DescriptorHeap> cbvSrvHeap;

    struct OffscreenData {
        OffscreenData() : cbPtr(Q_NULLPTR), rotationAngle(0) { }
        ComPtr<ID3D12Resource> rt;
        ComPtr<ID3D12Resource> ds;
        ComPtr<ID3D12GraphicsCommandList> bundle;
        ComPtr<ID3D12PipelineState> pipelineState;
        ComPtr<ID3D12RootSignature> rootSignature;
        ComPtr<ID3D12Resource> vertexBuffer;
        ComPtr<ID3D12Resource> constantBuffer;
        quint8 *cbPtr;
        D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
        QMatrix4x4 projection;
        float rotationAngle;
    } offscreen;

    struct OnscreenData {
        OnscreenData() : cbPtr(Q_NULLPTR), rotationAngle(0) { }
        ComPtr<ID3D12GraphicsCommandList> bundle;
        ComPtr<ID3D12PipelineState> pipelineState;
        ComPtr<ID3D12RootSignature> rootSignature;
        ComPtr<ID3D12Resource> vertexBuffer;
        ComPtr<ID3D12Resource> constantBuffer;
        quint8 *cbPtr;
        D3D12_VERTEX_BUFFER_VIEW vertexBufferView[2];
        QMatrix4x4 projection;
        float rotationAngle;
    } onscreen;
};

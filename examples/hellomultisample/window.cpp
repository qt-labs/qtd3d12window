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
#include "shader_vs.h"
#include "shader_ps.h"

static const int OFFSCREEN_SAMPLES = 4;

static const float offscreenClearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };

Window::Window()
    : f(Q_NULLPTR),
      cbPtr(Q_NULLPTR),
      rotationAngle(0)
{
    setExtraRenderTargetCount(1);
}

Window::~Window()
{
    if (cbPtr)
        constantBuffer->Unmap(0, Q_NULLPTR);

    delete f;
}

void Window::setupOffscreenWithMatchingSize()
{
    msaaRT = Q_NULLPTR;
    msaaDS = Q_NULLPTR;
    msaaRT.Attach(createExtraRenderTargetAndView(extraRenderTargetCPUHandle(0), size(), offscreenClearColor, OFFSCREEN_SAMPLES));
    msaaDS.Attach(createExtraDepthStencilAndView(extraDepthStencilCPUHandle(0), size(), OFFSCREEN_SAMPLES));

    projection.setToIdentity();
    projection.perspective(60.0f, width() / float(height()), 0.1f, 100.0f);
}

void Window::initializeD3D()
{
    f = createFence();
    ID3D12Device *dev = device();

    setupOffscreenWithMatchingSize();

    D3D12_ROOT_PARAMETER rootParameter;
    rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameter.Descriptor.ShaderRegister = 0; // b0
    rootParameter.Descriptor.RegisterSpace = 0;

    D3D12_ROOT_SIGNATURE_DESC desc;
    desc.NumParameters = 1;
    desc.pParameters = &rootParameter;
    desc.NumStaticSamplers = 0;
    desc.pStaticSamplers = Q_NULLPTR;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error))) {
        qWarning("Failed to serialize root signature");
        return;
    }
    if (FAILED(dev->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature)))) {
        qWarning("Failed to create root signature");
        return;
    }

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    D3D12_SHADER_BYTECODE vshader;
    vshader.pShaderBytecode = g_VS_Simple;
    vshader.BytecodeLength = sizeof(g_VS_Simple);
    D3D12_SHADER_BYTECODE pshader;
    pshader.pShaderBytecode = g_PS_Simple;
    pshader.BytecodeLength = sizeof(g_PS_Simple);

    D3D12_RASTERIZER_DESC rastDesc = {};
    rastDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rastDesc.CullMode = D3D12_CULL_MODE_BACK;
    rastDesc.FrontCounterClockwise = TRUE; // Vertices are given CCW
    rastDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rastDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rastDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rastDesc.DepthClipEnable = TRUE;

    // No blending, just enable color write.
    D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc = {};
    defaultRenderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    D3D12_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0] = defaultRenderTargetBlendDesc;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature = rootSignature.Get();
    psoDesc.VS = vshader;
    psoDesc.PS = pshader;
    psoDesc.RasterizerState = rastDesc;
    psoDesc.BlendState = blendDesc;
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc = msaaRT->GetDesc().SampleDesc; // use multisampling
    if (FAILED(dev->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState)))) {
        qWarning("Failed to create graphics pipeline state");
        return;
    }

    if (FAILED(dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator(), Q_NULLPTR, IID_PPV_ARGS(&commandList)))) {
        qWarning("Failed to create command list");
        return;
    }
    commandList->Close();

    const float vertices[] = {
        0.0f, 0.707f, 0.0f, /* color */ 1.0f, 0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, 0.0f,             0.0f, 1.0f, 0.0f, 1.0f,
        0.5f, -0.5f, 0.0f,              0.0f, 0.0f, 1.0f, 1.0f
    };
    const UINT vertexBufferSize = sizeof(vertices);

    D3D12_HEAP_PROPERTIES heapProp = {};
    heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bufDesc;
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Alignment = 0;
    bufDesc.Width = vertexBufferSize;
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.SampleDesc.Quality = 0;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    if (FAILED(dev->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &bufDesc,
                                            D3D12_RESOURCE_STATE_GENERIC_READ, Q_NULLPTR, IID_PPV_ARGS(&vertexBuffer)))) {
        qWarning("Failed to create committed resource (vertex buffer)");
        return;
    }

    quint8 *p = Q_NULLPTR;
    D3D12_RANGE readRange = { 0, 0 };
    if (FAILED(vertexBuffer->Map(0, &readRange, reinterpret_cast<void **>(&p)))) {
        qWarning("Map failed");
        return;
    }
    memcpy(p, vertices, vertexBufferSize);
    vertexBuffer->Unmap(0, Q_NULLPTR);

    vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
    vertexBufferView.StrideInBytes = (3 + 4) * sizeof(float);
    vertexBufferView.SizeInBytes = vertexBufferSize;

    const UINT CB_SIZE = alignedCBSize(128); // 2 * float4x4
    bufDesc.Width = CB_SIZE;
    if (FAILED(dev->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &bufDesc,
                                            D3D12_RESOURCE_STATE_GENERIC_READ, Q_NULLPTR, IID_PPV_ARGS(&constantBuffer)))) {
        qWarning("Failed to create committed resource (constant buffer)");
        return;
    }

    if (FAILED(constantBuffer->Map(0, &readRange, reinterpret_cast<void **>(&p)))) {
        qWarning("Map failed (constant buffer)");
        return;
    }
    cbPtr = p;
}

void Window::resizeD3D(const QSize &)
{
    setupOffscreenWithMatchingSize();
}

void Window::paintD3D()
{
    modelview.setToIdentity();
    modelview.translate(0, 0, -2);
    // our highly sophisticated animation
    modelview.rotate(rotationAngle, 0, 0, 1);
    rotationAngle += 1;

    memcpy(cbPtr, modelview.constData(), 16 * sizeof(float));
    memcpy(cbPtr + 16 * sizeof(float), projection.constData(), 16 * sizeof(float));

    commandAllocator()->Reset();
    commandList->Reset(commandAllocator(), pipelineState.Get());

    commandList->SetGraphicsRootSignature(rootSignature.Get());

    commandList->SetGraphicsRootConstantBufferView(0, constantBuffer->GetGPUVirtualAddress());

    D3D12_VIEWPORT viewport = { 0, 0, float(width()), float(height()), 0, 1 };
    commandList->RSSetViewports(1, &viewport);
    D3D12_RECT scissorRect = { 0, 0, width() - 1, height() - 1 };
    commandList->RSSetScissorRects(1, &scissorRect);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = extraRenderTargetCPUHandle(0);
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = extraDepthStencilCPUHandle(0);
    commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    commandList->ClearRenderTargetView(rtvHandle, offscreenClearColor, 0, Q_NULLPTR);
    commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, Q_NULLPTR);

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
    commandList->DrawInstanced(3, 1, 0, 0);

    transitionResource(msaaRT.Get(), commandList.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
    transitionResource(backBufferRenderTarget(), commandList.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RESOLVE_DEST);
    commandList->ResolveSubresource(backBufferRenderTarget(), 0, msaaRT.Get(), 0, DXGI_FORMAT_R8G8B8A8_UNORM);
    transitionResource(backBufferRenderTarget(), commandList.Get(), D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_PRESENT);
    transitionResource(msaaRT.Get(), commandList.Get(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList->Close();

    ID3D12CommandList *commandLists[] = { commandList.Get() };
    commandQueue()->ExecuteCommandLists(_countof(commandLists), commandLists);

    update();
}

void Window::afterPresent()
{
    waitForGPU(f);
}

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
#include "shader_vs_off.h"
#include "shader_ps_off.h"
#include "shader_vs_on.h"
#include "shader_ps_on.h"
#include <QFileDialog>
#include <QMessageBox>

static const int OFFSCREEN_WIDTH = 512;
static const int OFFSCREEN_HEIGHT = 512;

static const float offscreenClearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
static const float onscreenClearColor[] = { 0.4f, 0.5f, 0.5f, 1.0f };

Window::Window()
    : f(Q_NULLPTR)
{
    setExtraRenderTargetCount(1);
}

Window::~Window()
{
    if (offscreen.cbPtr)
        offscreen.constantBuffer->Unmap(0, Q_NULLPTR);
    if (onscreen.cbPtr)
        onscreen.constantBuffer->Unmap(0, Q_NULLPTR);

    delete f;
}

void Window::initializeD3D()
{
    f = createFence();
    ID3D12Device *dev = device();

    if (FAILED(dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator(), Q_NULLPTR, IID_PPV_ARGS(&commandList)))) {
        qWarning("Failed to create command list");
        return;
    }
    commandList->Close(); // created in recording state, close it for now

    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
    cbvHeapDesc.NumDescriptors = 1;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    if (FAILED(dev->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&cbvSrvHeap)))) {
        qWarning("Failed to create CBV/SRV/UAV descriptor heap");
        return;
    }

    initializeOffscreen();
    initializeOnscreen();
}

static inline D3D12_GRAPHICS_PIPELINE_STATE_DESC pso()
{
    D3D12_RASTERIZER_DESC rastDesc = {};
    rastDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rastDesc.CullMode = D3D12_CULL_MODE_BACK;
    rastDesc.FrontCounterClockwise = TRUE;
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
    psoDesc.SampleDesc.Count = 1;

    return psoDesc;
}

void Window::initializeOffscreen()
{
    ID3D12Device *dev = device();

    // Create an offscreen render target of size 512x512. Pass the clear color to avoid performance warnings.
    // Have a depth-stencil buffer as well with the matching size.
    QSize sz(OFFSCREEN_WIDTH, OFFSCREEN_HEIGHT);
    offscreen.rt.Attach(createExtraRenderTargetAndView(extraRenderTargetCPUHandle(0), sz, offscreenClearColor));
    offscreen.ds.Attach(createExtraDepthStencilAndView(extraDepthStencilCPUHandle(0), sz));

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
    if (FAILED(dev->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                        IID_PPV_ARGS(&offscreen.rootSignature)))) {
        qWarning("Failed to create root signature");
        return;
    }

    const D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    D3D12_SHADER_BYTECODE vshader;
    vshader.pShaderBytecode = g_VS_Offscreen;
    vshader.BytecodeLength = sizeof(g_VS_Offscreen);
    D3D12_SHADER_BYTECODE pshader;
    pshader.pShaderBytecode = g_PS_Offscreen;
    pshader.BytecodeLength = sizeof(g_PS_Offscreen);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = pso();
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature = offscreen.rootSignature.Get();
    psoDesc.VS = vshader;
    psoDesc.PS = pshader;

    if (FAILED(dev->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&offscreen.pipelineState)))) {
        qWarning("Failed to create graphics pipeline state");
        return;
    }

    const float vertices[] = {
        0.0f, 0.707f, 0.0f, /* color */ 1.0f, 0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, 0.0f,             0.0f, 1.0f, 0.0f, 1.0f,
        0.5f, -0.5f, 0.0f,              0.0f, 0.0f, 1.0f, 1.0f
    };
    const UINT vertexBufferSize = sizeof(vertices);

    D3D12_HEAP_PROPERTIES heapProp = {};
    heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bufDesc = {};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width = vertexBufferSize;
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    if (FAILED(dev->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &bufDesc,
                                            D3D12_RESOURCE_STATE_GENERIC_READ, Q_NULLPTR,
                                            IID_PPV_ARGS(&offscreen.vertexBuffer)))) {
        qWarning("Failed to create committed resource (vertex buffer for triangle)");
        return;
    }

    quint8 *p = Q_NULLPTR;
    D3D12_RANGE readRange = { 0, 0 };
    if (FAILED(offscreen.vertexBuffer->Map(0, &readRange, reinterpret_cast<void **>(&p)))) {
        qWarning("Map failed (vertex buffer for triangle)");
        return;
    }
    memcpy(p, vertices, vertexBufferSize);
    offscreen.vertexBuffer->Unmap(0, Q_NULLPTR);

    offscreen.vertexBufferView.BufferLocation = offscreen.vertexBuffer->GetGPUVirtualAddress();
    offscreen.vertexBufferView.StrideInBytes = (3 + 4) * sizeof(float);
    offscreen.vertexBufferView.SizeInBytes = vertexBufferSize;

    const UINT CB_SIZE = alignedCBSize(128); // 2 * float4x4
    bufDesc.Width = CB_SIZE;
    if (FAILED(dev->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &bufDesc,
                                            D3D12_RESOURCE_STATE_GENERIC_READ, Q_NULLPTR,
                                            IID_PPV_ARGS(&offscreen.constantBuffer)))) {
        qWarning("Failed to create committed resource (constant buffer for triangle)");
        return;
    }

    if (FAILED(offscreen.constantBuffer->Map(0, &readRange, reinterpret_cast<void **>(&p)))) {
        qWarning("Map failed (constant buffer for triangle)");
        return;
    }
    offscreen.cbPtr = p;

    offscreen.projection.perspective(60.0f, OFFSCREEN_WIDTH / float(OFFSCREEN_HEIGHT), 0.1f, 100.0f);

    // Create a bundle for drawing a triangle.
    if (FAILED(dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE, bundleAllocator(), Q_NULLPTR, IID_PPV_ARGS(&offscreen.bundle)))) {
        qWarning("Failed to create offscreen command bundle");
        return;
    }
    offscreen.bundle->SetPipelineState(offscreen.pipelineState.Get());
    offscreen.bundle->SetGraphicsRootSignature(offscreen.rootSignature.Get());
    offscreen.bundle->SetGraphicsRootConstantBufferView(0, offscreen.constantBuffer->GetGPUVirtualAddress());
    offscreen.bundle->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    offscreen.bundle->IASetVertexBuffers(0, 1, &offscreen.vertexBufferView);
    offscreen.bundle->DrawInstanced(3, 1, 0, 0);
    offscreen.bundle->Close();
}

void Window::initializeOnscreen()
{
    // Set up a cube that is textured with the render target from the offscreen step.

    ID3D12Device *dev = device();

    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0; // s0
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_PARAMETER rootParameters[2];
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[0].Descriptor.ShaderRegister = 0; // b0
    rootParameters[0].Descriptor.RegisterSpace = 0;

    D3D12_DESCRIPTOR_RANGE descRange;
    descRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descRange.NumDescriptors = 1;
    descRange.BaseShaderRegister = 0; // t0
    descRange.RegisterSpace = 0;
    descRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[1].DescriptorTable.pDescriptorRanges = &descRange;

    D3D12_ROOT_SIGNATURE_DESC desc;
    desc.NumParameters = 2;
    desc.pParameters = rootParameters;
    desc.NumStaticSamplers = 1;
    desc.pStaticSamplers = &sampler;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error))) {
        qWarning("Failed to serialize root signature");
        return;
    }
    if (FAILED(dev->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                        IID_PPV_ARGS(&onscreen.rootSignature)))) {
        qWarning("Failed to create root signature");
        return;
    }

    // We have a non-interleaved layout.
    const D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    D3D12_SHADER_BYTECODE vshader;
    vshader.pShaderBytecode = g_VS_Onscreen;
    vshader.BytecodeLength = sizeof(g_VS_Onscreen);
    D3D12_SHADER_BYTECODE pshader;
    pshader.pShaderBytecode = g_PS_Onscreen;
    pshader.BytecodeLength = sizeof(g_PS_Onscreen);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = pso();
    psoDesc.RasterizerState.FrontCounterClockwise = FALSE; // winding order for the cube data below is CW
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature = onscreen.rootSignature.Get();
    psoDesc.VS = vshader;
    psoDesc.PS = pshader;

    if (FAILED(dev->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&onscreen.pipelineState)))) {
        qWarning("Failed to create graphics pipeline state");
        return;
    }

    // borrowed from qtdeclarative/examples/quick/rendercontrol
    const float v[] = {
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
    const float texCoords[] = {
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

    const UINT vertexBufferSize = sizeof(v) + sizeof(texCoords);

    D3D12_HEAP_PROPERTIES heapProp = {};
    heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bufDesc = {};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width = vertexBufferSize;
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    if (FAILED(dev->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &bufDesc,
                                            D3D12_RESOURCE_STATE_GENERIC_READ, Q_NULLPTR,
                                            IID_PPV_ARGS(&onscreen.vertexBuffer)))) {
        qWarning("Failed to create committed resource (vertex buffer for cube)");
        return;
    }

    quint8 *p = Q_NULLPTR;
    D3D12_RANGE readRange = { 0, 0 };
    if (FAILED(onscreen.vertexBuffer->Map(0, &readRange, reinterpret_cast<void **>(&p)))) {
        qWarning("Map failed (vertex buffer for cube)");
        return;
    }
    memcpy(p, v, sizeof(v));
    memcpy(p + sizeof(v), texCoords, sizeof(texCoords));
    onscreen.vertexBuffer->Unmap(0, Q_NULLPTR);

    onscreen.vertexBufferView[0].BufferLocation = onscreen.vertexBuffer->GetGPUVirtualAddress();
    onscreen.vertexBufferView[0].StrideInBytes = 3 * sizeof(float);
    onscreen.vertexBufferView[0].SizeInBytes = sizeof(v);
    onscreen.vertexBufferView[1].BufferLocation = onscreen.vertexBuffer->GetGPUVirtualAddress() + sizeof(v);
    onscreen.vertexBufferView[1].StrideInBytes = 2 * sizeof(float);
    onscreen.vertexBufferView[1].SizeInBytes = sizeof(texCoords);

    const UINT CB_SIZE = alignedCBSize(128); // 2 * float4x4
    bufDesc.Width = CB_SIZE;
    if (FAILED(dev->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &bufDesc,
                                            D3D12_RESOURCE_STATE_GENERIC_READ, Q_NULLPTR,
                                            IID_PPV_ARGS(&onscreen.constantBuffer)))) {
        qWarning("Failed to create committed resource (constant buffer for cube)");
        return;
    }

    if (FAILED(onscreen.constantBuffer->Map(0, &readRange, reinterpret_cast<void **>(&p)))) {
        qWarning("Map failed (constant buffer for cube)");
        return;
    }
    onscreen.cbPtr = p;

    dev->CreateShaderResourceView(offscreen.rt.Get(), Q_NULLPTR, cbvSrvHeap->GetCPUDescriptorHandleForHeapStart());

    // Create a bundle for drawing a cube.
    if (FAILED(dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE, bundleAllocator(), Q_NULLPTR, IID_PPV_ARGS(&onscreen.bundle)))) {
        qWarning("Failed to create onscreen command bundle");
        return;
    }
    onscreen.bundle->SetPipelineState(onscreen.pipelineState.Get());
    onscreen.bundle->SetGraphicsRootSignature(onscreen.rootSignature.Get());
    onscreen.bundle->SetGraphicsRootConstantBufferView(0, onscreen.constantBuffer->GetGPUVirtualAddress());
    // This is only here to be able to add the SetGraphicsRootDescriptorTable call below.
    // Must match the heap set on the direct command list.
    ID3D12DescriptorHeap *heaps[] = { cbvSrvHeap.Get() };
    onscreen.bundle->SetDescriptorHeaps(_countof(heaps), heaps);
    // cbvSrvHeap has a single SRV descriptor only so the start address is just what we need
    onscreen.bundle->SetGraphicsRootDescriptorTable(1, cbvSrvHeap->GetGPUDescriptorHandleForHeapStart());
    onscreen.bundle->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    onscreen.bundle->IASetVertexBuffers(0, 2, onscreen.vertexBufferView);
    onscreen.bundle->DrawInstanced(36, 1, 0, 0);
    onscreen.bundle->Close();

    onscreen.projection.perspective(60.0f, width() / float(height()), 0.1f, 100.0f);
}

void Window::resizeD3D(const QSize &)
{
    onscreen.projection.setToIdentity();
    onscreen.projection.perspective(60.0f, width() / float(height()), 0.1f, 100.0f);
}

void Window::paintD3D()
{
    commandAllocator()->Reset();
    commandList->Reset(commandAllocator(), Q_NULLPTR);

    paintOffscreen();
    paintOnscreen();

    commandList->Close();
    ID3D12CommandList *commandLists[] = { commandList.Get() };
    commandQueue()->ExecuteCommandLists(_countof(commandLists), commandLists);

    update();
}

void Window::paintOffscreen()
{
    QMatrix4x4 modelview;
    modelview.translate(0, 0, -2);
    modelview.rotate(offscreen.rotationAngle, 0, 0, 1);
    offscreen.rotationAngle += 1;
    memcpy(offscreen.cbPtr, modelview.constData(), 16 * sizeof(float));
    memcpy(offscreen.cbPtr + 16 * sizeof(float), offscreen.projection.constData(), 16 * sizeof(float));

    D3D12_VIEWPORT viewport = { 0, 0, OFFSCREEN_WIDTH, OFFSCREEN_HEIGHT, 0, 1 };
    commandList->RSSetViewports(1, &viewport);
    D3D12_RECT scissorRect = { 0, 0, OFFSCREEN_WIDTH - 1, OFFSCREEN_HEIGHT - 1 };
    commandList->RSSetScissorRects(1, &scissorRect);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = extraRenderTargetCPUHandle(0);
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = extraDepthStencilCPUHandle(0);
    commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    commandList->ClearRenderTargetView(rtvHandle, offscreenClearColor, 0, Q_NULLPTR);
    commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, Q_NULLPTR);

    commandList->ExecuteBundle(offscreen.bundle.Get());
}

void Window::paintOnscreen()
{
    QMatrix4x4 modelview;
    modelview.translate(0, 0, -2);
    modelview.rotate(onscreen.rotationAngle, 1, 0.5, 0);
    onscreen.rotationAngle += 1;
    memcpy(onscreen.cbPtr, modelview.constData(), 16 * sizeof(float));
    memcpy(onscreen.cbPtr + 16 * sizeof(float), onscreen.projection.constData(), 16 * sizeof(float));

    D3D12_VIEWPORT viewport = { 0, 0, float(width()), float(height()), 0, 1 };
    commandList->RSSetViewports(1, &viewport);
    D3D12_RECT scissorRect = { 0, 0, width() - 1, height() - 1 };
    commandList->RSSetScissorRects(1, &scissorRect);

    transitionResource(offscreen.rt.Get(), commandList.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    transitionResource(backBufferRenderTarget(), commandList.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = backBufferRenderTargetCPUHandle();
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = depthStencilCPUHandle();
    commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    commandList->ClearRenderTargetView(rtvHandle, onscreenClearColor, 0, Q_NULLPTR);
    commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, Q_NULLPTR);

    ID3D12DescriptorHeap *heaps[] = { cbvSrvHeap.Get() };
    commandList->SetDescriptorHeaps(_countof(heaps), heaps);

    commandList->ExecuteBundle(onscreen.bundle.Get());

    transitionResource(backBufferRenderTarget(), commandList.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    transitionResource(offscreen.rt.Get(), commandList.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
}

void Window::afterPresent()
{
    waitForGPU(f);
}

void Window::readbackAndSave()
{
    commandList->Reset(commandAllocator(), Q_NULLPTR);
    QImage img = readbackRGBA8888(offscreen.rt.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, commandList.Get());
    QString fn = QFileDialog::getSaveFileName(Q_NULLPTR, QStringLiteral("Save PNG"), QString(), QStringLiteral("PNG files (*.png)"));
    if (!fn.isEmpty()) {
        img.save(fn);
        QMessageBox::information(Q_NULLPTR, QStringLiteral("Saved"), QStringLiteral("Offscreen render target read back and saved to ") + fn);
    }
}

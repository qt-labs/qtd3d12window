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
#include "shader_cs.h"

// size must be a power of two due to the way mipmap generation is done
static const int TEXTURE_WIDTH = 512;
static const int TEXTURE_HEIGHT = 512;

static const int TEXTURE_MIP_LEVELS = 10; // ceil(log2(max(width, height))) + 1

Window::Window()
    : f(Q_NULLPTR),
      cbPtr(Q_NULLPTR),
      rotationAngle(0)
{
}

Window::~Window()
{
    if (cbPtr)
        constantBuffer->Unmap(0, Q_NULLPTR);

    delete f;
}

void Window::initializeD3D()
{
    QImage qtLogo = QImage(QStringLiteral(":/qt.png")).convertToFormat(QImage::Format_RGBA8888).scaled(TEXTURE_WIDTH, TEXTURE_HEIGHT);
    if (qtLogo.isNull()) {
        qWarning("Failed to load image data");
        return;
    }

    f = createFence();
    ID3D12Device *dev = device();

    // One static sampler (no sampler heap is needed).
    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // In addition to the constant buffer view we will now also have a shader
    // resource view in order to expose the texture to the pixel shader.
    D3D12_DESCRIPTOR_RANGE descRange[2];
    descRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    descRange[0].NumDescriptors = 1;
    descRange[0].BaseShaderRegister = 0; // b0
    descRange[0].RegisterSpace = 0;
    descRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    descRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descRange[1].NumDescriptors = 1;
    descRange[1].BaseShaderRegister = 0; // t0
    descRange[1].RegisterSpace = 0;
    descRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameter;
    rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameter.DescriptorTable.NumDescriptorRanges = 2;
    rootParameter.DescriptorTable.pDescriptorRanges = descRange;

    D3D12_ROOT_SIGNATURE_DESC desc;
    desc.NumParameters = 1;
    desc.pParameters = &rootParameter;
    desc.NumStaticSamplers = 1;
    desc.pStaticSamplers = &sampler;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error))) {
        QByteArray msg(static_cast<const char *>(error->GetBufferPointer()), error->GetBufferSize());
        qWarning("Failed to serialize root signature: %s", qPrintable(msg));
        return;
    }
    if (FAILED(dev->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature)))) {
        qWarning("Failed to create root signature");
        return;
    }

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    D3D12_SHADER_BYTECODE vshader;
    vshader.pShaderBytecode = g_VS_Texture;
    vshader.BytecodeLength = sizeof(g_VS_Texture);
    D3D12_SHADER_BYTECODE pshader;
    pshader.pShaderBytecode = g_PS_Texture;
    pshader.BytecodeLength = sizeof(g_PS_Texture);

    D3D12_RASTERIZER_DESC rastDesc = {};
    rastDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rastDesc.CullMode = D3D12_CULL_MODE_BACK;
    rastDesc.FrontCounterClockwise = TRUE; // Vertices are given CCW
    rastDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rastDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rastDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rastDesc.DepthClipEnable = TRUE;

    // Our material is transparent (the image has alpha < 255). Enable blending.
    const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc = {
        TRUE, FALSE,
        D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD,
        D3D12_BLEND_ZERO, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_LOGIC_OP_NOOP,
        D3D12_COLOR_WRITE_ENABLE_ALL
    };
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
    psoDesc.SampleDesc.Count = 1;
    if (FAILED(dev->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState)))) {
        qWarning("Failed to create graphics pipeline state");
        return;
    }

    if (FAILED(dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator(), Q_NULLPTR, IID_PPV_ARGS(&commandList)))) {
        qWarning("Failed to create command list");
        return;
    }

    // Vertex buffer
    const float vertices[] = {
        -0.5f, -0.5f, 0, /* coords */ 0, 1, // the image was not flipped so accommodate for it here
        0.5f, -0.5f, 0,               1, 1,
        -0.5f, 0.5f, 0,               0, 0,

        -0.5f, 0.5f, 0,               0, 0,
        0.5f, -0.5f, 0,               1, 1,
        0.5f, 0.5f, 0,                1, 0
    };
    const UINT vertexBufferSize = sizeof(vertices);

    D3D12_HEAP_PROPERTIES defaultHeapProp = {};
    defaultHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_HEAP_PROPERTIES uploadHeapProp = defaultHeapProp;
    uploadHeapProp.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bufDesc = {};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width = vertexBufferSize;
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    if (FAILED(dev->CreateCommittedResource(&uploadHeapProp, D3D12_HEAP_FLAG_NONE, &bufDesc,
                                            D3D12_RESOURCE_STATE_GENERIC_READ, Q_NULLPTR, IID_PPV_ARGS(&vertexBuffer)))) {
        qWarning("Failed to create committed resource (vertex buffer)");
        return;
    }

    quint8 *p = Q_NULLPTR;
    D3D12_RANGE readRange = { 0, 0 };
    if (FAILED(vertexBuffer->Map(0, &readRange, reinterpret_cast<void **>(&p)))) {
        qWarning("Map failed (vertex buffer)");
        return;
    }
    memcpy(p, vertices, vertexBufferSize);
    vertexBuffer->Unmap(0, Q_NULLPTR);

    vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
    vertexBufferView.StrideInBytes = (3 + 2) * sizeof(float);
    vertexBufferView.SizeInBytes = vertexBufferSize;

    // Constant buffer
    const UINT CB_SIZE = alignedCBSize(128); // 2 * float4x4
    bufDesc.Width = CB_SIZE;
    if (FAILED(dev->CreateCommittedResource(&uploadHeapProp, D3D12_HEAP_FLAG_NONE, &bufDesc,
                                            D3D12_RESOURCE_STATE_GENERIC_READ, Q_NULLPTR, IID_PPV_ARGS(&constantBuffer)))) {
        qWarning("Failed to create committed resource (constant buffer)");
        return;
    }

    if (FAILED(constantBuffer->Map(0, &readRange, reinterpret_cast<void **>(&p)))) {
        qWarning("Map failed (constant buffer)");
        return;
    }
    cbPtr = p; // won't Unmap() this here

    // Constant buffer view and shader resource view descriptors are stored in the same heap.
    cbvSrvUavStride = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_DESCRIPTOR_HEAP_DESC cbvSrvUavHeapDesc = {};
    // CBV + SRV + (TEXTURE_MIP_LEVELS - 1) * UAV
    cbvSrvUavHeapDesc.NumDescriptors = 1 + 1 + (TEXTURE_MIP_LEVELS - 1);
    cbvSrvUavHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cbvSrvUavHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    if (FAILED(dev->CreateDescriptorHeap(&cbvSrvUavHeapDesc, IID_PPV_ARGS(&cbvSrvUavHeap)))) {
        qWarning("Failed to create CBV/SRV/UAV descriptor heap");
        return;
    }

    // CBV for the pixel shader
    D3D12_CPU_DESCRIPTOR_HANDLE cbvSrvUavHandle = cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = constantBuffer->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = CB_SIZE;
    dev->CreateConstantBufferView(&cbvDesc, cbvSrvUavHandle);
    cbvSrvUavHandle.ptr += cbvSrvUavStride;

    // Texture (with mipmaps and allowing read/write via UAVs)
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = TEXTURE_WIDTH;
    textureDesc.Height = TEXTURE_HEIGHT;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = TEXTURE_MIP_LEVELS;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    if (FAILED(dev->CreateCommittedResource(&defaultHeapProp, D3D12_HEAP_FLAG_NONE, &textureDesc,
                                            D3D12_RESOURCE_STATE_COPY_DEST, Q_NULLPTR, IID_PPV_ARGS(&texture)))) {
        qWarning("Failed to create texture resource");
        return;
    }

    // Shader resource view for exposing the texture to the compute and pixel shaders
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = TEXTURE_MIP_LEVELS;
    dev->CreateShaderResourceView(texture.Get(), &srvDesc, cbvSrvUavHandle);
    cbvSrvUavHandle.ptr += cbvSrvUavStride;

    // Mipmap generation will need an UAV for each level that needs to be generated.
    for (int level = 1; level < TEXTURE_MIP_LEVELS; ++level) {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = textureDesc.Format;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Texture2D.MipSlice = level;
        dev->CreateUnorderedAccessView(texture.Get(), Q_NULLPTR, &uavDesc, cbvSrvUavHandle);
        cbvSrvUavHandle.ptr += cbvSrvUavStride;
    }

    ComPtr<ID3D12Resource> textureUploadBuffer;
    UINT64 textureUploadBufferSize;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT textureLayout;
    dev->GetCopyableFootprints(&textureDesc, 0, 1, 0, &textureLayout, Q_NULLPTR, Q_NULLPTR, &textureUploadBufferSize);
    bufDesc.Width = textureUploadBufferSize;
    if (FAILED(dev->CreateCommittedResource(&uploadHeapProp, D3D12_HEAP_FLAG_NONE, &bufDesc,
                                            D3D12_RESOURCE_STATE_GENERIC_READ, Q_NULLPTR, IID_PPV_ARGS(&textureUploadBuffer)))) {
        qWarning("Failed to create texture upload buffer resource");
        return;
    }

    // Unlike hellotexture, upload only level 0. Leave the rest to a compute shader.
    if (FAILED(textureUploadBuffer->Map(0, &readRange, reinterpret_cast<void **>(&p)))) {
        qWarning("Map failed (texture upload buffer)");
        return;
    }
    p += textureLayout.Offset;
    for (int y = 0; y < TEXTURE_HEIGHT; ++y) {
        memcpy(p, qtLogo.scanLine(y), TEXTURE_WIDTH * 4);
        p += textureLayout.Footprint.RowPitch;
    }
    textureUploadBuffer->Unmap(0, Q_NULLPTR);

    D3D12_TEXTURE_COPY_LOCATION dstLoc;
    dstLoc.pResource = texture.Get();
    dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLoc.SubresourceIndex = 0;
    D3D12_TEXTURE_COPY_LOCATION srcLoc;
    srcLoc.pResource = textureUploadBuffer.Get();
    srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLoc.PlacedFootprint = textureLayout;
    commandList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, Q_NULLPTR);

    // once the copy is done, the texture is ready to be used in the compute shader...
    transitionResource(texture.Get(), commandList.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    uavBarrier(texture.Get(), commandList.Get());

    initMipMaps();
    generateMipMaps();

    // ...and then in the pixel shader
    uavBarrier(texture.Get(), commandList.Get());
    transitionResource(texture.Get(), commandList.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // Execute the texture upload and mipmap generation.
    commandList->Close();
    ID3D12CommandList *commandLists[] = { commandList.Get() };
    commandQueue()->ExecuteCommandLists(_countof(commandLists), commandLists);

    // Block until all the above has finished.
    waitForGPU(f);

    setupProjection();
}

void Window::initMipMaps()
{
    ID3D12Device *dev = device();

    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;

    D3D12_DESCRIPTOR_RANGE descRange[2];
    descRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descRange[0].NumDescriptors = 1;
    descRange[0].BaseShaderRegister = 0; // t0
    descRange[0].RegisterSpace = 0;
    descRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    descRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    descRange[1].NumDescriptors = 4;
    descRange[1].BaseShaderRegister = 0; // u0..u3
    descRange[1].RegisterSpace = 0;
    descRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // Split into two to allow switching between the first and second set of UAVs later.
    D3D12_ROOT_PARAMETER rootParameters[3];
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &descRange[0];

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[1].DescriptorTable.pDescriptorRanges = &descRange[1];

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[2].Constants.Num32BitValues = 4; // uint2 mip1Size, uint sampleLevel, uint totalMips
    rootParameters[2].Constants.ShaderRegister = 0; // b0
    rootParameters[2].Constants.RegisterSpace = 0;

    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.NumParameters = 3;
    desc.pParameters = rootParameters;
    desc.NumStaticSamplers = 1;
    desc.pStaticSamplers = &sampler;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error))) {
        QByteArray msg(static_cast<const char *>(error->GetBufferPointer()), error->GetBufferSize());
        qWarning("Failed to serialize compute root signature: %s", qPrintable(msg));
        return;
    }
    if (FAILED(dev->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
                                        IID_PPV_ARGS(&computeRootSignature)))) {
        qWarning("Failed to create compute root signature");
        return;
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = computeRootSignature.Get();
    psoDesc.CS.pShaderBytecode = g_CS_Generate4MipMaps;
    psoDesc.CS.BytecodeLength = sizeof(g_CS_Generate4MipMaps);

    if (FAILED(dev->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&computeState)))) {
        qWarning("Failed to create compute pipeline state");
        return;
    }
}

void Window::generateMipMaps()
{
    commandList->SetPipelineState(computeState.Get());
    commandList->SetComputeRootSignature(computeRootSignature.Get());

    ID3D12DescriptorHeap *heaps[] = { cbvSrvUavHeap.Get() };
    commandList->SetDescriptorHeaps(_countof(heaps), heaps);

    D3D12_GPU_DESCRIPTOR_HANDLE h = cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart(); // CBV, not needed here
    h.ptr += cbvSrvUavStride; // SRV
    commandList->SetComputeRootDescriptorTable(0, h);

    h.ptr += cbvSrvUavStride; // now points to the first of (TEXTURE_MIP_LEVELS - 1) UAV descriptors

    for (quint32 level = 1; level < TEXTURE_MIP_LEVELS; level += 4, h.ptr += cbvSrvUavStride * 4) {
        commandList->SetComputeRootDescriptorTable(1, h);

        QSize sz(TEXTURE_WIDTH, TEXTURE_HEIGHT);
        sz.setWidth(qMax(1, sz.width() >> level));
        sz.setHeight(qMax(1, sz.height() >> level));

        const quint32 constants[4] = { quint32(sz.width()), quint32(sz.height()),
                                       level - 1,
                                       TEXTURE_MIP_LEVELS - 1 };
        commandList->SetComputeRoot32BitConstants(2, 4, constants, 0);

        commandList->Dispatch(sz.width(), sz.height(), 1);
    }
}

void Window::resizeD3D(const QSize &)
{
    setupProjection();
}

void Window::setupProjection()
{
    projection.setToIdentity();
    projection.perspective(60.0f, width() / float(height()), 0.1f, 100.0f);
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

    ID3D12DescriptorHeap *heaps[] = { cbvSrvUavHeap.Get() };
    commandList->SetDescriptorHeaps(_countof(heaps), heaps);
    commandList->SetGraphicsRootDescriptorTable(0, cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart());

    D3D12_VIEWPORT viewport = { 0, 0, float(width()), float(height()), 0, 1 };
    commandList->RSSetViewports(1, &viewport);
    D3D12_RECT scissorRect = { 0, 0, width() - 1, height() - 1 };
    commandList->RSSetScissorRects(1, &scissorRect);

    transitionResource(backBufferRenderTarget(), commandList.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = backBufferRenderTargetCPUHandle();
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = depthStencilCPUHandle();
    commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, Q_NULLPTR);
    commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, Q_NULLPTR);

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
    commandList->DrawInstanced(6, 1, 0, 0);

    transitionResource(backBufferRenderTarget(), commandList.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

    commandList->Close();
    ID3D12CommandList *commandLists[] = { commandList.Get() };
    commandQueue()->ExecuteCommandLists(_countof(commandLists), commandLists);

    update();
}

void Window::afterPresent()
{
    waitForGPU(f);
}

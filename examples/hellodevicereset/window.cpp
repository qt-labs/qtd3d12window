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
#include "tdr.h"

void Window::initializeD3D()
{
    f.reset(createFence());
    ID3D12Device *dev = device();

    if (FAILED(dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator(), Q_NULLPTR,
                                      IID_PPV_ARGS(&commandList)))) {
        qWarning("Failed to create command list");
        return;
    }
    commandList->Close();

    D3D12_DESCRIPTOR_RANGE descRange[2];
    descRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    descRange[0].NumDescriptors = 1;
    descRange[0].BaseShaderRegister = 0;
    descRange[0].RegisterSpace = 0;
    descRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    descRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    descRange[1].NumDescriptors = 1;
    descRange[1].BaseShaderRegister = 0;
    descRange[1].RegisterSpace = 0;
    descRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER param;
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    param.DescriptorTable.NumDescriptorRanges = 2;
    param.DescriptorTable.pDescriptorRanges = descRange;

    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.NumParameters = 1;
    desc.pParameters = &param;

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
    psoDesc.CS.pShaderBytecode = g_timeout;
    psoDesc.CS.BytecodeLength = sizeof(g_timeout);

    if (FAILED(dev->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&computeState)))) {
        qWarning("Failed to create compute pipeline state");
        return;
    }
}

void Window::releaseD3D()
{
    // Release all resources. initializeD3D() will get invoked later on.
    commandList = Q_NULLPTR;
    computeState = Q_NULLPTR;
    computeRootSignature = Q_NULLPTR;
    f.reset();
}

void Window::resizeD3D(const QSize &)
{
}

void Window::paintD3D()
{
    commandAllocator()->Reset();
    commandList->Reset(commandAllocator(), Q_NULLPTR);

    transitionResource(backBufferRenderTarget(), commandList.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

    green += 0.01f;
    if (green > 1.0f)
        green = 0.0f;
    const float clearColor[] = { 0.0f, green, 0.0f, 1.0f };
    commandList->ClearRenderTargetView(backBufferRenderTargetCPUHandle(), clearColor, 0, Q_NULLPTR);

    transitionResource(backBufferRenderTarget(), commandList.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

    commandList->Close();
    ID3D12CommandList *commandLists[] = { commandList.Get() };
    commandQueue()->ExecuteCommandLists(_countof(commandLists), commandLists);

    update();
}

void Window::afterPresent()
{
    waitForGPU(f.data());
}

void Window::timeout()
{
    commandAllocator()->Reset();
    commandList->Reset(commandAllocator(), computeState.Get());

    commandList->SetComputeRootSignature(computeRootSignature.Get());
    commandList->Dispatch(256, 1, 1);

    commandList->Close();
    ID3D12CommandList *commandLists[] = { commandList.Get() };
    commandQueue()->ExecuteCommandLists(_countof(commandLists), commandLists);

    waitForGPU(f.data());
}

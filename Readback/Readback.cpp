#include <Windows.h>
#include <LM.h>
#include <tchar.h>
#include <wrl/client.h>
#include <iostream>
#include <string>
#include <clocale>
#include <stdexcept>
#include <d3d12.h>
#include <d3dcompiler.h>
#include "../_common/dxcommon.h"

#pragma comment(lib,"Netapi32.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace std;
using Microsoft::WRL::ComPtr;

namespace
{
	ComPtr<ID3D12Device> gDev;
}

void CHK(HRESULT hr)
{
	if (FAILED(hr))
		throw runtime_error("HRESULT is failed value.");
}

void setResourceBarrier(ID3D12GraphicsCommandList* commandList,
	ID3D12Resource* res,
	D3D12_RESOURCE_STATES before,
	D3D12_RESOURCE_STATES after)
{
	D3D12_RESOURCE_BARRIER desc = {};
	desc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	desc.Transition.pResource = res;
	desc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	desc.Transition.StateBefore = before;
	desc.Transition.StateAfter = after;
	desc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	commandList->ResourceBarrier(1, &desc);
}

void proc()
{
	ComPtr<ID3D12CommandAllocator> cmdAlloc;
	ComPtr<ID3D12CommandQueue> cmdQueue;

	ComPtr<ID3D12GraphicsCommandList> cmdList;
	ComPtr<ID3D12Fence> fence;
	HANDLE fenceEveneHandle;

	ComPtr<ID3D12DescriptorHeap> descHeapUav;

	ComPtr<ID3D12RootSignature> rootSignature;
	ComPtr<ID3D12PipelineState> pso;
	ComPtr<ID3D12Resource> bufferDefault;
	ComPtr<ID3D12Resource> bufferReadback;

#if _DEBUG
	ID3D12Debug* debug = nullptr;
	D3D12GetDebugInterface(IID_PPV_ARGS(&debug));
	if (debug)
	{
		debug->EnableDebugLayer();
		debug->Release();
		debug = nullptr;
	}
#endif /* _DEBUG */
	ID3D12Device* dev;
	CHK(D3D12CreateDevice(
		nullptr,
		D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(&dev)));
	gDev = dev;

	CHK(gDev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(cmdAlloc.ReleaseAndGetAddressOf())));

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	CHK(gDev->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(cmdQueue.ReleaseAndGetAddressOf())));

	CHK(gDev->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		cmdAlloc.Get(),
		nullptr,
		IID_PPV_ARGS(cmdList.ReleaseAndGetAddressOf())));

	CHK(gDev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.ReleaseAndGetAddressOf())));

	fenceEveneHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);

	// Create root signature
	{
		CD3DX12_DESCRIPTOR_RANGE descRange[1];
		descRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

		CD3DX12_ROOT_PARAMETER rootParam[1];
		rootParam[0].InitAsDescriptorTable(ARRAYSIZE(descRange), descRange);

		ID3D10Blob *sig, *info;
		auto rootSigDesc = D3D12_ROOT_SIGNATURE_DESC();
		rootSigDesc.NumParameters = 1;
		rootSigDesc.NumStaticSamplers = 0;
		rootSigDesc.pParameters = rootParam;
		rootSigDesc.pStaticSamplers = nullptr;
		rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
		CHK(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &info));
		gDev->CreateRootSignature(
			0,
			sig->GetBufferPointer(),
			sig->GetBufferSize(),
			IID_PPV_ARGS(rootSignature.ReleaseAndGetAddressOf()));
		sig->Release();
	}

	// Create shader
	ID3D10Blob *cs;
	{
		ID3D10Blob *info;
		UINT flag = 0;
#if _DEBUG
		flag |= D3DCOMPILE_DEBUG;
#endif /* _DEBUG */
		CHK(D3DCompileFromFile(L"Readback.hlsl", nullptr, nullptr, "CSMain", "cs_5_0", flag, 0, &cs, &info));
	}

	// Create PSO
	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.CS.BytecodeLength = cs->GetBufferSize();
	psoDesc.CS.pShaderBytecode = cs->GetBufferPointer();
	psoDesc.pRootSignature = rootSignature.Get();
	CHK(gDev->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(pso.ReleaseAndGetAddressOf())));
	cs->Release();

	// Create DescriptorHeap for UAV
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.NumDescriptors = 10;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	CHK(gDev->CreateDescriptorHeap(&desc, IID_PPV_ARGS(descHeapUav.ReleaseAndGetAddressOf())));

	// Create buffer on device memory
	auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(256, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	CHK(gDev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		nullptr,
		IID_PPV_ARGS(bufferDefault.ReleaseAndGetAddressOf())));
	bufferDefault->SetName(L"BufferDefault");

	// Create buffer on system memory
	resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(256);
	CHK(gDev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(bufferReadback.ReleaseAndGetAddressOf())));
	bufferReadback->SetName(L"BufferUpload");

	// Setup UAV
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.Buffer.NumElements = 64;
	uavDesc.Buffer.StructureByteStride = 4;
	gDev->CreateUnorderedAccessView(
		bufferDefault.Get(),
		nullptr,
		&uavDesc,
		descHeapUav->GetCPUDescriptorHandleForHeapStart());

	// Record commands
	cmdList->SetComputeRootSignature(rootSignature.Get());
	cmdList->SetPipelineState(pso.Get());
	cmdList->SetDescriptorHeaps(1, descHeapUav.GetAddressOf());
	cmdList->SetComputeRootDescriptorTable(0, descHeapUav->GetGPUDescriptorHandleForHeapStart());
	cmdList->Dispatch(1, 1, 1);
	setResourceBarrier(cmdList.Get(), bufferDefault.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
	cmdList->CopyResource(bufferReadback.Get(), bufferDefault.Get());

	// Execute
	CHK(cmdList->Close());
	ID3D12CommandList* cmds = cmdList.Get();
	cmdQueue->ExecuteCommandLists(1, &cmds);

	// Wait until GPU finished
	CHK(fence->SetEventOnCompletion(1, fenceEveneHandle));
	CHK(cmdQueue->Signal(fence.Get(), 1));
	auto wait = WaitForSingleObject(fenceEveneHandle, 10000);
	if (wait != WAIT_OBJECT_0)
		throw runtime_error("Failed WaitForSingleObject().");

	// Cleanup command
	CHK(cmdAlloc->Reset());
	CloseHandle(fenceEveneHandle);

	// Get system memory pointer
	void* data;
	CHK(bufferReadback->Map(0, nullptr, &data));

	// Convert from UTF-32 to UTF-16
	const char32_t *str = reinterpret_cast<char32_t*>(data); // Shader wrote U"Hello, world!"
	size_t len = char_traits<char32_t>::length(str);
	wstring c8str(len, 0xDCDC);
	for (size_t i = 0; i < len; i++)
	{
		c8str[i] = static_cast<wchar_t>(str[i]);
	}
	bufferReadback->Unmap(0, nullptr);

	// Output
	wcout << c8str << endl;
	wcout << endl << L"Regards," << endl << L"GPU" << endl;
}

int wmain(int argc, wchar_t** argv)
{
	setlocale(LC_CTYPE, "");
	locale ctype(locale::classic(), locale(""), locale::ctype);
	wcout.imbue(ctype);

	// Get user name
	wchar_t userName[128];
	DWORD userNameSize = ARRAYSIZE(userName);
	GetUserName(userName, &userNameSize);
	// Get Microsoft account name
	USER_INFO_10 *ui = nullptr;
	NetUserGetInfo(nullptr, userName, 10, reinterpret_cast<LPBYTE*>(&ui));
	if (ui)
	{
		wcscpy_s(userName, ui->usri10_full_name);
		NetApiBufferFree(ui);
	}
	wcout << L"Dear "<< userName << L":" << endl << endl;

	proc();
	_getwch();

	return 0;
}

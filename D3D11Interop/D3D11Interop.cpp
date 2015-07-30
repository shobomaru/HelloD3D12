#include <Windows.h>
#include <tchar.h>
#include <wrl/client.h>
#include <stdexcept>
#include <dxgi1_3.h>
#include <d3d12.h>
#include <d3d11on12.h>
#include <d3dcompiler.h>
#include "../_common/dxcommon.h"

#include <DirectXMath.h>
using DirectX::XMFLOAT3; // for WaveFrontReader
#include "../Mesh/WaveFrontReader.h"

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace std;
using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace
{
	const int WINDOW_WIDTH = 400;
	const int WINDOW_HEIGHT = 240;
	const int BUFFER_COUNT = 2;
	HWND g_mainWindowHandle = 0;
};

void CHK(HRESULT hr)
{
	if (FAILED(hr))
		throw runtime_error("HRESULT is failed value.");
}

class D3D
{
	ComPtr<IDXGIFactory2> mDxgiFactory;
	ComPtr<IDXGISwapChain1> mSwapChain;
	ComPtr<ID3D12Resource> mD3DBuffer[BUFFER_COUNT];
	int mBufferWidth, mBufferHeight;
	UINT64 mFrameCount = 0;
	static const UINT MaxFrameLatency = 2;

	ID3D12Device* mDev;
	ComPtr<ID3D12CommandAllocator> mCmdAlloc[MaxFrameLatency];
	ComPtr<ID3D12CommandQueue> mCmdQueue;

	ComPtr<ID3D12GraphicsCommandList> mCmdList;
	ComPtr<ID3D12Fence> mFence;
	HANDLE mFenceEveneHandle = 0;

	ComPtr<ID3D12DescriptorHeap> mDescHeapRtv;
	ComPtr<ID3D12DescriptorHeap> mDescHeapDsv;
	ComPtr<ID3D12DescriptorHeap> mDescHeapCbvSrvUav[MaxFrameLatency];
	void* mCBUploadPtr = nullptr;

	ComPtr<ID3D12RootSignature> mRootSignature;
	ComPtr<ID3D12PipelineState> mPso;
	ComPtr<ID3D12Resource> mVB;
	D3D12_VERTEX_BUFFER_VIEW mVBView = {};
	D3D12_INDEX_BUFFER_VIEW mIBView = {};
	UINT mIndexCount = 0;
	UINT mVBIndexOffset = 0;
	ComPtr<ID3D12Resource> mDB;
	ComPtr<ID3D12Resource> mCB;

	ComPtr<ID3D11On12Device> mDev11on12;
	ComPtr<ID3D11Device> mDev11;
	ComPtr<ID3D11DeviceContext> mDevCont11;
	ComPtr<ID3D11Texture2D> mBackBuffer11Tex[BUFFER_COUNT];
	ComPtr<ID3D11RenderTargetView> mBackBuffer11Rtv[BUFFER_COUNT];

public:
	D3D(int width, int height, HWND hWnd)
		: mBufferWidth(width), mBufferHeight(height), mDev(nullptr)
	{
		{
#if _DEBUG
			CHK(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(mDxgiFactory.ReleaseAndGetAddressOf())));
#else
			CHK(CreateDXGIFactory2(0, IID_PPV_ARGS(mDxgiFactory.ReleaseAndGetAddressOf())));
#endif /* _DEBUG */
		}

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
		mDev = dev;

		for (auto& a : mCmdAlloc)
		{
			CHK(mDev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(a.ReleaseAndGetAddressOf())));
		}

		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		CHK(mDev->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(mCmdQueue.ReleaseAndGetAddressOf())));

		DXGI_SWAP_CHAIN_DESC1 scDesc = {};
		scDesc.Width = width;
		scDesc.Height = height;
		scDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		scDesc.SampleDesc.Count = 1;
		scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		scDesc.BufferCount = BUFFER_COUNT;
		scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
		CHK(mDxgiFactory->CreateSwapChainForHwnd(mCmdQueue.Get(), hWnd, &scDesc, nullptr, nullptr, mSwapChain.ReleaseAndGetAddressOf()));

		CHK(mDev->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			mCmdAlloc[0].Get(),
			nullptr,
			IID_PPV_ARGS(mCmdList.ReleaseAndGetAddressOf())));
		mCmdList->Close();

		CHK(mDev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(mFence.ReleaseAndGetAddressOf())));

		mFenceEveneHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);

		for (int i = 0; i < BUFFER_COUNT; i++)
		{
			CHK(mSwapChain->GetBuffer(i, IID_PPV_ARGS(mD3DBuffer[i].ReleaseAndGetAddressOf())));
			mD3DBuffer[i]->SetName(L"SwapChain_Buffer");
		}

		{
			D3D12_DESCRIPTOR_HEAP_DESC desc = {};
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			desc.NumDescriptors = 10;
			desc.NodeMask = 0;
			CHK(mDev->CreateDescriptorHeap(&desc, IID_PPV_ARGS(mDescHeapRtv.ReleaseAndGetAddressOf())));

			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
			desc.NumDescriptors = 10;
			desc.NodeMask = 0;
			CHK(mDev->CreateDescriptorHeap(&desc, IID_PPV_ARGS(mDescHeapDsv.ReleaseAndGetAddressOf())));

			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			desc.NumDescriptors = 100;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			desc.NodeMask = 0;
			for (auto& c : mDescHeapCbvSrvUav)
			{
				CHK(mDev->CreateDescriptorHeap(&desc, IID_PPV_ARGS(c.ReleaseAndGetAddressOf())));
			}
		}

		auto rtvStep = mDev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		for (auto i = 0u; i < BUFFER_COUNT; i++)
		{
			auto d = mDescHeapRtv->GetCPUDescriptorHandleForHeapStart();
			d.ptr += i * rtvStep;
			mDev->CreateRenderTargetView(mD3DBuffer[i].Get(), nullptr, d);
		}

		{
			CD3DX12_DESCRIPTOR_RANGE descRange1[1];
			descRange1[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

			CD3DX12_ROOT_PARAMETER rootParam[1];
			rootParam[0].InitAsDescriptorTable(ARRAYSIZE(descRange1), descRange1);

			ID3D10Blob *sig, *info;
			auto rootSigDesc = D3D12_ROOT_SIGNATURE_DESC();
			rootSigDesc.NumParameters = 1;
			rootSigDesc.NumStaticSamplers = 0;
			rootSigDesc.pParameters = rootParam;
			rootSigDesc.pStaticSamplers = nullptr;
			rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
			CHK(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &info));
			mDev->CreateRootSignature(
				0,
				sig->GetBufferPointer(),
				sig->GetBufferSize(),
				IID_PPV_ARGS(mRootSignature.ReleaseAndGetAddressOf()));
			sig->Release();
		}

		ID3D10Blob *vs, *ps;
		{
			ID3D10Blob *info;
			UINT flag = 0;
#if _DEBUG
			flag |= D3DCOMPILE_DEBUG;
#endif /* _DEBUG */
			CHK(D3DCompileFromFile(L"../Mesh/Mesh.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", flag, 0, &vs, &info));
			CHK(D3DCompileFromFile(L"../Mesh/Mesh.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", flag, 0, &ps, &info));
		}
		D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.InputLayout.NumElements = 3;
		psoDesc.InputLayout.pInputElementDescs = inputLayout;
		psoDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
		psoDesc.pRootSignature = mRootSignature.Get();
		psoDesc.VS.pShaderBytecode = vs->GetBufferPointer();
		psoDesc.VS.BytecodeLength = vs->GetBufferSize();
		psoDesc.PS.pShaderBytecode = ps->GetBufferPointer();
		psoDesc.PS.BytecodeLength = ps->GetBufferSize();
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(CCCD3DX12_DEFAULT());
		psoDesc.BlendState = CD3DX12_BLEND_DESC(CCCD3DX12_DEFAULT());
		psoDesc.DepthStencilState.DepthEnable = true;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		psoDesc.DepthStencilState.StencilEnable = false;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		psoDesc.SampleDesc.Count = 1;
		CHK(mDev->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(mPso.ReleaseAndGetAddressOf())));
		vs->Release();
		ps->Release();

		WaveFrontReader<uint16_t> mesh;
		CHK(mesh.Load(L"../Mesh/teapot.obj"));

		mIndexCount = static_cast<UINT>(mesh.indices.size());
		mVBIndexOffset = static_cast<UINT>(sizeof(mesh.vertices[0]) * mesh.vertices.size());
		UINT IBSize = static_cast<UINT>(sizeof(mesh.indices[0]) * mIndexCount);

		void* vbData = mesh.vertices.data();
		void* ibData = mesh.indices.data();
		CHK(mDev->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(mVBIndexOffset + IBSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(mVB.ReleaseAndGetAddressOf())));
		mVB->SetName(L"VertexBuffer");
		char* vbUploadPtr = nullptr;
		CHK(mVB->Map(0, nullptr, reinterpret_cast<void**>(&vbUploadPtr)));
		memcpy_s(vbUploadPtr, mVBIndexOffset, vbData, mVBIndexOffset);
		memcpy_s(vbUploadPtr + mVBIndexOffset, IBSize, ibData, IBSize);
		mVB->Unmap(0, nullptr);

		mVBView.BufferLocation = mVB->GetGPUVirtualAddress();
		mVBView.StrideInBytes = sizeof(mesh.vertices[0]);
		mVBView.SizeInBytes = mVBIndexOffset;
		mIBView.BufferLocation = mVB->GetGPUVirtualAddress() + mVBIndexOffset;
		mIBView.Format = DXGI_FORMAT_R16_UINT;
		mIBView.SizeInBytes = IBSize;

		auto resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			DXGI_FORMAT_R32_TYPELESS, mBufferWidth, mBufferHeight, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
			D3D12_TEXTURE_LAYOUT_UNKNOWN, 0);
		D3D12_CLEAR_VALUE dsvClearValue;
		dsvClearValue.Format = DXGI_FORMAT_D32_FLOAT;
		dsvClearValue.DepthStencil.Depth = 1.0f;
		dsvClearValue.DepthStencil.Stencil = 0;
		CHK(mDev->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), // No need to read/write by CPU
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&dsvClearValue,
			IID_PPV_ARGS(mDB.ReleaseAndGetAddressOf())));
		mDB->SetName(L"DepthTexture");

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
		dsvDesc.Texture2D.MipSlice = 0;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		mDev->CreateDepthStencilView(mDB.Get(), &dsvDesc, mDescHeapDsv->GetCPUDescriptorHandleForHeapStart());

#define CB_SIZE 256
		UINT cbSize = CB_SIZE;
		assert((cbSize % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) == 0);
		CHK(mDev->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(cbSize * MaxFrameLatency),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(mCB.ReleaseAndGetAddressOf())));
		mCB->SetName(L"ConstantBuffer");
		for (auto i = 0u; i < MaxFrameLatency; ++i)
		{
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
			cbvDesc.BufferLocation = mCB->GetGPUVirtualAddress() + i * cbSize;
			cbvDesc.SizeInBytes = cbSize;
			mDev->CreateConstantBufferView(
				&cbvDesc,
				mDescHeapCbvSrvUav[i]->GetCPUDescriptorHandleForHeapStart());
		}
		CHK(mCB->Map(0, nullptr, reinterpret_cast<void**>(&mCBUploadPtr)));

		// Create D3D11
		UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
		auto featureLevel = D3D_FEATURE_LEVEL_11_0;
		auto cmdQueue = static_cast<IUnknown*>(mCmdQueue.Get());
#if _DEBUG
		flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif /* _DEBUG */
		CHK(D3D11On12CreateDevice(
			static_cast<IUnknown*>(mDev),
			flags,
			&featureLevel,
			1,
			&cmdQueue,
			1,
			0,
			mDev11.ReleaseAndGetAddressOf(),
			mDevCont11.ReleaseAndGetAddressOf(),
			nullptr));
		CHK(mDev11.As(&mDev11on12));

		D3D11_RESOURCE_FLAGS backBuffer11Flags = {};
		backBuffer11Flags.BindFlags = D3D11_BIND_RENDER_TARGET;
		for (int i = 0; i < BUFFER_COUNT; i++)
		{
			CHK(mDev11on12->CreateWrappedResource(
				mD3DBuffer[i].Get(),
				&backBuffer11Flags,
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				IID_PPV_ARGS(&mBackBuffer11Tex[i])));

			D3D11_TEXTURE2D_DESC texDesc;
			mBackBuffer11Tex[i]->GetDesc(&texDesc);
			D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
			rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			rtvDesc.Format = texDesc.Format;
			rtvDesc.Texture2D.MipSlice = 0;

			CHK(mDev11->CreateRenderTargetView(
				mBackBuffer11Tex[i].Get(),
				&rtvDesc,
				mBackBuffer11Rtv[i].ReleaseAndGetAddressOf()));
		}
	}
	~D3D()
	{
		mCB->Unmap(0, nullptr);
		CloseHandle(mFenceEveneHandle);
	}
	ID3D12Device* GetDevice() const
	{
		return mDev;
	}
	void Draw()
	{
		mFrameCount++;

		int cmdIndex = mFrameCount % MaxFrameLatency;
		auto* cmdQueue = mCmdQueue.Get();
		auto* cmdList = mCmdList.Get();

		// Wait untill next queue be freed
		if (mFrameCount > MaxFrameLatency)
		{
			mFence->SetEventOnCompletion(mFrameCount - MaxFrameLatency, mFenceEveneHandle);
			DWORD wait = WaitForSingleObject(mFenceEveneHandle, 10000);
			if (wait != WAIT_OBJECT_0)
				throw runtime_error("Failed WaitForSingleObject().");

			CHK(mCmdAlloc[cmdIndex]->Reset());
		}

		CHK(cmdList->Reset(mCmdAlloc[cmdIndex].Get(), nullptr));

		// Upload constant buffer
		{
			static float rot = 0.0f;
			rot += 1.0f;
			if (rot >= 360.0f) rot = 0.0f;

			XMMATRIX worldMat, viewMat, projMat;
			worldMat = XMMatrixRotationY(XMConvertToRadians(rot));
			viewMat = XMMatrixLookAtLH({ 0, 1, -1.5f }, { 0, 0.5f, 0 }, { 0, 1, 0 });
			projMat = XMMatrixPerspectiveFovLH(45, (float)mBufferWidth / mBufferHeight, 0.01f, 50.0f);
			auto mvpMat = XMMatrixTranspose(worldMat * viewMat * projMat);

			auto worldTransMat = XMMatrixTranspose(worldMat);

			// mCBUploadPtr is Write-Combine memory
			// Shift offset to guarantee that the pointer has not referred by executing command list.
			char* ptr = reinterpret_cast<char*>(mCBUploadPtr) + CB_SIZE * cmdIndex;
			memcpy_s(ptr, 64, &mvpMat, 64);
			memcpy_s(ptr + 64, 64, &worldTransMat, 64);
		}

		// Get current RTV descriptor
		auto descHandleRtvStep = mDev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		D3D12_CPU_DESCRIPTOR_HANDLE descHandleRtv = mDescHeapRtv->GetCPUDescriptorHandleForHeapStart();
		descHandleRtv.ptr += ((mFrameCount - 1) % BUFFER_COUNT) * descHandleRtvStep;
		// Get current swap chain
		ID3D12Resource* d3dBuffer = mD3DBuffer[(mFrameCount - 1) % BUFFER_COUNT].Get();
		// Get DSV
		auto descHandleDsv = mDescHeapDsv->GetCPUDescriptorHandleForHeapStart();

		// Barrier Present -> RenderTarget
		setResourceBarrier(mCmdList.Get(), d3dBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

		// Viewport & Scissor
		D3D12_VIEWPORT viewport = {};
		viewport.Width = (float)mBufferWidth;
		viewport.Height = (float)mBufferHeight;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		cmdList->RSSetViewports(1, &viewport);
		D3D12_RECT scissor = {};
		scissor.right = (LONG)mBufferWidth;
		scissor.bottom = (LONG)mBufferHeight;
		cmdList->RSSetScissorRects(1, &scissor);

		// Clear DepthTexture
		mCmdList->ClearDepthStencilView(descHandleDsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		// Clear
		{
			float clearColor[4] = { 0.1f, 0.2f, 0.3f, 1.0f };
			mCmdList->ClearRenderTargetView(descHandleRtv, clearColor, 0, nullptr);
		}

		mCmdList->OMSetRenderTargets(1, &descHandleRtv, true, &descHandleDsv);

		// Draw
		cmdList->SetGraphicsRootSignature(mRootSignature.Get());
		ID3D12DescriptorHeap* descHeaps[] = { mDescHeapCbvSrvUav[cmdIndex].Get() };
		cmdList->SetDescriptorHeaps(ARRAYSIZE(descHeaps), descHeaps);
		{
			cmdList->SetGraphicsRootDescriptorTable(0, mDescHeapCbvSrvUav[cmdIndex]->GetGPUDescriptorHandleForHeapStart());
			cmdList->SetPipelineState(mPso.Get());
			cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			cmdList->IASetVertexBuffers(0, 1, &mVBView);
			cmdList->IASetIndexBuffer(&mIBView);
			cmdList->DrawIndexedInstanced(mIndexCount, 1, 0, 0, 0);
		}

		// Barrier RenderTarget -> Present
		//setResourceBarrier(mCmdList.Get(), d3dBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

		// Exec
		CHK(cmdList->Close());
		ID3D12CommandList* const cmdLists = cmdList;
		//cmdQueue->ExecuteCommandLists(1, &cmdLists);
		CHK(cmdQueue->Signal(mFence.Get(), mFrameCount));

		// Acquire RTV for D3D11
		ID3D11Resource* backBuffer11 = mBackBuffer11Tex[(mFrameCount - 1) % BUFFER_COUNT].Get();
		mDev11on12->AcquireWrappedResources(&backBuffer11, 1);

		// Draw
		auto* rtv11 = mBackBuffer11Rtv[(mFrameCount - 1) % BUFFER_COUNT].Get();
		float color11[4] = {0.4f, 0.2f, 0.1f, 1.0f};
		mDevCont11->ClearRenderTargetView(rtv11, color11);

		// Release RTV for D3D11
		mDev11on12->ReleaseWrappedResources(&backBuffer11, 1);

		// Present
		CHK(mSwapChain->Present(1, 0));
	}

private:
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
};

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message) {
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE) {
			PostMessage(hWnd, WM_DESTROY, 0, 0);
			return 0;
		}
		break;

	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

static HWND setupWindow(int width, int height)
{
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = (HMODULE)GetModuleHandle(0);
	wcex.hIcon = nullptr;
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = _T("WindowClass");
	wcex.hIconSm = nullptr;
	if (!RegisterClassEx(&wcex)) {
		throw runtime_error("RegisterClassEx()");
	}

	RECT rect = { 0, 0, width, height };
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
	const int windowWidth = (rect.right - rect.left);
	const int windowHeight = (rect.bottom - rect.top);

	HWND hWnd = CreateWindow(_T("WindowClass"), _T("Window"),
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, windowWidth, windowHeight,
		nullptr, nullptr, nullptr, nullptr);
	if (!hWnd) {
		throw runtime_error("CreateWindow()");
	}

	return hWnd;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	MSG msg;
	ZeroMemory(&msg, sizeof msg);

	ID3D12Device* dev = nullptr;

#ifdef NDEBUG
	try
#endif
	{
		g_mainWindowHandle = setupWindow(WINDOW_WIDTH, WINDOW_HEIGHT);
		ShowWindow(g_mainWindowHandle, SW_SHOW);
		UpdateWindow(g_mainWindowHandle);

		D3D d3d(WINDOW_WIDTH, WINDOW_HEIGHT, g_mainWindowHandle);
		dev = d3d.GetDevice();

		while (msg.message != WM_QUIT) {
			BOOL r = PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE);
			if (r == 0) {
				d3d.Draw();
			}
			else {
				DispatchMessage(&msg);
			}
		}
	}
#ifdef NDEBUG
	catch (std::exception &e) {
		MessageBoxA(g_mainWindowHandle, e.what(), "Exception occuured.", MB_ICONSTOP);
	}
#endif

	if (dev)
		dev->Release();

	return static_cast<int>(msg.wParam);
}

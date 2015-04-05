#include <Windows.h>
#include <tchar.h>
#include <wrl/client.h>
#include <fstream>
#include <vector>
#include <tuple>
#include <algorithm>
#include <stdexcept>
#include <dxgi1_3.h>
#include <d3d12.h>
#include <d3dcompiler.h>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace std;
using Microsoft::WRL::ComPtr;

namespace
{
	const int WINDOW_WIDTH = 400;
	const int WINDOW_HEIGHT = 240;
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
	ComPtr<ID3D12Resource> mD3DBuffer;
	int mBufferWidth, mBufferHeight;
	UINT64 mFrameCount = 0;

	ID3D12Device* mDev;
	ComPtr<ID3D12CommandAllocator> mCmdAlloc;
	ComPtr<ID3D12CommandQueue> mCmdQueue;

	ComPtr<ID3D12GraphicsCommandList> mCmdList;
	ComPtr<ID3D12Fence> mFence;
	HANDLE mFenceEveneHandle = 0;

	ComPtr<ID3D12DescriptorHeap> mDescHeapRtv;
	ComPtr<ID3D12DescriptorHeap> mDescHeapCbvSrvUav;
	ComPtr<ID3D12DescriptorHeap> mDescHeapSampler;

	ComPtr<ID3D12RootSignature> mRootSignature;
	ComPtr<ID3D12PipelineState> mPso;
	ComPtr<ID3D12Resource> mTex;

public:
	D3D(int width, int height, HWND hWnd)
		: mBufferWidth(width), mBufferHeight(height), mDev(nullptr)
	{
		{
#if _DEBUG
			CHK(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(mDxgiFactory.ReleaseAndGetAddressOf())));
#else
			CHK(CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory2)));
#endif /* _DEBUG */
		}

		D3D12_CREATE_DEVICE_FLAG createFlag = D3D12_CREATE_DEVICE_NONE;
#if _DEBUG
		createFlag = D3D12_CREATE_DEVICE_DEBUG;
#endif /* _DEBUG */
		ID3D12Device* dev;
		CHK(D3D12CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_WARP,
			//D3D_DRIVER_TYPE_HARDWARE,
			createFlag,
			D3D_FEATURE_LEVEL_11_1,
			D3D12_SDK_VERSION,
			IID_PPV_ARGS(&dev)));
		mDev = dev;

		CHK(mDev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(mCmdAlloc.ReleaseAndGetAddressOf())));

		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		CHK(mDev->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(mCmdQueue.ReleaseAndGetAddressOf())));

		DXGI_SWAP_CHAIN_DESC1 scDesc = {};
		scDesc.Width = width;
		scDesc.Height = height;
		scDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		scDesc.SampleDesc.Count = 1;
		scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		scDesc.BufferCount = 1;
		scDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		CHK(mDxgiFactory->CreateSwapChainForHwnd(mCmdQueue.Get(), hWnd, &scDesc, nullptr, nullptr, mSwapChain.ReleaseAndGetAddressOf()));

		CHK(mDev->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			mCmdAlloc.Get(),
			nullptr,
			IID_PPV_ARGS(mCmdList.ReleaseAndGetAddressOf())));

		CHK(mDev->CreateFence(0, D3D12_FENCE_MISC_NONE, IID_PPV_ARGS(mFence.ReleaseAndGetAddressOf())));

		mFenceEveneHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);

		CHK(mSwapChain->GetBuffer(0, IID_PPV_ARGS(mD3DBuffer.ReleaseAndGetAddressOf())));
		mD3DBuffer->SetName(L"SwapChain_Buffer");

		{
			D3D12_DESCRIPTOR_HEAP_DESC desc = {};
			desc.Type = D3D12_RTV_DESCRIPTOR_HEAP;
			desc.NumDescriptors = 10;
			//desc.Flags = D3D12_DESCRIPTOR_HEAP_SHADER_VISIBLE;
			desc.NodeMask = 0;
			CHK(mDev->CreateDescriptorHeap(&desc, IID_PPV_ARGS(mDescHeapRtv.ReleaseAndGetAddressOf())));

			desc.Type = D3D12_CBV_SRV_UAV_DESCRIPTOR_HEAP;
			desc.NumDescriptors = 100;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_SHADER_VISIBLE;
			desc.NodeMask = 0;
			CHK(mDev->CreateDescriptorHeap(&desc, IID_PPV_ARGS(mDescHeapCbvSrvUav.ReleaseAndGetAddressOf())));

			desc.Type = D3D12_SAMPLER_DESCRIPTOR_HEAP;
			desc.NumDescriptors = 10;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_SHADER_VISIBLE;
			desc.NodeMask = 0;
			CHK(mDev->CreateDescriptorHeap(&desc, IID_PPV_ARGS(mDescHeapSampler.ReleaseAndGetAddressOf())));
		}

		mDev->CreateRenderTargetView(mD3DBuffer.Get(), nullptr, mDescHeapRtv->GetCPUDescriptorHandleForHeapStart());

		{
			D3D12_DESCRIPTOR_RANGE descRange1, descRange2;
			descRange1.Init(D3D12_DESCRIPTOR_RANGE_SRV, 1, 0);
			descRange2.Init(D3D12_DESCRIPTOR_RANGE_SAMPLER, 1, 0);

			D3D12_ROOT_PARAMETER rootParam[2];
			rootParam[0].InitAsDescriptorTable(1, &descRange1);
			rootParam[1].InitAsDescriptorTable(1, &descRange2);

			ID3D10Blob *sig, *info;
			D3D12_ROOT_SIGNATURE rootSigDesc = D3D12_ROOT_SIGNATURE();
			rootSigDesc.NumParameters = 2;
			rootSigDesc.NumStaticSamplers = 0;
			rootSigDesc.pParameters = rootParam;
			rootSigDesc.pStaticSamplers = nullptr;
			CHK(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_V1, &sig, &info));
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
			CHK(D3DCompileFromFile(L"TextureOptimize.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", flag, 0, &vs, &info));
			CHK(D3DCompileFromFile(L"TextureOptimize.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", flag, 0, &ps, &info));
		}
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.InputLayout.NumElements = 0;
		psoDesc.pRootSignature = mRootSignature.Get();
		psoDesc.VS.pShaderBytecode = vs->GetBufferPointer();
		psoDesc.VS.BytecodeLength = vs->GetBufferSize();
		psoDesc.PS.pShaderBytecode = ps->GetBufferPointer();
		psoDesc.PS.BytecodeLength = ps->GetBufferSize();
		psoDesc.RasterizerState = CD3D12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = CD3D12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = false;
		psoDesc.DepthStencilState.StencilEnable = false;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;
		CHK(mDev->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(mPso.ReleaseAndGetAddressOf())));
		vs->Release();
		ps->Release();

		int mipSlice = 1;
		{
			// Calcurate Mip
			unsigned int initialWidth = 256, initialHeight = 256;
			unsigned int bytePerPixel = 4;
			vector<tuple<unsigned int, unsigned int>> mipSize;
			auto maxSize = max(initialHeight, initialWidth);
			DWORD mipCount;
			if (maxSize == 1)
			{
				mipSize.resize(1);
				mipSize[0] = make_tuple(1, 1);
			}
			else
			{
				_BitScanReverse(&mipCount, maxSize - 1);
				mipSize.resize(mipCount + 2);
				int w = initialWidth, h = initialHeight;
				mipSize[0] = make_tuple(w, h);
				for (auto i = 1u; i < mipSize.size(); i++)
				{
					mipSize[i] = make_tuple(w /= 2, h /= 2);
				}
			}
			mipSlice = mipSize.size();

			// Read DDS File
			int totalTexSize = 0;
			for_each(mipSize.cbegin(),
					mipSize.cend(),
					[&](auto m) { totalTexSize += get<0>(m) * get<1>(m); });
			totalTexSize *= bytePerPixel;
			vector<char> texData(totalTexSize);

			ifstream ifs("d3d12.dds", ios::binary);
			if (!ifs)
				throw runtime_error("Texture not found.");
			ifs.seekg(128, ios::beg); // Skip DDS header
			ifs.read(texData.data(), texData.size());
			D3D12_RESOURCE_DESC resourceDesc = CD3D12_RESOURCE_DESC::Tex2D(
				DXGI_FORMAT_B8G8R8A8_UNORM, initialWidth, initialHeight, 1, (UINT16)mipSlice,
				1, 0, D3D12_RESOURCE_MISC_NONE,
				D3D12_TEXTURE_LAYOUT_UNKNOWN, 0);
			CHK(mDev->CreateCommittedResource(
				&CD3D12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_MISC_NONE,
				&resourceDesc,
				D3D12_RESOURCE_USAGE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(mTex.ReleaseAndGetAddressOf())));
			mTex->SetName(L"Texure");
			int copySrcOffset = 0;
			for (auto i = 0u; i < mipSize.size(); ++i)
			{
				D3D12_BOX box = {};
				box.right = get<0>(mipSize[i]);
				box.bottom = get<1>(mipSize[i]);
				box.back = 1;
				int copySrcSize = bytePerPixel * box.right * box.bottom;
				CHK(mTex->WriteToSubresource(
					i,
					&box,
					texData.data() + copySrcOffset,
					bytePerPixel * box.right,
					0));
				copySrcOffset += copySrcSize;
			}
		}
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = mipSlice;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.PlaneSlice = 0;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		mDev->CreateShaderResourceView(mTex.Get(), &srvDesc, mDescHeapCbvSrvUav->GetCPUDescriptorHandleForHeapStart());

		D3D12_SAMPLER_DESC samplerDesc;
		samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.MinLOD = -FLT_MAX;
		samplerDesc.MaxLOD = FLT_MAX;
		samplerDesc.MipLODBias = 0;
		samplerDesc.MaxAnisotropy = 0;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_NEVER;
		mDev->CreateSampler(&samplerDesc, mDescHeapSampler->GetCPUDescriptorHandleForHeapStart());
	}
	~D3D()
	{
		CloseHandle(mFenceEveneHandle);
	}
	ID3D12Device* GetDevice() const
	{
		return mDev;
	}
	void Draw()
	{
		// Set queue flushed event
		CHK(mFence->SetEventOnCompletion(mFrameCount, mFenceEveneHandle));

		D3D12_CPU_DESCRIPTOR_HANDLE descHandleRtv = mDescHeapRtv->GetCPUDescriptorHandleForHeapStart();

		// Barrier Present -> RenderTarget
		setResourceBarrier(mCmdList.Get(), mD3DBuffer.Get(), D3D12_RESOURCE_USAGE_PRESENT, D3D12_RESOURCE_USAGE_RENDER_TARGET);

		// Viewport & Scissor
		D3D12_VIEWPORT viewport = {};
		viewport.Width = (float)mBufferWidth;
		viewport.Height = (float)mBufferHeight;
		mCmdList->RSSetViewports(1, &viewport);
		D3D12_RECT scissor = {};
		scissor.right = (LONG)mBufferWidth;
		scissor.bottom = (LONG)mBufferHeight;
		mCmdList->RSSetScissorRects(1, &scissor);

		// Clear
		{
			auto saturate = [](float a) { return a < 0 ? 0 : a > 1 ? 1 : a; };
			float clearColor[4];
			static float h = 0.0f;
			h += 0.02f;
			if (h >= 1) h = 0.0f;
			clearColor[0] = saturate(std::abs(h * 6.0f - 3.0f) - 1.0f);
			clearColor[1] = saturate(2.0f - std::abs(h * 6.0f - 2.0f));
			clearColor[2] = saturate(2.0f - std::abs(h * 6.0f - 4.0f));
			mCmdList->ClearRenderTargetView(descHandleRtv, clearColor, nullptr, 0);
		}

		mCmdList->SetRenderTargets(&descHandleRtv, true, 1, nullptr);

		// Draw
		{
			mCmdList->SetGraphicsRootSignature(mRootSignature.Get());
			ID3D12DescriptorHeap* descHeaps[] = { mDescHeapCbvSrvUav.Get(), mDescHeapSampler.Get() };
			mCmdList->SetDescriptorHeaps(descHeaps, ARRAYSIZE(descHeaps));
			mCmdList->SetGraphicsRootDescriptorTable(0, mDescHeapCbvSrvUav->GetGPUDescriptorHandleForHeapStart());
			mCmdList->SetGraphicsRootDescriptorTable(1, mDescHeapSampler->GetGPUDescriptorHandleForHeapStart());
			mCmdList->SetPipelineState(mPso.Get());
			mCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			mCmdList->DrawInstanced(4, 1, 0, 0);
		}

		// Barrier RenderTarget -> Present
		setResourceBarrier(mCmdList.Get(), mD3DBuffer.Get(), D3D12_RESOURCE_USAGE_RENDER_TARGET, D3D12_RESOURCE_USAGE_PRESENT);

		// Exec
		CHK(mCmdList->Close());
		ID3D12CommandList* const cmdList = mCmdList.Get();
		mCmdQueue->ExecuteCommandLists(1, &cmdList);

		// Present
		CHK(mSwapChain->Present(1, 0));

		// Wait for queue flushed
		// CPU stall occured!
		CHK(mCmdQueue->Signal(mFence.Get(), mFrameCount));
		DWORD wait = WaitForSingleObject(mFenceEveneHandle, 10000);
		if (wait != WAIT_OBJECT_0)
			throw runtime_error("Failed WaitForSingleObject().");

		mFrameCount++;

		CHK(mCmdAlloc->Reset());
		CHK(mCmdList->Reset(mCmdAlloc.Get(), nullptr));
	}

private:
	void setResourceBarrier(ID3D12GraphicsCommandList* commandList, ID3D12Resource* res, UINT before, UINT after)
	{
		D3D12_RESOURCE_BARRIER_DESC desc = {};
		desc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		desc.Transition.pResource = res;
		desc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		desc.Transition.StateBefore = before;
		desc.Transition.StateAfter = after;
		desc.Transition.Flags = D3D12_RESOURCE_TRANSITION_BARRIER_NONE;
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

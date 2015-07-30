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
#include "../_common/dxcommon.h"

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace std;
using Microsoft::WRL::ComPtr;

#define USE_BC1_TEXTURE 1

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
	ComPtr<ID3D12Resource> mTexUpload;
	ComPtr<ID3D12Resource> mTexDefault;
	vector<tuple<unsigned int, unsigned int, unsigned int>> mTexMipSize; // <0>width, <1>height, <2>uploadHeapOffset

	unsigned int texAlign(unsigned int s) {
		return (s + D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1u) & ~(D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT - 1);
	};
	unsigned int pitchAlign(unsigned int w) {
		return (w + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
	};

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
		scDesc.BufferCount = BUFFER_COUNT;
		scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
		CHK(mDxgiFactory->CreateSwapChainForHwnd(mCmdQueue.Get(), hWnd, &scDesc, nullptr, nullptr, mSwapChain.ReleaseAndGetAddressOf()));

		CHK(mDev->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			mCmdAlloc.Get(),
			nullptr,
			IID_PPV_ARGS(mCmdList.ReleaseAndGetAddressOf())));

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

			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			desc.NumDescriptors = 100;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			desc.NodeMask = 0;
			CHK(mDev->CreateDescriptorHeap(&desc, IID_PPV_ARGS(mDescHeapCbvSrvUav.ReleaseAndGetAddressOf())));

			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			desc.NumDescriptors = 10;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			desc.NodeMask = 0;
			CHK(mDev->CreateDescriptorHeap(&desc, IID_PPV_ARGS(mDescHeapSampler.ReleaseAndGetAddressOf())));
		}

		auto rtvStep = mDev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		for (auto i = 0u; i < BUFFER_COUNT; i++)
		{
			auto d = mDescHeapRtv->GetCPUDescriptorHandleForHeapStart();
			d.ptr += i * rtvStep;
			mDev->CreateRenderTargetView(mD3DBuffer[i].Get(), nullptr, d);
		}

		{
			CD3DX12_DESCRIPTOR_RANGE descRange1, descRange2;
			descRange1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
			descRange2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

			CD3DX12_ROOT_PARAMETER rootParam[2];
			rootParam[0].InitAsDescriptorTable(1, &descRange1);
			rootParam[1].InitAsDescriptorTable(1, &descRange2);

			ID3D10Blob *sig, *info;
			auto rootSigDesc = D3D12_ROOT_SIGNATURE_DESC();
			rootSigDesc.NumParameters = 2;
			rootSigDesc.NumStaticSamplers = 0;
			rootSigDesc.pParameters = rootParam;
			rootSigDesc.pStaticSamplers = nullptr;
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
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(CCCD3DX12_DEFAULT());
		psoDesc.BlendState = CD3DX12_BLEND_DESC(CCCD3DX12_DEFAULT());
		psoDesc.DepthStencilState.DepthEnable = false;
		psoDesc.DepthStencilState.StencilEnable = false;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;
		CHK(mDev->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(mPso.ReleaseAndGetAddressOf())));
		vs->Release();
		ps->Release();

		{
			// Calcurate Mip
			unsigned int initialWidth = 256, initialHeight = 256;
			unsigned int bytePerPixel = 4;
#if USE_BC1_TEXTURE
			unsigned int bcByte = 8;
#endif
			auto minSize = min(initialHeight, initialWidth);
			DWORD mipCount;
			unsigned int copyDestOffset = 0;
			unsigned int copyDestTotalSize = 0;
			if (minSize == 1)
			{
				mTexMipSize.resize(1);
				mTexMipSize[0] = make_tuple(1, 1, copyDestOffset);
			}
			else
			{
				_BitScanReverse(&mipCount, minSize - 1);
				mTexMipSize.resize(mipCount + 2);
				int w = initialWidth;
				int h = initialHeight;
				mTexMipSize[0] = make_tuple(w, h, copyDestOffset);
				for (auto i = 1u; i < mTexMipSize.size(); i++)
				{
					int cw = w, ch = h;
#if USE_BC1_TEXTURE
					cw = (w + 3) / 4;
					ch = (h + 3) / 4;
#endif
					copyDestOffset += texAlign(pitchAlign(bytePerPixel * cw) * ch);
					mTexMipSize[i] = make_tuple(w /= 2, h /= 2, copyDestOffset);
				}
			}
			auto& lastSlice = mTexMipSize[mTexMipSize.size() - 1];
			copyDestTotalSize = get<2>(lastSlice) + texAlign(pitchAlign(bytePerPixel * get<0>(lastSlice)) * get<1>(lastSlice));

			// Read DDS File
			int totalTexSize = 0;
#if USE_BC1_TEXTURE
			for_each(mTexMipSize.cbegin(),
				mTexMipSize.cend(),
				[&](auto m) {
				totalTexSize += ((get<0>(m) + 3) / 4) * ((get<1>(m) + 3) / 4) * bcByte;
			});
#else
			for_each(mTexMipSize.cbegin(),
				mTexMipSize.cend(),
				[&](auto m) { totalTexSize += get<0>(m) * get<1>(m) * bytePerPixel; });
#endif
			vector<char> texData(totalTexSize);

#if USE_BC1_TEXTURE
			ifstream ifs("d3d12_bc1.dds", ios::binary);
#else
			ifstream ifs("d3d12.dds", ios::binary);
#endif
			if (!ifs)
				throw runtime_error("Texture not found.");
			ifs.seekg(128, ios::beg); // Skip DDS header
			ifs.read(texData.data(), texData.size());
			auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(copyDestTotalSize, D3D12_RESOURCE_FLAG_NONE, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
			CHK(mDev->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&resourceDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(mTexUpload.ReleaseAndGetAddressOf())));
			mTexUpload->SetName(L"Texure");

			int copySrcOffset = 0;
			char *dest;
			CHK(mTexUpload->Map(0, nullptr, reinterpret_cast<void**>(&dest)));
			for (auto i = 0u; i < mTexMipSize.size(); ++i)
			{
				auto curSlice = mTexMipSize[i];
				auto cw = get<0>(curSlice), ch = get<1>(curSlice);
#if USE_BC1_TEXTURE
				cw = (cw + 3) / 4;
				ch = (ch + 3) / 4;
#endif
				for (auto h = 0u; h < ch; ++h)
				{
					auto r = get<2>(curSlice) + pitchAlign(bytePerPixel * cw) * h;
#if USE_BC1_TEXTURE
					auto s = bcByte * cw;
#else
					auto s = bytePerPixel * cw;
#endif
					memcpy_s(dest + r, s, texData.data() + copySrcOffset, s); // To maximize WC memory writing performance, size should be aligned by 16 byte on x86.
					copySrcOffset += s;
				}
			}
			mTexUpload->Unmap(0, nullptr);
		}

		DXGI_FORMAT texFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
#if USE_BC1_TEXTURE
		texFormat = DXGI_FORMAT_BC1_UNORM;
#endif
		{
			auto resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
				texFormat, get<0>(mTexMipSize[0]), get<1>(mTexMipSize[0]), 1, (UINT16)mTexMipSize.size(),
				1, 0, D3D12_RESOURCE_FLAG_NONE,
				D3D12_TEXTURE_LAYOUT_UNKNOWN, 0);
			CHK(mDev->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&resourceDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(mTexDefault.ReleaseAndGetAddressOf())));
		}
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = texFormat;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = mTexMipSize.size();
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.PlaneSlice = 0;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		mDev->CreateShaderResourceView(mTexDefault.Get(), &srvDesc, mDescHeapCbvSrvUav->GetCPUDescriptorHandleForHeapStart());

		D3D12_SAMPLER_DESC samplerDesc;
		samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.MinLOD = -FLT_MAX;
		samplerDesc.MaxLOD = FLT_MAX;
		samplerDesc.MipLODBias = 0;
		samplerDesc.MaxAnisotropy = 0;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
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
		mFrameCount++;

		// Get current RTV descriptor
		auto descHandleRtvStep = mDev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		D3D12_CPU_DESCRIPTOR_HANDLE descHandleRtv = mDescHeapRtv->GetCPUDescriptorHandleForHeapStart();
		descHandleRtv.ptr += ((mFrameCount - 1) % BUFFER_COUNT) * descHandleRtvStep;
		// Get current swap chain
		ID3D12Resource* d3dBuffer = mD3DBuffer[(mFrameCount - 1) % BUFFER_COUNT].Get();

		// Copy texture from upload heap to default heap
		if (mFrameCount == 1)
		{
			for (auto i = 0u; i < mTexMipSize.size(); i++)
			{
				D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
				srcLoc.pResource = mTexUpload.Get();
				srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
				srcLoc.PlacedFootprint.Offset = get<2>(mTexMipSize[i]);
#if USE_BC1_TEXTURE
				srcLoc.PlacedFootprint.Footprint.Format = DXGI_FORMAT_BC1_UNORM;
				srcLoc.PlacedFootprint.Footprint.Width = max(4, get<0>(mTexMipSize[i]));
				srcLoc.PlacedFootprint.Footprint.Height = max(4, get<1>(mTexMipSize[i]));
				auto cw = (get<0>(mTexMipSize[i]) + 3) / 4;
				srcLoc.PlacedFootprint.Footprint.RowPitch = pitchAlign(cw * 8);
#else
				srcLoc.PlacedFootprint.Footprint.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
				srcLoc.PlacedFootprint.Footprint.Width = get<0>(mTexMipSize[i]);
				srcLoc.PlacedFootprint.Footprint.Height = get<1>(mTexMipSize[i]);
				srcLoc.PlacedFootprint.Footprint.RowPitch = pitchAlign(get<0>(mTexMipSize[i]) * 4);
#endif
				srcLoc.PlacedFootprint.Footprint.Depth = 1;
				D3D12_TEXTURE_COPY_LOCATION destLoc = {};
				destLoc.pResource = mTexDefault.Get();
				destLoc.SubresourceIndex = i;
				destLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				D3D12_BOX box = {};
#if USE_BC1_TEXTURE
				box.right = max(4, get<0>(mTexMipSize[i]));
				box.bottom = max(4, get<1>(mTexMipSize[i]));
#else
				box.right = get<0>(mTexMipSize[i]);
				box.bottom = get<1>(mTexMipSize[i]);
#endif
				box.back = 1;
				mCmdList->CopyTextureRegion(&destLoc, 0, 0, 0, &srcLoc, &box);
			}

			setResourceBarrier(mCmdList.Get(), mTexDefault.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}

		// Release intermediate texture
		// Attention: Texture deleting must do while it is NOT referred by command list, or you will get segfalut or device removing.
		if (mFrameCount == 2)
		{
			mTexUpload.Reset();
		}

		// Barrier Present -> RenderTarget
		setResourceBarrier(mCmdList.Get(), d3dBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

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
			mCmdList->ClearRenderTargetView(descHandleRtv, clearColor, 0, nullptr);
		}

		mCmdList->OMSetRenderTargets(1, &descHandleRtv, true, nullptr);

		// Draw
		{
			mCmdList->SetGraphicsRootSignature(mRootSignature.Get());
			ID3D12DescriptorHeap* descHeaps[] = { mDescHeapCbvSrvUav.Get(), mDescHeapSampler.Get() };
			mCmdList->SetDescriptorHeaps(ARRAYSIZE(descHeaps), descHeaps);
			mCmdList->SetGraphicsRootDescriptorTable(0, mDescHeapCbvSrvUav->GetGPUDescriptorHandleForHeapStart());
			mCmdList->SetGraphicsRootDescriptorTable(1, mDescHeapSampler->GetGPUDescriptorHandleForHeapStart());
			mCmdList->SetPipelineState(mPso.Get());
			mCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			mCmdList->DrawInstanced(4, 1, 0, 0);
		}

		// Barrier RenderTarget -> Present
		setResourceBarrier(mCmdList.Get(), d3dBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

		// Exec
		CHK(mCmdList->Close());
		ID3D12CommandList* const cmdList = mCmdList.Get();
		mCmdQueue->ExecuteCommandLists(1, &cmdList);

		// Present
		CHK(mSwapChain->Present(1, 0));

		// Set queue flushed event
		CHK(mFence->SetEventOnCompletion(mFrameCount, mFenceEveneHandle));

		// Wait for queue flushed
		// This code would occur CPU stall!
		CHK(mCmdQueue->Signal(mFence.Get(), mFrameCount));
		DWORD wait = WaitForSingleObject(mFenceEveneHandle, 10000);
		if (wait != WAIT_OBJECT_0)
			throw runtime_error("Failed WaitForSingleObject().");

		CHK(mCmdAlloc->Reset());
		CHK(mCmdList->Reset(mCmdAlloc.Get(), nullptr));
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

#pragma once

//#include <d3dx12.h>
// Helper structures and functions for D3D12 not found on Standalone Windows SDK for Windows 10
// so I made emulated code.

struct CD3DX12_DEFAULT { };

struct CD3DX12_RASTERIZER_DESC : D3D12_RASTERIZER_DESC
{
	CD3DX12_RASTERIZER_DESC(CD3DX12_DEFAULT)
	{
		FillMode = D3D12_FILL_MODE_SOLID;
		CullMode = D3D12_CULL_MODE_BACK;
		FrontCounterClockwise = FALSE;
		DepthBias = 0;
		DepthBiasClamp = 0.0f;
		SlopeScaledDepthBias = 0.0f;
		DepthClipEnable = TRUE;
		MultisampleEnable = FALSE;
		AntialiasedLineEnable = FALSE;
		ForcedSampleCount = 0;
		ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	}
};

struct CD3DX12_BLEND_DESC : D3D12_BLEND_DESC
{
	CD3DX12_BLEND_DESC(CD3DX12_DEFAULT)
	{
		AlphaToCoverageEnable = FALSE;
		IndependentBlendEnable = FALSE;
		for (auto& rt : RenderTarget)
		{
			rt.BlendEnable = FALSE;
			rt.LogicOpEnable = FALSE;
			rt.SrcBlend = D3D12_BLEND_ONE;
			rt.DestBlend = D3D12_BLEND_ZERO;
			rt.BlendOp = D3D12_BLEND_OP_ADD;
			rt.SrcBlendAlpha = D3D12_BLEND_ONE;
			rt.DestBlendAlpha = D3D12_BLEND_ZERO;
			rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
			rt.LogicOp = D3D12_LOGIC_OP_NOOP;
			rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		}
	}
};

struct CD3DX12_DEPTH_STENCIL_DESC : D3D12_DEPTH_STENCIL_DESC
{
	CD3DX12_DEPTH_STENCIL_DESC(CD3DX12_DEFAULT)
	{
		DepthEnable = TRUE;
		DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		StencilEnable = FALSE;
		StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
		StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
		FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
		FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
		FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
		BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
		BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
		BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	}
};

struct CD3DX12_HEAP_PROPERTIES : D3D12_HEAP_PROPERTIES
{
	CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE type)
	{
		Type = type;
		CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		CreationNodeMask = 0;
		VisibleNodeMask = 0;
	}

	bool IsCPUAccessible() const
	{
		switch (Type)
		{
		case D3D12_HEAP_TYPE_UPLOAD:
		case D3D12_HEAP_TYPE_READBACK:
			return true;
		case D3D12_HEAP_TYPE_CUSTOM:
			switch (CPUPageProperty)
			{
			case D3D12_CPU_PAGE_PROPERTY_WRITE_BACK:
			case D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE:
				return true;
			}
			return false;
		}
		return false;
	}

	operator const D3D12_HEAP_PROPERTIES&() { return *this; }
};

struct CD3DX12_RESOURCE_DESC : D3D12_RESOURCE_DESC
{
	static D3D12_RESOURCE_DESC Buffer(UINT64 size)
	{
		D3D12_RESOURCE_DESC desc;
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Alignment = 0;
		desc.Width = size;
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;
		return desc;
	}
};

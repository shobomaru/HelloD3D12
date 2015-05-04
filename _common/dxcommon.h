#pragma once

//#include <d3dx12.h>
// Helper structures and functions for D3D12 not found on Standalone Windows SDK for Windows 10
// so I made emulated code.

struct CD3DX12_DEFAULT { };
const static CD3DX12_DEFAULT D3DX12_DEFAULT;

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
	static D3D12_RESOURCE_DESC Buffer(
		UINT64 width,
		D3D12_RESOURCE_FLAGS miscFlags = D3D12_RESOURCE_FLAG_NONE,
		UINT64 alignment = 0)
	{
		D3D12_RESOURCE_DESC desc;
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Alignment = alignment;
		desc.Width = width;
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags = miscFlags;
		return desc;
	}

	static D3D12_RESOURCE_DESC Tex2D(
		DXGI_FORMAT format,
		UINT64 width,
		UINT height,
		UINT16 arraySize = 1,
		UINT16 mipLelels = 0,
		UINT sampleCount = 1,
		UINT sampleQuality = 0,
		D3D12_RESOURCE_FLAGS miscFlags = D3D12_RESOURCE_FLAG_NONE,
		D3D12_TEXTURE_LAYOUT layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
		UINT64 alignment = 0)
	{
		D3D12_RESOURCE_DESC desc;
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Alignment = alignment;
		desc.Width = width;
		desc.Height = height;
		desc.DepthOrArraySize = arraySize;
		desc.MipLevels = mipLelels;
		desc.Format = format;
		desc.SampleDesc.Count = sampleCount;
		desc.SampleDesc.Quality = sampleQuality;
		desc.Layout = layout;
		desc.Flags = miscFlags;
		return desc;
	}
};

struct CD3DX12_DESCRIPTOR_RANGE : D3D12_DESCRIPTOR_RANGE
{
	void Init(
		D3D12_DESCRIPTOR_RANGE_TYPE rangeType,
		UINT numDescriptors,
		UINT baseShaderRegister,
		UINT registerSpace = 0,
		UINT offsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND)
	{
		RangeType = rangeType;
		NumDescriptors = numDescriptors;
		BaseShaderRegister = baseShaderRegister;
		RegisterSpace = registerSpace;
		OffsetInDescriptorsFromTableStart = offsetInDescriptorsFromTableStart;
	}
};

struct CD3DX12_ROOT_DESCRIPTOR_TABLE : D3D12_ROOT_DESCRIPTOR_TABLE
{
	void Init(
		UINT numDescriptorRanges,
		const D3D12_DESCRIPTOR_RANGE* _pDescriptorRanges)
	{
		NumDescriptorRanges = numDescriptorRanges;
		pDescriptorRanges = _pDescriptorRanges;
	}
};

struct CD3DX12_ROOT_CONSTANTS : D3D12_ROOT_CONSTANTS
{
	void Init(
		UINT num32BitValues,
		UINT shaderRegister,
		UINT registerSpace = 0)
	{
		Num32BitValues = num32BitValues;
		ShaderRegister = shaderRegister;
		RegisterSpace = registerSpace;
	}
};

struct CD3DX12_ROOT_DESCRIPTOR : D3D12_ROOT_DESCRIPTOR
{
	void Init(UINT shaderRegister, UINT registerSpace = 0)
	{
		ShaderRegister = shaderRegister;
		RegisterSpace = registerSpace;
	}
};

struct CD3DX12_ROOT_PARAMETER : D3D12_ROOT_PARAMETER
{
	void InitAsDescriptorTable(
		UINT numDescriptorRanges,
		const CD3DX12_DESCRIPTOR_RANGE* pDescriptorRanges,
		D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL)
	{
		ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		ShaderVisibility = visibility;
		static_cast<CD3DX12_ROOT_DESCRIPTOR_TABLE&>(DescriptorTable).Init(
			numDescriptorRanges,
			pDescriptorRanges);
	}

	void InitAsConstants(
		UINT num32BitValues,
		UINT shaderRegister,
		UINT registerSpace = 0,
		D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL)
	{
		ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		ShaderVisibility = visibility;
		static_cast<CD3DX12_ROOT_CONSTANTS&>(Constants).Init(
			num32BitValues, shaderRegister, registerSpace);
	}

	void InitAsConstantBufferView(
		UINT shaderRegister,
		UINT registerSpace = 0,
		D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL)
	{
		ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		ShaderVisibility = visibility;
		static_cast<CD3DX12_ROOT_DESCRIPTOR&>(Descriptor).Init(
			shaderRegister, registerSpace);
	}
};


struct VSIn
{
	float3 pos : POSITION;
	float3 normal : NORMAL;
};

struct VSOut
{
	float4 pos : SV_POSITION;
	float3 normal : NORMAL;
};

#define MaxFrameLatency (2)

cbuffer SceneParam
{
	float4x4 worldViewProjMatrix[MaxFrameLatency];
	float4x4 worldMatrix[MaxFrameLatency];
};

cbuffer SceneIndex
{
	uint sceneParamIndex;
};

VSOut VSMain(VSIn vsIn)
{
	VSOut output;
	output.pos = mul(float4(vsIn.pos.xyz, 1), worldViewProjMatrix[sceneParamIndex]);
	output.normal = mul(vsIn.normal.xyz, (float3x3)(worldMatrix[sceneParamIndex]));
	return output;
}

float4 PSMain(VSOut vsOut) : SV_TARGET
{
	return float4(vsOut.normal.xyz * 0.5 + 0.5, 1);
}
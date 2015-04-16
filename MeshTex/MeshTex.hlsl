struct VSIn
{
	float3 pos : POSITION;
	float2 texCoord : TEXCOORD;
};

struct VSOut
{
	float4 pos : SV_POSITION;
	float2 texCoord : TEXCOORD;
};

cbuffer Scene
{
	float4x4 worldViewProjMatrix;
	float4x4 worldMatrix;
};

VSOut VSMain(VSIn vsIn)
{
	VSOut output;
	output.pos = mul(float4(vsIn.pos.xyz, 1), worldViewProjMatrix);
	output.texCoord = vsIn.texCoord;
	return output;
}

Texture2D<float4> tBase : register(t0);
SamplerState sLinear : register(s0);

float4 PSMain(VSOut vsOut) : SV_TARGET
{
	return float4(tBase.SampleLevel(sLinear, vsOut.texCoord, 0).rgb, 1);
}
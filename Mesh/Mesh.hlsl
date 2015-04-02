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

cbuffer Scene
{
	float4x4 worldViewProjMatrix;
	float4x4 worldMatrix;
};

VSOut VSMain(VSIn vsIn)
{
	VSOut output;
	output.pos = mul(float4(vsIn.pos.xyz, 1), worldViewProjMatrix);
	output.normal = mul(vsIn.normal.xyz, (float3x3)(worldMatrix));
	return output;
}

float4 PSMain(VSOut vsOut) : SV_TARGET
{
	return float4(vsOut.normal.xyz * 0.5 + 0.5, 1);
}
struct VSOut
{
	float4 pos : SV_POSITION;
	float2 texcoord : TEXCOORD;
};

VSOut VSMain(float3 pos : POSITION)
{
	VSOut output;
	output.texcoord = float2(0, 0);
	output.pos = float4(pos.xyz, 1);
	return output;
}

float4 PSMain(VSOut vsOut) : SV_TARGET
{
	return float4(0.9, 0.9, 0.9, 1);
}
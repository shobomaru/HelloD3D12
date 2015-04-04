struct VSOut
{
	float4 pos : SV_POSITION;
	float2 texcoord : TEXCOORD;
};

VSOut VSMain(uint vertexID : SV_VERTEXID)
{
	VSOut output;
	output.texcoord = float2((vertexID << 1) & 2, vertexID & 2);
	output.pos = float4(output.texcoord * float2(2, -2) + float2(-1, 1), 0, 1);
	output.pos.xy *= 0.9;
	return output;
}

Texture2D<float4> tBase : register(t0);
SamplerState sLinear : register(s0);

float4 PSMain(VSOut vsOut) : SV_TARGET
{
	return tBase.SampleLevel(sLinear, vsOut.texcoord, 2);
}
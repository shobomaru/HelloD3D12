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

void PSMain(VSOut vsOut)
{
}

Texture2D<float> tDepth : register(t0);
SamplerState sLinear : register(s0);

VSOut VSDrawDepth(uint vertexID : SV_VERTEXID)
{
	VSOut output;
	output.texcoord = float2((vertexID << 1) & 2, vertexID & 2);
	output.pos = float4(output.texcoord * float2(2, -2) + float2(-1, 1), 0, 1);
	return output;
}

float4 PSDrawDepth(VSOut vsOut) : SV_TARGET
{
	float c = tDepth.SampleLevel(sLinear, vsOut.texcoord, 0);
	return float4(c.rrr, 1);
}
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

cbuffer bScene : register(b0)
{
	float ColorMultiply;
};
//ConstantBuffer<float> ColorMultiply : register(b0); // Cannot complie new CB syntax on D3DCompiler_47

float4 PSMain(VSOut vsOut) : SV_TARGET
{
	float4 c = tBase.SampleLevel(sLinear, vsOut.texcoord, 0);
	return float4(c.xyz * ColorMultiply, c.w);
}
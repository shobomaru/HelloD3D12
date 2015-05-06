RWStructuredBuffer<uint> Buf;

[numthreads(14, 1, 1)]
void CSMain(uint3 tid : SV_DispatchThreadID)
{
	// Hello, World!
	uint gOutputString[14] = { 72, 101, 108, 108, 111, 44, 32, 119, 111, 114, 108, 100, 33, 0 };

	// Write message
	Buf[tid.x] = gOutputString[tid.x];
}

struct FullscreenVSOutput
{
	float4 positionCS : SV_Position;
	float2 texcoord   : TEXCOORD0;
};

FullscreenVSOutput main(uint vertexId : SV_VertexID)
{
	FullscreenVSOutput output;
	output.texcoord = float2((vertexId << 1) & 2, vertexId & 2);
	output.positionCS = float4(output.texcoord * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
	return output;
}

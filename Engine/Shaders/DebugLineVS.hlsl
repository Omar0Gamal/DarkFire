#include "Common.hlsli"

struct VSLineInput
{
	float3 position : POSITION;
	float4 color    : COLOR;
};

struct PSLineInput
{
	float4 positionCS : SV_POSITION;
	float4 color      : COLOR;
};

PSLineInput main(VSLineInput input)
{
	PSLineInput output;
	output.positionCS = mul(float4(input.position, 1.0f), g_View.viewProjMatrix);
	output.color = input.color;
	return output;
}

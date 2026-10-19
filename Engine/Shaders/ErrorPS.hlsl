#include "Common.hlsli"

struct PSOutput
{
	float4 color    : SV_Target0;
	uint   entityId : SV_Target1;
};

PSOutput main(PSInput input)
{
	PSOutput output;
	// Magenta error fallback color
	output.color = float4(1.0f, 0.0f, 1.0f, 1.0f);
	output.entityId = input.entityId;
	return output;
}

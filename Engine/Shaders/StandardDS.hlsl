#include "Common.hlsli"

struct HSConstantOutput
{
	float edges[3] : SV_TessFactor;
	float inside   : SV_InsideTessFactor;
};

float3 projectOntoPlane(float3 pt, float3 planePt, float3 planeNormal)
{
	return pt - dot(pt - planePt, planeNormal) * planeNormal;
}

[domain("tri")]
PSInput main(
	HSConstantOutput input,
	float3 bary : SV_DomainLocation,
	const OutputPatch<PSInput, 3> patch)
{
	PSInput output;

	// Linear barycentric interpolation
	float3 linearPos = bary.x * patch[0].positionWS + bary.y * patch[1].positionWS + bary.z * patch[2].positionWS;
	float3 normalWS  = normalize(bary.x * patch[0].normalWS + bary.y * patch[1].normalWS + bary.z * patch[2].normalWS);
	float2 texcoord  = bary.x * patch[0].texcoord + bary.y * patch[1].texcoord + bary.z * patch[2].texcoord;
	float4 tangentWS = bary.x * patch[0].tangentWS + bary.y * patch[1].tangentWS + bary.z * patch[2].tangentWS;
	tangentWS.xyz    = normalize(tangentWS.xyz);
	float4 color     = bary.x * patch[0].color + bary.y * patch[1].color + bary.z * patch[2].color;

	// Curved Phong Tessellation (Boubekeur & Alexa 2008) for smooth silhouettes
	float3 c0 = projectOntoPlane(linearPos, patch[0].positionWS, patch[0].normalWS);
	float3 c1 = projectOntoPlane(linearPos, patch[1].positionWS, patch[1].normalWS);
	float3 c2 = projectOntoPlane(linearPos, patch[2].positionWS, patch[2].normalWS);
	float3 phongPos = bary.x * c0 + bary.y * c1 + bary.z * c2;

	float phongAlpha = clamp(g_Pass.shGridMin.w, 0.0f, 0.75f);
	float3 finalPosWS = lerp(linearPos, phongPos, phongAlpha);

	output.positionWS = finalPosWS;
	output.positionCS = mul(float4(finalPosWS, 1.0f), g_View.viewProjMatrix);
	output.normalWS   = normalWS;
	output.texcoord   = texcoord;
	output.tangentWS  = tangentWS;
	output.color      = color;
	output.entityId   = patch[0].entityId;

	return output;
}

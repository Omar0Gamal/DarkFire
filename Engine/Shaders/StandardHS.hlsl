#include "Common.hlsli"

struct HSConstantOutput
{
	float edges[3] : SV_TessFactor;
	float inside   : SV_InsideTessFactor;
};

HSConstantOutput CalcHSPatchConstants(
	InputPatch<PSInput, 3> patch,
	uint patchId : SV_PrimitiveID)
{
	HSConstantOutput pt;

	// Alpha-cutout foliage, two-sided fabrics and thin chains should not be tessellated (prevents mesh warping and black flickering)
	bool isThinSurface = (g_Material.alphaCutoff > 0.001f) || ((g_Material.materialFlags & (DF_MAT_FLAG_TWO_SIDED | DF_MAT_FLAG_CLOTH)) != 0);
	if (isThinSurface)
	{
		pt.edges[0] = 1.0f;
		pt.edges[1] = 1.0f;
		pt.edges[2] = 1.0f;
		pt.inside   = 1.0f;
		return pt;
	}

	float maxTess = clamp(g_Pass.shGridMin.x, 1.0f, 8.0f);
	float minDist = max(g_Pass.shGridMin.y, 0.5f);
	float maxDist = max(g_Pass.shGridMin.z, minDist + 1.0f);

	float3 mid0 = 0.5f * (patch[1].positionWS + patch[2].positionWS);
	float3 mid1 = 0.5f * (patch[2].positionWS + patch[0].positionWS);
	float3 mid2 = 0.5f * (patch[0].positionWS + patch[1].positionWS);

	float d0 = distance(mid0, g_View.cameraPosition);
	float d1 = distance(mid1, g_View.cameraPosition);
	float d2 = distance(mid2, g_View.cameraPosition);

	float f0 = saturate((maxDist - d0) / (maxDist - minDist));
	float f1 = saturate((maxDist - d1) / (maxDist - minDist));
	float f2 = saturate((maxDist - d2) / (maxDist - minDist));

	pt.edges[0] = lerp(1.0f, maxTess, f0 * f0);
	pt.edges[1] = lerp(1.0f, maxTess, f1 * f1);
	pt.edges[2] = lerp(1.0f, maxTess, f2 * f2);
	pt.inside   = (pt.edges[0] + pt.edges[1] + pt.edges[2]) / 3.0f;

	return pt;
}

[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("CalcHSPatchConstants")]
[maxtessfactor(8.0f)]
PSInput main(
	InputPatch<PSInput, 3> patch,
	uint i : SV_OutputControlPointID,
	uint patchId : SV_PrimitiveID)
{
	return patch[i];
}

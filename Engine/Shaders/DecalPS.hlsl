// DarkFire Engine - DecalPS.hlsl
// Dual-Pipeline (Forward+ & Deferred) OBB Projected Decal Pixel Shader (ps_5_0)

#define g_TextureDepth g_TextureDepth_Slot9Unused
#include "Common.hlsli"
#undef g_TextureDepth

// Hardware Reversed-Z depth buffer bound at slot 8 for decal pass
Texture2D g_TextureDepth : register(t8);

struct DecalPSInput
{
	float4 positionCS : SV_Position;
	float4 clipPos    : TEXCOORD0;
};

float4 main(DecalPSInput input) : SV_Target0
{
	// 1. Screen-space coordinates from SV_Position
	float2 screenDimensions = g_View.viewportSize;
	float2 screenUV = input.positionCS.xy / screenDimensions;

	// 2. Sample hardware Reversed-Z depth (0.0 = far plane/sky, 1.0 = near plane)
	float depthZ = g_TextureDepth.SampleLevel(g_SamplerPointClamp, screenUV, 0).r;
	if (depthZ <= 0.000001f)
	{
		discard;
	}

	// 3. Reconstruct World-Space Surface Position from Reversed-Z depth
	float2 ndcXY = screenUV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
	float4 worldPosH = mul(float4(ndcXY, depthZ, 1.0f), g_View.invViewProjMatrix);
	float3 worldPos = worldPosH.xyz / max(abs(worldPosH.w), 0.000001f);

	// 4. Transform worldPos into decal local OBB space [-0.5, 0.5]^3
	// CBPerObject::invWorldMatrix stores decal inverse world matrix
	float4 localPosH = mul(float4(worldPos, 1.0f), g_Object.invWorldMatrix);
	float3 localPos = localPosH.xyz / max(abs(localPosH.w), 0.000001f);

	// 5. Clip pixels outside unit box [-0.5, 0.5]^3
	clip(0.5f - abs(localPos.xyz));

	// 6. Compute UV projection and smooth box-edge fade
	float2 uv = localPos.xz + 0.5f;
	uv = uv * g_Material.tilingOffset.xy + g_Material.tilingOffset.zw;

	float maxDistFromCenter = max(abs(localPos.x), max(abs(localPos.y), abs(localPos.z)));
	float edgeFade = saturate((0.5f - maxDistFromCenter) * 10.0f);

	// 7. Sample decal albedo modulated by baseColorFactor and modulated by edgeFade
	float4 decalTex = g_TextureAlbedo.Sample(g_SamplerLinearWrap, uv);
	float4 decalColor = decalTex * g_Material.baseColorFactor;
	decalColor.a *= edgeFade;

	if (decalColor.a <= 0.001f)
	{
		discard;
	}

	// Output RGBA with alpha-blending for Forward+ (m_hdrSceneRTV) or Deferred (m_gbufferAlbedoRTV)
	return float4(decalColor.rgb, decalColor.a);
}

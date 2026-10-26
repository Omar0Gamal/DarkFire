#include "Lighting.hlsli"

cbuffer CBWaterBuffer : register(b6)
{
	CBWater g_Water;
};

struct PSOutput
{
	float4 color    : SV_Target0;
	uint   entityId : SV_Target1;
};

PSOutput main(PSInput input)
{
	float2 screenUV = input.positionCS.xy * g_View.invViewportSize;

	// Sample Reversed-Z depth
	float sceneDepthZ = g_TextureDepth.SampleLevel(g_SamplerPointClamp, screenUV, 0).r;

	// Reconstruct linear view space Z using inverse projection matrix
	float4 seabedViewPt = mul(float4(0.0f, 0.0f, sceneDepthZ, 1.0f), g_View.invProjMatrix);
	float seabedLinearZ = seabedViewPt.z / max(abs(seabedViewPt.w), 0.00001f);

	float4 waterViewPt = mul(float4(0.0f, 0.0f, input.positionCS.z, 1.0f), g_View.invProjMatrix);
	float waterLinearZ = waterViewPt.z / max(abs(waterViewPt.w), 0.00001f);

	// Discard if occluded by foreground geometry
	if (waterLinearZ > seabedLinearZ + 0.02f)
	{
		discard;
	}

	float depthDelta = max(seabedLinearZ - waterLinearZ, 0.0f);

	// Dual counter-scrolling procedural wave ripples for micro-surface perturbation
	float time = g_Frame.time;
	float2 uvRipple1 = input.positionWS.xz * 0.45f + float2(0.06f, 0.03f) * time;
	float2 uvRipple2 = input.positionWS.xz * 0.75f - float2(0.04f, 0.07f) * time;

	float ripple1 = sin(uvRipple1.x * 6.28f + sin(uvRipple1.y * 3.14f));
	float ripple2 = cos(uvRipple2.x * 5.12f - cos(uvRipple2.y * 4.28f));

	float3 rippleNormal;
	rippleNormal.x = (ripple1 - ripple2) * 0.08f;
	rippleNormal.z = (ripple1 + ripple2) * 0.08f;
	rippleNormal.y = 1.0f;
	rippleNormal = normalize(rippleNormal);

	float3 N = normalize(input.normalWS + rippleNormal * 0.35f);
	float3 V = normalize(g_View.cameraPosition - input.positionWS);

	// Underwater Refraction
	float refractionStrength = g_Water.opticalParams.z;
	float2 refractedUV = clamp(screenUV + N.xz * refractionStrength * saturate(depthDelta * 0.5f), 0.001f, 0.999f);

	float refDepthSample = g_TextureDepth.SampleLevel(g_SamplerPointClamp, refractedUV, 0).r;
	float4 refViewPt = mul(float4(0.0f, 0.0f, refDepthSample, 1.0f), g_View.invProjMatrix);
	float refLinearZ = refViewPt.z / max(abs(refViewPt.w), 0.00001f);
	if (refLinearZ < waterLinearZ)
	{
		refractedUV = screenUV;
	}

	// Sample underwater seabed from g_TextureCustom0 (copy of HDR scene color)
	float3 seabedColor = g_TextureCustom0.SampleLevel(g_SamplerLinearClamp, refractedUV, 0).rgb;

	// Animated dual-wave Sunlight Caustics pattern projected onto seabed
	float3 seabedPos = input.positionWS - V * depthDelta;
	float causticsIntensity = g_Water.opticalParams.w;
	float2 cUV1 = seabedPos.xz * 1.2f + float2(0.5f, 0.3f) * time;
	float2 cUV2 = seabedPos.xz * 1.6f - float2(0.35f, 0.45f) * time;
	float c1 = sin(cUV1.x + sin(cUV1.y * 1.2f));
	float c2 = cos(cUV2.x - cos(cUV2.y * 1.4f));
	float caustics = pow(max(0.0f, (c1 + c2) * 0.5f + 0.5f), 3.0f);
	float causticsAtten = causticsIntensity * exp(-depthDelta * 0.5f) * saturate(depthDelta * 2.0f);
	seabedColor += caustics * causticsAtten * float3(1.0f, 0.95f, 0.82f);

	// Beer-Lambert Optical Depth Absorption with Linear Colors
	float depthAbsorption = g_Water.opticalParams.x;
	float waterTransmittance = exp(-depthDelta * depthAbsorption);
	float3 shallowLinear = sRGBToLinear(g_Water.shallowColor.rgb);
	float3 deepLinear = sRGBToLinear(g_Water.deepColor.rgb);
	float3 foamLinear = sRGBToLinear(g_Water.foamColor.rgb);

	float3 waterBodyColor = lerp(shallowLinear, deepLinear, 1.0f - waterTransmittance);
	float3 absorbedSeabed = lerp(waterBodyColor, seabedColor, waterTransmittance);

	// Shoreline & Crest Surf Foam
	float foamThreshold = max(g_Water.opticalParams.y, 0.05f);
	float shorelineFactor = saturate(1.0f - depthDelta / foamThreshold);
	float waveHeight = input.positionWS.y - g_Water.waterExtra.x;
	float waveCrestFactor = smoothstep(g_Water.waveParams.x * 0.85f, g_Water.waveParams.x * 1.10f, waveHeight);

	float2 foamUV = input.positionWS.xz * 0.8f + float2(0.05f, 0.02f) * time;
	float foamNoise = (sin(foamUV.x * 4.0f) * cos(foamUV.y * 4.0f) + sin(foamUV.x * 9.0f + time)) * 0.5f + 0.5f;

	float totalFoam = saturate(shorelineFactor * 1.0f + waveCrestFactor * 0.75f) * foamNoise;
	float3 withFoam = lerp(absorbedSeabed, foamLinear, totalFoam);

	// Pre-Filtered Sky & Environment Reflection with Schlick Fresnel
	float NdotV = saturate(dot(N, V));
	float3 F0 = float3(0.02f, 0.02f, 0.02f);
	float3 fresnel = fresnelSchlick(NdotV, F0);

	float3 R = reflect(-V, N);
	float skyT = saturate(R.y * 0.5f + 0.5f);
	float3 skyReflection = lerp(g_Pass.skyHorizonColor.rgb, g_Pass.skyZenithColor.rgb, skyT);

	// Sample pre-filtered equirectangular sky if bound
	float2 eqUV = float2(atan2(R.z, R.x) * (0.5f * INV_PI) + 0.5f, 0.5f - asin(clamp(R.y, -1.0f, 1.0f)) * INV_PI);
	float3 skySample = g_TextureSkyEquirect.SampleLevel(g_SamplerLinearWrap, eqUV, 1.0f).rgb;
	if (any(skySample > 0.001f))
	{
		skyReflection = skySample * g_Pass.ambientLightColor.a;
	}

	float3 reflectionColor = skyReflection;

	// Physical GGX Specular Sun Highlight from Shared PBR BRDF
	float3 sunSpecular = float3(0.0f, 0.0f, 0.0f);
	if (g_Pass.lightCount > 0 && (int)g_Lights[0].positionType.w == 0)
	{
		float3 L = normalize(-g_Lights[0].directionRange.xyz);
		float3 H = normalize(V + L);
		float NdotL = max(dot(N, L), 0.0f);
		if (NdotL > 0.0f)
		{
			float waterRoughness = 0.08f;
			float D = DistributionGGX(N, H, waterRoughness);
			float G = GeometrySmith(N, V, L, waterRoughness);
			float3 F = fresnelSchlick(max(dot(H, V), 0.0f), F0);

			float3 specNum = D * G * F;
			float specDenom = 4.0f * max(NdotV, 0.001f) * max(NdotL, 0.001f);
			float3 specular = specNum / max(specDenom, 0.0001f);

			float shadowFactor = evaluateDirectionalShadow(input.positionWS, N, L, input.positionCS.xy);
			float3 radiance = g_Lights[0].colorIntensity.rgb * g_Lights[0].colorIntensity.a;
			sunSpecular = specular * radiance * NdotL * shadowFactor;
		}
	}

	float3 finalColor = lerp(withFoam, reflectionColor, fresnel.r) + sunSpecular;
	finalColor = applyAtmosphericFog(finalColor, input.positionWS, g_View.cameraPosition);

	PSOutput output;
	output.color = float4(finalColor, 1.0f);
	output.entityId = input.entityId;

	return output;
}

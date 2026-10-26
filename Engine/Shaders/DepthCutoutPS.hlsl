#include "Common.hlsli"

void main(PSInput input)
{
	float alpha = g_TextureAlbedo.Sample(g_SamplerAnisoWrap, input.texcoord).a * g_Material.baseColorFactor.a;
	clip((alpha - g_Material.alphaCutoff) / max(fwidth(alpha), 0.0001f));
}

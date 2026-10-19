struct VS_INPUT
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 binormal : BINORMAL0;
    float3 tangent : TANGENT;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 direction_to_camera : TEXCOORD1;
};


cbuffer constant : register(b0)
{
    row_major float4x4 m_world;
    row_major float4x4 m_view;
    row_major float4x4 m_proj;
    float4 m_camera_position;
};



VS_OUTPUT vsmain(VS_INPUT input)
{
    VS_OUTPUT output = (VS_OUTPUT) 0;

	//WORLD SPACE
    output.position = mul(input.position, m_world);
	//VIEW SPACE
    output.position = mul(output.position, m_view);
	//SCREEN SPACE
    output.position = mul(output.position, m_proj);


    output.texcoord = input.texcoord;

    return output;
}


Texture2D Color : register(t0);
sampler ColorSampler : register(s0);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 direction_to_camera : TEXCOORD1;
};

float4 psmain(PS_INPUT input) : SV_TARGET
{
    float4 color = Color.Sample(ColorSampler, input.texcoord);
    return float4(color.rgb, 1.0);
}
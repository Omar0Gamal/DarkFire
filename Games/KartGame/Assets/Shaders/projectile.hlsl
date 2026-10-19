struct VS_INPUT
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 binormal : BINORMAL0;
    float3 tangent : TANGENT;
};

struct light
{
    float4 m_lightColor;
    float4 m_lightDirection;
    float4 m_lightPosition;
};


cbuffer constant : register(b0)
{
    row_major float4x4 m_world;
    row_major float4x4 m_view;
    row_major float4x4 m_proj;
    float4 m_camera_position;
    light m_lights[32];
    int m_numLights;
};


float3 computePhongDirLighting(float ka, float3 ia, float kd, float3 id, float ks, float3 is, float shininess,
	float3 color, float3 normal, float3 lightDir, float3 dirToDCamera)
{
	//AMBIENT LIGHT
    ia *= (color.rgb);
    float3 ambient_light = ka * ia;

	//DIFFUSE LIGHT
    float amount_diffuse_light = dot(lightDir, normal);
    id *= (color.rgb);
    float3 diffuse_light = kd * id * amount_diffuse_light;

	//SPECULAR LIGHT
    float3 reflected_light = reflect(lightDir, normal);
    float amount_specular_light = pow(max(0.0, dot(reflected_light, dirToDCamera)), shininess);

    float3 specular_light = ks * amount_specular_light * is;

    float3 final_light = ambient_light + diffuse_light + specular_light;

    return final_light;
}


float3 processLighting(float3 color, float3 normal, float3 dirToDCamera)
{

    float3 final_light = float3(0, 0, 0);

    for (int i = 0; i < m_numLights; i++)
    {
		//AMBIENT LIGHT
        float ka = 4.5;
        float3 ia = float3(0.09, 0.09, 0.09);

        float kd = 1.0;
        float3 id = m_lights[i].m_lightColor.rgb;

        float ks = 0.0;
        float3 is = float3(1.0, 1.0, 1.0);

        float shininess = 30.0;


        final_light += computePhongDirLighting(ka, ia, kd, id, ks, is, shininess, color, normal, m_lights[i].m_lightDirection.xyz, dirToDCamera);
    }

    return final_light;
}

//--------------------------------------------------------------------
//--------------------------------------------------------------------




struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 direction_to_camera : TEXCOORD1;
};


VS_OUTPUT vsmain(VS_INPUT input)
{
    VS_OUTPUT output = (VS_OUTPUT) 0;

    output.position = mul(input.position, m_world);
    output.direction_to_camera = normalize(output.position.xyz - m_camera_position.xyz);
    output.position = mul(output.position, m_view);
    output.position = mul(output.position, m_proj);

    output.texcoord = input.texcoord;
    output.normal = normalize(mul(input.normal, m_world));
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
    return float4(1, 0, 0, 1);
}
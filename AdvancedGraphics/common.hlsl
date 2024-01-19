//Stores Common Data shared amoungst multiple shaders

struct material_data
{
    float4 diffuse_albedo;
    float3 fresnel;
    float roughness;
    float4x4 material_transformation;
    uint diffuse_map_index;
    uint normal_map_index;
    uint material_padding_1;
    uint material_padding_2;
};

//Stores array of textures
Texture2D texture_map[2] : register(t1);


cbuffer ObjectConstantBuffer : register(b0)
{
    float4x4 wvpMat;
};

cbuffer DefaultConstantBuffer : register(b1)
{
    float4x4 mat;
};

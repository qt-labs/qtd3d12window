cbuffer ConstantBuffer : register(b0)
{
        float4x4 modelview;
        float4x4 projection;
};

struct PSInput
{
        float4 position : SV_POSITION;
        float4 color : COLOR;
};

PSInput VS_Offscreen(float4 position : POSITION, float4 color : COLOR)
{
        PSInput result;

        float4x4 mvp = mul(projection, modelview);
        result.position = mul(mvp, position);
        result.color = color;

        return result;
}

float4 PS_Offscreen(PSInput input) : SV_TARGET
{
        return input.color;
}

struct PSInput2
{
        float4 position : SV_POSITION;
        float2 coord : TEXCOORD0;
};

PSInput2 VS_Onscreen(float4 position : POSITION, float2 coord : TEXCOORD0)
{
        PSInput2 result;

        float4x4 mvp = mul(projection, modelview);
        result.position = mul(mvp, position);
        result.coord = coord;

        return result;
}

Texture2D tex : register(t0);
SamplerState samp : register(s0);

float4 PS_Onscreen(PSInput2 input) : SV_TARGET
{
        return tex.Sample(samp, input.coord);
}

Texture2D<float> gTex;
SamplerState gSampler;

cbuffer ErosionPayload
{
    int2 Offset0;
    int2 Offset1;
    uint Beta;
    uint MaxDistance;
};

float main(float2 texC : TEXCOORD) : SV_TARGET0
{
    uint FragColor = 0;
    uint3 dims;
    gTex.GetDimensions(0, dims.x, dims.y, dims.z);
    int3 crd = int3 ( texC * dims.xy, 0);
    uint color = gTex.Load(crd) * 255;

    int3 crd0 = clamp(crd + int3(Offset0, 0), 0, (int3)dims.xyy - 1);
    int3 crd1 = clamp(crd + int3(Offset1, 0), 0, (int3)dims.xyy - 1);

    uint candidate0 = gTex.Load(crd0).r * 255;
    uint candidate1 = gTex.Load(crd1).r * 255;

    FragColor = min(min(color, candidate0 + Beta), candidate1 + Beta);

    if ( color == FragColor || FragColor > MaxDistance )
        discard;

    return (float)FragColor / 255;
}

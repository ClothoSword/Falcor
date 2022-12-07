Texture2D<float> gTex;
SamplerState gSampler;

float main(float2 texC : TEXCOORD) : SV_TARGET0
{
    uint FragColor = 0;
    uint3 dims;
    gTex.GetDimensions(0, dims.x, dims.y, dims.z);
    int3 crd = int3 ( texC * dims.xy, 0);
    uint color = gTex.Load(crd) * 255;

    if(color == 0)
        discard;

    return (float)color / 255.0;
}

Texture2D<float> gTex;
SamplerState gSampler;

float main(float2 texC : TEXCOORD) : SV_TARGET0
{
    uint FragColor = 0;
    uint3 dims;
    gTex.GetDimensions(0, dims.x, dims.y, dims.z);
    int3 crd = int3 ( texC * dims.xy, 0);
    uint color = gTex.Load(crd) * 255;

    if ( color != 255u )
        discard;

    int3 ncrd = clamp(crd + int3(0, -1, 0), 0, (int3)dims.xyy - 1);
    int3 scrd = clamp(crd + int3(0, +1, 0), 0, (int3)dims.xyy - 1);
    int3 ecrd = clamp(crd + int3(+1, 0, 0), 0, (int3)dims.xyy - 1);
    int3 wcrd = clamp(crd + int3(-1, 0, 0), 0, (int3)dims.xyy - 1);

    uint n = gTex.Load(ncrd).r * 255;
    uint s = gTex.Load(scrd).r * 255;
    uint e = gTex.Load(ecrd).r * 255;
    uint w = gTex.Load(wcrd).r * 255;

    FragColor = min(n, min(s, min(e, w)));
    if ( FragColor < 254u )
        FragColor++;
    return (float)FragColor / 255;
}

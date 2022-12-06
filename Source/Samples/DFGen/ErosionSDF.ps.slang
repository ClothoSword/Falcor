Texture2D<float> gTexPos;
Texture2D<float> gTexNeg;
SamplerState gSampler;

float main(float2 texC : TEXCOORD) : SV_TARGET0
{
    float Pos = sqrt(gTexPos.Sample(gSampler, texC));
    float Neg = sqrt(gTexNeg.Sample(gSampler, texC));
    float L = 0.5 + (Pos - Neg);
    return L;
}

struct PixelInput
{
	float4 svPosition: SV_POSITION;
    float4 world_position: POSITION;
	float4 normal: NORMAL;
};

cbuffer ColorBuffer : register(b2)
{
    float4 color: COLOR;
};

float4 main(PixelInput input) : SV_TARGET
{
	return color;
}
struct VertexInput
{
	float4 position: POSITION;
	float4 normal: NORMAL;
};

struct VertexOutput
{
	float4 svPosition: SV_POSITION;
	float4 world_position: POSITION;
	float4 normal: NORMAL;
};

cbuffer PVBuffer : register(b0)
{
	matrix projection;
	matrix view;
};

cbuffer ModelBuffer : register(b1)
{
	matrix model;
};

VertexOutput main(VertexInput input)
{
	VertexOutput result;

	result.svPosition = mul(projection, mul(view, mul(model, input.position)));
	result.world_position = mul(model, input.position);
	// TODO: do correctly?
	result.normal = normalize(mul(model, input.normal));

	return result;
}
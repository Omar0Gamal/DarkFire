struct PSLineInput
{
	float4 positionCS : SV_POSITION;
	float4 color      : COLOR;
};

float4 main(PSLineInput input) : SV_TARGET
{
	return input.color;
}

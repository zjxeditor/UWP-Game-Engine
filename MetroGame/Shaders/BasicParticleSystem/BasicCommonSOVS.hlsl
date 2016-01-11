struct Particle
{
	float3 InitialPosW	: POSITION;
	float3 InitialVelW	: VELOCITY;
	float2 SizeW		: SIZE;
	float  Age			: AGE;
	uint   Type			: TYPE;
};

Particle main(Particle vin)
{
	return vin;
}
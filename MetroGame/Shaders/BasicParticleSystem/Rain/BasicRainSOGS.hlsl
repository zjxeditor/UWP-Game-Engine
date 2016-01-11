#include "../BasicCommonSOGS.hlsl"

// The stream-out GS is just responsible for emitting 
// new particles and destroying old particles.  The logic
// programed here will generally vary from particle system
// to particle system, as the destroy/spawn rules will be 
// different.
[maxvertexcount(21)]
void main(point Particle gin[1],
	inout PointStream<Particle> ptStream)
{
	gin[0].Age += gElapseTime;

	if (gin[0].Type == PT_EMITTER)
	{
		// time to emit a new particle?
		if (gin[0].Age > 0.002f)
		{
			for (int i = 0; i < 20; ++i)
			{
				// Spread rain drops out above the camera.
				float3 vRandom = 50.0f*RandVec3((float)i / 5.0f);
				vRandom.y = 40.0f;

				Particle p;
				p.InitialPosW = gEmitPosW.xyz + vRandom;
				p.InitialVelW = float3(0.0f, 0.0f, 0.0f);
				p.SizeW = float2(1.0f, 1.0f);
				p.Age = 0.0f;
				p.Type = PT_FLARE;

				ptStream.Append(p);
			}

			// reset the time to emit
			gin[0].Age = 0.0f;
		}

		// always keep emitters
		ptStream.Append(gin[0]);
	}
	else
	{
		// Specify conditions to keep particle; this may vary from system to system.
		if (gin[0].Age <= 5.0f)
			ptStream.Append(gin[0]);
	}
}
#pragma once
#include "maths.h"

namespace random
{
	float uniform(float low = 0.0f, float high = 1.0f);
	int uniform_int(int low = 0, int high = 2);
	// Azimuth: 0 at +x axis in right handed system
	//			pi / 2 at +z axis in right handed system
	// Polar: 0 at the top
	Vector3 uniform_unit_hemisphere();
}
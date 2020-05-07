#include "random.h"

// TODO: other source of randomness than rand()
#include <cstdlib>
float random::uniform(float low, float high)
{
	float normalized = (rand() % 10000) / 10000.0f;
	float result = normalized * (high - low) + low;

	return result;
}

int random::uniform_int(int low, int high)
{
	int result = (rand() % (high - low)) + low;
	return result;
}

Vector3 random::uniform_unit_sphere() {
	float azimuth = random::uniform(0, math::PI2);
	float polar = math::acos(2 * random::uniform() - 1);
	float r = math::pow(random::uniform(), 1.0f / 3.0f);
	
	Vector3 result;

	result.x = r * math::cos(azimuth) * math::sin(polar);
	result.y = r * math::cos(polar);
	result.z = r * math::sin(azimuth) * math::sin(polar);

	return result;
}

Vector3 random::uniform_unit_hemisphere()
{
	float y = random::uniform(0.0f, 1.0f);
	float r = math::sqrt(1.0f - y * y);
	float phi = random::uniform(0.0f, math::PI2);

	float radius = random::uniform(0.0f, 1.0f);
	radius = math::sqrt(radius);
	
	Vector3 result;

	result.x = r * math::cos(phi) * radius;
	result.y = y * radius;
	result.z = r * math::sin(phi) * radius;

	return result;
}
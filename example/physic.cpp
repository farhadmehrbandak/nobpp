#include "physic.h"
#include "math.h"

float compute_force(float acceleration, float mass)
{
    return multiply(acceleration, mass);
}
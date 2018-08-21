#version 330

// Input: UV coordinates
in INTERFACE {
	vec2 uv;
} In ;

uniform mat4 camToWorld;
uniform mat4 clipToCam;
uniform vec3 viewPos;
uniform vec3 lightDirection;

const float groundRadius = 6371e3;
const float topRadius = 6471e3;
const float sunIntensity = 20.0;
const vec3 sunColor = vec3(1.474, 1.8504, 1.91198);
const vec3 kRayleigh = vec3(5.5e-6, 13.0e-6, 22.4e-6);
const float heightRayleigh = 8000.0;
const float heightMie = 1200.0;
const float kMie = 21e-6;
const float gMie = 0.758;

const float sunAngularRadius = 0.04675;
const float sunAngularRadiusCos = 0.998;

out vec3 fragColor;

#define SAMPLES_COUNT 16
#define SAMPLES_COUNT_INNER 8
#define M_PI 3.14159265358979323846


// Check if a sphere of a given radius is intersected by a ray defined by an origin wrt to the sphere center and a normalized direction.
bool intersects(vec3 rayOrigin, vec3 rayDir, float radius, out vec2 roots){
	float a = dot(rayDir,rayDir);
	float b = dot(rayOrigin, rayDir);
	float c = dot(rayOrigin, rayOrigin) - radius*radius;
	float delta = b*b - a*c;
	// No intersection if the polynome has no real roots.
	if(delta < 0.0){
		return false;
	}
	// If it intersects, return the two roots.
	float dsqrt = sqrt(delta);
	roots = (-b+vec2(-dsqrt,dsqrt))/a;
	return true;
}

float rayleighPhase(float cosAngle){
	const float k = 1.0/(4.0*M_PI);
	return k * 3.0/4.0 * (1.0 + cosAngle*cosAngle);
}

float miePhase(float cosAngle){
	const float k = 1.0/(4.0*M_PI);
	float g2 = gMie*gMie;
	return k * 3.0 * (1.0-g2) / (2.0 * (2.0 + g2)) * (1.0 + cosAngle*cosAngle) / pow(1 + g2 - 2.0 * gMie * cosAngle, 3.0/2.0);
}

vec3 computeRadiance(vec3 rayOrigin, vec3 rayDir, vec3 sunDir){
	// Check intersection with atmosphere.
	vec2 interTop, interGround;
	bool didHitTop = intersects(rayOrigin, rayDir, topRadius, interTop);
	// If no intersection with the atmosphere, it's the dark void of space.
	if(!didHitTop){
		return vec3(0.0);
	}
	// Now intersect with the planet.
	bool didHitGround = intersects(rayOrigin, rayDir, groundRadius, interGround);
	// Distance to the closest intersection.
	float distanceToInter = min(interTop.y, didHitGround ? interGround.x : 0.0);
	// Divide the distance traveled through the atmosphere in SAMPLES_COUNT parts.
	float stepSize = (distanceToInter - interTop.x)/SAMPLES_COUNT;
	// Angle between the sun direction and the ray.
	float cosViewSun = dot(rayDir, sunDir);
	
	// Accumulate optical distance for both scatterings.
	float rayleighDist = 0.0;
	float mieDist = 0.0;
	// Accumulate contributions for both scatterings.
	vec3 rayleighScatt = vec3(0.0);
	vec3 mieScatt = vec3(0.0);
	
	// March along the ray.
	for(int i = 0; i < SAMPLES_COUNT; ++i){
		// Compute the current position along the ray, ...
		vec3 currPos = rayOrigin + (i+0.5) * stepSize * rayDir;
		// ...and its distance to the ground (as we are in planet space).
		float currHeight = length(currPos) - groundRadius;
		// Compute density based on the characteristic height of Rayleigh and Mie.
		float rayleighStep = exp(-currHeight/heightRayleigh) * stepSize;
		float mieStep = exp(-currHeight/heightMie) * stepSize;
		// Accumulate optical distances.
		rayleighDist += rayleighStep;
		mieDist += mieStep;
		
		// We then march along the secondary ray going towards the sun.
		// Check when the ray leaves the atmosphere.
		vec2 interSecondTop;
		bool didHitSecondTop = intersects(currPos, sunDir, topRadius, interSecondTop);
		// Divide the distance traveled through the atmosphere in SAMPLES_COUNT_INNER parts.
		float secondStepSize = didHitSecondTop ? interSecondTop.y/SAMPLES_COUNT_INNER : 0.0;
		// Accumulate optical distance for both scatterings.
		float rayleighSecondDist = 0.0;
		float mieSecondDist = 0.0;
		
		// March along the secondary ray.
		for(int j = 0; j < SAMPLES_COUNT_INNER; ++j){
			// Compute the current position along the ray, ...
			vec3 currSecondPos = currPos + (j+0.5) * secondStepSize * sunDir;
			// ...and its distance to the ground (as we are in planet space).
			float currSecondHeight = length(currSecondPos) - groundRadius;
			// Compute density based on the characteristic height of Rayleigh and Mie.
			float rayleighSecondStep = exp(-currSecondHeight/heightRayleigh) * secondStepSize;
			float mieSecondStep = exp(-currSecondHeight/heightMie) * secondStepSize;
			// Accumulate optical distances.
			rayleighSecondDist += rayleighSecondStep;
			mieSecondDist += mieSecondStep;
		}
		
		// Compute associated attenuation.
		vec3 directAttenuation = exp(-(kMie * (mieDist) + kRayleigh * (rayleighDist)));
		vec3 secondaryAttenuation = exp(-(kMie * (mieSecondDist) + kRayleigh * (rayleighSecondDist)));
		vec3 attenuation = directAttenuation * secondaryAttenuation;
		// Accumulate scatterings.
		rayleighScatt += rayleighStep * attenuation;
		mieScatt += mieStep * attenuation;
	}
	// Final scattering participations.
	vec3 rayleighParticipation = kRayleigh * rayleighPhase(cosViewSun) * rayleighScatt;
	vec3 mieParticipation = kMie * miePhase(cosViewSun) * mieScatt;
	return sunIntensity * (rayleighParticipation + mieParticipation);
}

void main(){
	// Move to -1,1
	vec4 clipVertex = vec4(-1.0+2.0*In.uv, 0.0, 1.0);
	// Then to world space.
	vec3 viewRay = normalize((camToWorld*vec4((clipToCam*clipVertex).xyz, 0.0)).xyz);
	// We then move to the planet model space, where its center is in (0,0,0).
	vec3 planetSpaceViewPos = viewPos + vec3(0,6372e3,0);
	vec3 atmosphereColor = computeRadiance(planetSpaceViewPos, viewRay, lightDirection);
	fragColor = atmosphereColor;
}


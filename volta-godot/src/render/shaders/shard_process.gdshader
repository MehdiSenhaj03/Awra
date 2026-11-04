// Obsidian chips and glowing splinters: tumble, bounce on the glass, cool down.
// Emitted CUSTOM = (size, 0, life seconds, lit 0/1); CUSTOM.y becomes the age.
shader_type particles;

uniform float gravity = 26.0;

float hash(float n) { return fract(sin(n * 12.9898) * 43758.5453); }

void start() {
	CUSTOM.y = 0.0;
}

mat3 axis_rot(vec3 a, float t) {
	float c = cos(t);
	float s = sin(t);
	float ic = 1.0 - c;
	return mat3(
		vec3(c + a.x * a.x * ic, a.y * a.x * ic + a.z * s, a.z * a.x * ic - a.y * s),
		vec3(a.x * a.y * ic - a.z * s, c + a.y * a.y * ic, a.z * a.y * ic + a.x * s),
		vec3(a.x * a.z * ic + a.y * s, a.y * a.z * ic - a.x * s, c + a.z * a.z * ic));
}

void process() {
	CUSTOM.y += DELTA;
	if (CUSTOM.y >= CUSTOM.z) {
		ACTIVE = false;
	}
	VELOCITY.y -= gravity * DELTA;
	float half_size = CUSTOM.x * 0.5;
	if (TRANSFORM[3].y < half_size && VELOCITY.y < 0.0) {
		TRANSFORM[3].y = half_size;
		VELOCITY.y *= -0.35;
		VELOCITY.xz *= 0.7;
	}
	float k = min(1.0, (CUSTOM.z - CUSTOM.y) * 1.5);
	vec3 axis = normalize(vec3(hash(float(INDEX)) - 0.5, hash(float(INDEX) + 3.1) - 0.5, hash(float(INDEX) + 7.7) - 0.5) + 0.001);
	float spin = (hash(float(INDEX) + 1.7) * 2.0 - 1.0) * 14.0;
	mat3 r = axis_rot(axis, CUSTOM.y * spin) * (CUSTOM.x * max(k, 0.0));
	TRANSFORM[0].xyz = r[0];
	TRANSFORM[1].xyz = r[1];
	TRANSFORM[2].xyz = r[2];
}

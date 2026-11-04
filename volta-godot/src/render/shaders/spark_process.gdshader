// Sparks simulated on the GPU. Emitted CUSTOM = (size, 0, life, drag10 * 1000 + gravity + 100);
// CUSTOM.y counts the age.
shader_type particles;

void start() {
	CUSTOM.y = 0.0;
}

void process() {
	CUSTOM.y += DELTA;
	if (CUSTOM.y >= CUSTOM.z) {
		ACTIVE = false;
	}
	float drag = floor(CUSTOM.w / 1000.0) * 0.1;
	float grav = mod(CUSTOM.w, 1000.0) - 100.0;
	float dr = exp(-drag * DELTA);
	VELOCITY.xz *= dr;
	VELOCITY.y = VELOCITY.y * dr - grav * DELTA;
	if (TRANSFORM[3].y < 0.05) {
		TRANSFORM[3].y = 0.05;
		VELOCITY.y = max(VELOCITY.y, 0.0);
	}
	float k = 1.0 - CUSTOM.y / max(CUSTOM.z, 0.001);
	float s = CUSTOM.x * (0.4 + 0.6 * k);
	TRANSFORM[0].xyz = vec3(s, 0.0, 0.0);
	TRANSFORM[1].xyz = vec3(0.0, s, 0.0);
	TRANSFORM[2].xyz = vec3(0.0, 0.0, s);
}

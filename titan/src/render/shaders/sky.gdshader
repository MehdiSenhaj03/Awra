// A sky of dust: pale sun in an orange haze, ochre horizon, the blue-grey left above it; an ash storm swallows it
// in brown-black (dust), lightning flashes it.
shader_type sky;
uniform vec3 zenith : source_color = vec3(0.42, 0.47, 0.52);
uniform vec3 horizon : source_color = vec3(0.92, 0.66, 0.42);
uniform vec3 storm : source_color = vec3(0.16, 0.12, 0.10);
uniform float dust = 0.0;
uniform float flash = 0.0;
void sky() {
	vec3 d = EYEDIR;
	float h = clamp(d.y, -0.2, 1.0);
	vec3 col = mix(horizon, zenith, smoothstep(-0.02, 0.45, h));
	float s = max(dot(d, LIGHT0_DIRECTION), 0.0);
	col += vec3(1.0, 0.85, 0.6) * (pow(s, 900.0) * 3.0 + pow(s, 16.0) * 0.25 + pow(s, 4.0) * 0.06);
	// Bands of high dust.
	float band = sin(d.y * 40.0 + d.x * 6.0) * 0.5 + 0.5;
	col = mix(col, horizon * 0.9, band * 0.05 * smoothstep(0.4, 0.05, h));
	col = mix(col, storm, clamp(dust, 0.0, 0.95));
	if (d.y < 0.0) col = mix(col, horizon * 0.6, smoothstep(0.0, -0.1, d.y));
	col += vec3(0.85, 0.8, 1.0) * flash;
	COLOR = col;
}

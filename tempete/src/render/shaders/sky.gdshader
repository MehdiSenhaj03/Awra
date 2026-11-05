// The sky of a storm day: low golden sun in the west, blue-grey zenith, and a green-black darkness that grows with
// the storms (toward their bearing). Lightning flashes it.
shader_type sky;
uniform vec3 zenith : source_color = vec3(0.22, 0.32, 0.45);
uniform vec3 horizon : source_color = vec3(0.85, 0.72, 0.52);
uniform vec3 storm : source_color = vec3(0.06, 0.10, 0.10);
uniform vec3 storm_dir = vec3(-1.0, 0.0, 0.0);
uniform float darkness = 0.4;
uniform float flash = 0.0;
void sky() {
	vec3 d = EYEDIR;
	float h = clamp(d.y, -0.2, 1.0);
	vec3 col = mix(horizon, zenith, smoothstep(0.0, 0.5, h));
	// Sun.
	float s = max(dot(d, LIGHT0_DIRECTION), 0.0);
	col += LIGHT0_COLOR * (pow(s, 600.0) * 4.0 + pow(s, 8.0) * 0.25);
	// The storm side of the sky.
	float toward = clamp(dot(normalize(vec3(d.x, 0.0, d.z)), normalize(storm_dir)) * 0.5 + 0.5, 0.0, 1.0);
	float k = darkness * smoothstep(0.2, 1.0, toward) * smoothstep(-0.1, 0.25, h);
	col = mix(col, storm, clamp(k + darkness * 0.35, 0.0, 0.95));
	if (d.y < 0.0) col = mix(col, vec3(0.08, 0.08, 0.07), smoothstep(0.0, -0.1, d.y));
	col += vec3(0.7, 0.75, 1.0) * flash;
	COLOR = col;
}

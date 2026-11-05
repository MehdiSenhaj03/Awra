// Storm clouds (tower, anvil, mothership base, wall cloud): lumpy displaced surfaces, lit from the low sun on top
// and slate-dark beneath, soft at the silhouette, with rotating striations on the base when `bands` > 0.
shader_type spatial;
render_mode blend_mix, depth_draw_opaque, cull_back, diffuse_lambert, specular_disabled;

uniform vec3 lit : source_color = vec3(0.92, 0.88, 0.80);
uniform vec3 shade : source_color = vec3(0.10, 0.12, 0.15);
uniform vec3 tint : source_color = vec3(0.30, 0.42, 0.40);
uniform float alpha = 1.0;
uniform float bands = 0.0;
uniform float spin = 0.0;
uniform float bumps = 0.12;
uniform float scale = 1.0;
uniform float t = 0.0;
varying float vy;
varying vec3 lp;

float hash(vec3 p) { return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453); }
float noise(vec3 p) {
	vec3 i = floor(p);
	vec3 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	float n000 = hash(i), n100 = hash(i + vec3(1, 0, 0)), n010 = hash(i + vec3(0, 1, 0)), n110 = hash(i + vec3(1, 1, 0));
	float n001 = hash(i + vec3(0, 0, 1)), n101 = hash(i + vec3(1, 0, 1)), n011 = hash(i + vec3(0, 1, 1)), n111 = hash(i + vec3(1, 1, 1));
	return mix(mix(mix(n000, n100, f.x), mix(n010, n110, f.x), f.y), mix(mix(n001, n101, f.x), mix(n011, n111, f.x), f.y), f.z);
}
float fbm(vec3 p) {
	float v = 0.0;
	float a = 0.5;
	for (int i = 0; i < 4; i++) { v += a * noise(p); p *= 2.1; a *= 0.5; }
	return v;
}

void vertex() {
	lp = VERTEX;
	float n = fbm(VERTEX * 3.0 * scale + vec3(0.0, t * 0.02, 0.0));
	VERTEX += NORMAL * (n - 0.5) * bumps;
	vy = NORMAL.y;
}

void fragment() {
	float n = fbm(lp * 6.0 * scale + vec3(t * 0.03, 0.0, 0.0));
	float up = clamp(vy * 0.5 + 0.5, 0.0, 1.0);
	vec3 col = mix(shade, lit, smoothstep(0.25, 0.95, up) * (0.6 + 0.4 * n));
	col = mix(col, tint * 0.7, (1.0 - up) * 0.35);
	if (bands > 0.0) {
		// Striations: rings around the axis, slowly turning.
		float r = length(lp.xz);
		float a = atan(lp.z, lp.x) + spin;
		float s = sin(r * 18.0 + a * 0.5 + n * 3.0);
		col *= 0.75 + 0.25 * smoothstep(-0.2, 0.8, s) * bands;
	}
	float rim = pow(1.0 - abs(dot(NORMAL, VIEW)), 1.5);
	// Ragged, soft outer edge (radius in the mesh's own units).
	float edge = 1.0 - smoothstep(0.78, 1.0, length(lp.xz) + (n - 0.5) * 0.25);
	ALBEDO = col;
	ALPHA = alpha * (1.0 - rim * 0.85) * smoothstep(0.1, 0.4, n + 0.2) * mix(1.0, edge, bands > 0.0 ? 1.0 : 0.6);
	EMISSION = col * 0.08;
}

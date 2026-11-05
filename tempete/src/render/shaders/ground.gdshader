// The plains: a patchwork of fields (kind per 250 m patch from a data texture: wheat, corn, pasture, plowed,
// trees) with crop rows in a direction of their own, macro variation, and the darkening of wet soil.
shader_type spatial;
render_mode cull_back;

uniform sampler2D fields : filter_nearest;
uniform sampler2D wet : filter_linear;
uniform float size = 12000.0;
uniform float n = 48.0;
varying vec3 wpos;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
float fbm(vec2 p) {
	float v = 0.0;
	float a = 0.5;
	for (int i = 0; i < 4; i++) { v += a * noise(p); p *= 2.07; a *= 0.5; }
	return v;
}

void vertex() {
	wpos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

void fragment() {
	// World to map: x east, z = -north.
	vec2 m = vec2(wpos.x, -wpos.z);
	vec2 cell = floor(m / (size / n));
	vec2 f = fract(m / (size / n));
	float kind = texelFetch(fields, ivec2(clamp(cell, vec2(0.0), vec2(n - 1.0))), 0).r * 255.0;
	float h = hash(cell);
	float rowdir = step(0.5, h);
	float along = mix(m.x, m.y, rowdir);
	// Crop rows, faded out with distance (no moiré).
	float fw = fwidth(along * 3.1);
	float rows = 0.5 + 0.5 * sin(along * 3.1) * (1.0 - smoothstep(0.5, 2.0, fw));
	vec3 col;
	float rough = 0.9;
	if (kind < 0.5) {
		col = mix(vec3(0.62, 0.50, 0.24), vec3(0.75, 0.62, 0.30), fbm(m * 0.02) * 0.8 + h * 0.2);
		col *= 0.88 + 0.12 * rows;
	} else if (kind < 1.5) {
		col = mix(vec3(0.20, 0.33, 0.12), vec3(0.30, 0.42, 0.16), fbm(m * 0.03));
		col *= 0.75 + 0.25 * rows;
	} else if (kind < 2.5) {
		col = mix(vec3(0.30, 0.40, 0.18), vec3(0.42, 0.48, 0.22), fbm(m * 0.015 + h));
	} else if (kind < 3.5) {
		col = mix(vec3(0.30, 0.22, 0.15), vec3(0.40, 0.30, 0.20), fbm(m * 0.04));
		col *= 0.8 + 0.2 * rows;
	} else {
		col = mix(vec3(0.12, 0.20, 0.09), vec3(0.18, 0.27, 0.12), fbm(m * 0.08));
	}
	// Patch edges: a thin strip of grass and a darker line.
	float edge = min(min(f.x, 1.0 - f.x), min(f.y, 1.0 - f.y));
	col = mix(vec3(0.25, 0.30, 0.15), col, smoothstep(0.0, 0.02, edge));
	// Macro variation so the patchwork does not look printed.
	col *= 0.85 + 0.3 * fbm(m * 0.0015);
	float w = texture(wet, (m / size)).r;
	col *= 1.0 - 0.45 * w;
	rough = mix(0.9, 0.35, w);
	ALBEDO = col;
	ROUGHNESS = rough;
	SPECULAR = 0.3 + 0.4 * w;
}

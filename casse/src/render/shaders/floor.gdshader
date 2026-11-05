// Vault floor: black marble with pale veins and fine brass seams, glass tiles, the glowing exit,
// darkness where the lights are off. One plane for the whole level; tile types come from a data texture.
shader_type spatial;

uniform sampler2D u_tiles : filter_nearest;
uniform sampler2D u_dark : filter_linear;
uniform vec2 u_size = vec2(16.0, 10.0);
uniform float u_time = 0.0;
uniform vec3 u_accent : source_color = vec3(0.18, 0.95, 1.0);

varying vec3 v_w;

void vertex() {
	v_w = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p) {
	vec2 i = floor(p), f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
float fbm(vec2 p) {
	float s = 0.0, a = 0.5;
	for (int i = 0; i < 5; i++) { s += a * noise(p); p = p * 2.03 + 1.7; a *= 0.5; }
	return s;
}

void fragment() {
	vec2 p = v_w.xz;
	vec2 tc = floor(p);
	vec2 f = fract(p);
	float ty = floor(texture(u_tiles, (tc + 0.5) / u_size).r * 255.0 / 40.0 + 0.5);
	if (ty > 3.5) discard;
	float dark = texture(u_dark, p / u_size).r;
	float cell = hash(tc);
	vec3 col;
	float rough = 0.18;
	vec3 em = vec3(0.0);
	float sd = min(min(f.x, 1.0 - f.x), min(f.y, 1.0 - f.y));
	float seam = 1.0 - smoothstep(0.0, 0.025, sd);
	if (ty > 1.5 && ty < 2.5) {
		// Glass: dark teal, fine grid, a sheen.
		col = vec3(0.01, 0.03, 0.035);
		float g = 1.0 - smoothstep(0.0, 0.02, min(abs(fract(p.x * 4.0) - 0.5), abs(fract(p.y * 4.0) - 0.5)) - 0.47);
		em = vec3(0.05, 0.35, 0.4) * (0.15 + 0.85 * g) * 0.25 + vec3(0.1, 0.6, 0.7) * seam * 0.4;
		rough = 0.04;
	} else {
		// Black marble with veins; tiles alternate in tone; brass seams.
		float n = fbm(p * 0.55 + cell * 7.0);
		float vein = smoothstep(0.035, 0.0, abs(n - 0.52)) + 0.5 * smoothstep(0.015, 0.0, abs(fbm(p * 1.3 + 3.0) - 0.5));
		float checker = mod(tc.x + tc.y, 2.0);
		col = mix(vec3(0.016, 0.017, 0.024), vec3(0.03, 0.03, 0.04), checker * 0.6 + cell * 0.3);
		col += vec3(0.16, 0.16, 0.18) * vein * 0.35;
		col = mix(col, vec3(0.45, 0.33, 0.14), seam * 0.7);
		rough = 0.1 + vein * 0.25 + seam * 0.3;
		if (ty > 2.5) {
			// Exit: chevrons flowing outwards.
			float ch = fract(p.x * 0.0 + p.y * 0.0 + u_time * 0.8 - (abs(f.x - 0.5) + abs(f.y - 0.5)));
			em = vec3(0.2, 1.0, 0.55) * (0.25 + 0.75 * smoothstep(0.6, 1.0, ch)) * 0.9;
			col *= 0.4;
		}
	}
	float k = mix(1.0, 0.4, dark);
	ALBEDO = col * k * mix(vec3(1.0), vec3(0.6, 0.7, 1.2), dark);
	EMISSION = em * mix(1.0, 0.5, dark);
	ROUGHNESS = rough;
	METALLIC = 0.0;
	SPECULAR = 0.7 * k;
}

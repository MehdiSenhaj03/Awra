// A rain (or hail) shaft (an open tube, UV.y from the ground 0 to the cloud base 1): falling streaks, curtains,
// fading into the cloud at the top and thin at the silhouette.
shader_type spatial;
render_mode blend_mix, depth_draw_never, cull_disabled, unshaded;
uniform vec3 color : source_color = vec3(0.17, 0.2, 0.21);
uniform float density = 0.6;
uniform float t = 0.0;
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
void fragment() {
	vec2 uv = UV;
	float streak = noise(vec2(uv.x * 220.0, uv.y * 3.0 + t * 2.5));
	float curtain = noise(vec2(uv.x * 9.0 + t * 0.05, uv.y * 1.5));
	float a = density * (0.3 + 0.7 * curtain) * (0.6 + 0.4 * streak);
	// Soft at the top (into the cloud base) and at the silhouette, where the shaft is thin.
	a *= smoothstep(0.0, 0.35, 1.0 - uv.y) * smoothstep(0.0, 0.04, uv.y);
	float fres = abs(dot(NORMAL, VIEW));
	a *= smoothstep(0.05, 0.6, fres);
	ALBEDO = color * (0.85 + 0.15 * streak);
	ALPHA = clamp(a * 0.8, 0.0, 0.85);
}

// A tornado: a twisting condensation funnel (UV.x around, UV.y from ground 0 to cloud base 1), translucent at the
// edges, browner and dustier near the ground; `rope` thins and kinks it as it dies.
shader_type spatial;
render_mode blend_mix, depth_draw_never, cull_disabled;
uniform float t = 0.0;
uniform float dust = 0.6;
uniform float alpha = 0.9;
uniform vec3 cloud : source_color = vec3(0.32, 0.34, 0.36);
uniform vec3 dirt : source_color = vec3(0.36, 0.28, 0.20);
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
void fragment() {
	float spin = UV.x * 6.0 + t * 1.6 - UV.y * 4.0;
	float bands = noise(vec2(spin * 3.0, UV.y * 12.0 - t * 2.0));
	float n = noise(vec2(UV.x * 40.0 + t * 3.0, UV.y * 30.0));
	vec3 col = mix(dirt, cloud, smoothstep(0.0, 0.5, UV.y));
	col *= 0.7 + 0.45 * bands;
	float fres = abs(dot(NORMAL, VIEW));
	float a = alpha * smoothstep(0.0, 0.5, fres) * (0.65 + 0.35 * n);
	a *= mix(1.0, dust, 1.0 - smoothstep(0.0, 0.2, UV.y));
	ALBEDO = col;
	ALPHA = clamp(a, 0.0, 1.0);
	ROUGHNESS = 1.0;
}

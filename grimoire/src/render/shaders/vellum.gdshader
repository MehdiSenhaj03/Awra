// Over the painted arena: the grain and warmth of vellum, a darker margin like an illuminated page, and a soft
// light where the duel happens.
shader_type canvas_item;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}

void fragment() {
	vec2 uv = UV;
	float grain = noise(uv * vec2(900.0, 500.0)) * 0.5 + noise(uv * vec2(180.0, 110.0)) * 0.5;
	float fibers = noise(vec2(uv.x * 40.0, uv.y * 900.0)) * 0.5;
	vec2 d = (uv - vec2(0.5, 0.58)) * vec2(1.0, 1.25);
	float vig = smoothstep(0.35, 0.95, length(d));
	vec3 warm = vec3(0.30, 0.2, 0.08);
	float a = vig * 0.75 + (grain * 0.08 + fibers * 0.04);
	COLOR = vec4(mix(warm, vec3(0.02, 0.01, 0.0), vig), a);
}

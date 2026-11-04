// The abyss under the arena: the sector painting far below, a slow nebula and twinkling stars.
shader_type spatial;
render_mode unshaded, fog_disabled;

uniform sampler2D u_tex : source_color, filter_linear_mipmap;
uniform float u_has = 0.0;
uniform float u_time = 0.0;
uniform float u_level = 0.0;
uniform vec3 u_a : source_color = vec3(0.1, 0.4, 0.5);
uniform vec3 u_b : source_color = vec3(0.4, 0.1, 0.3);

float h(vec2 p) { return fract(sin(dot(p, vec2(41.3, 289.1))) * 43758.5); }
float n(vec2 p) { vec2 i = floor(p), f = fract(p); f = f * f * (3.0 - 2.0 * f);
	return mix(mix(h(i), h(i + vec2(1, 0)), f.x), mix(h(i + vec2(0, 1)), h(i + vec2(1, 1)), f.x), f.y); }
float fbm(vec2 p) { float s = 0.0, a = 0.5; for (int i = 0; i < 5; i++) { s += a * n(p); p *= 2.03; a *= 0.5; } return s; }

void fragment() {
	vec2 p = UV * vec2(3.0, 2.0);
	float t = u_time * 0.01;
	float neb = fbm(p * 1.5 + vec2(t, -t)) * fbm(p * 3.0 - vec2(t * 2.0, 0.0));
	vec3 col = mix(u_a, u_b, fbm(p * 0.8 + 3.0)) * pow(neb, 2.2) * 0.9;
	vec2 sp = UV * vec2(900.0, 600.0) * 0.5;
	float st = step(0.997, h(floor(sp))) * (0.5 + 0.5 * sin(u_time * 2.0 + h(floor(sp)) * 40.0));
	col += vec3(st) * 1.5;
	if (u_has > 0.5) {
		vec3 img = texture(u_tex, UV).rgb;
		col = img * (0.85 + u_level * 0.25) + col * 0.35;
	}
	float v = smoothstep(0.75, 0.2, length(UV - 0.5));
	ALBEDO = col * v;
}

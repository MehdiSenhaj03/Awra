// A rift in the ash: jagged edges, dark depth, an ember glow far below.
shader_type spatial;
render_mode unshaded;
#include "res://src/render/shaders/noise.gdshaderinc"
uniform float seed = 0.0;
void fragment() {
	vec2 c = UV * 2.0 - 1.0;
	float edge = 1.0 - abs(c.y) - (fbm2(vec2(UV.x * 10.0, seed)) - 0.5) * 0.5;
	float ends = 1.0 - pow(abs(c.x), 6.0);
	float k = edge * ends;
	if (k < 0.05) discard;
	float glow = smoothstep(0.55, 1.0, k) * (0.6 + 0.4 * fbm2(UV * vec2(20.0, 4.0) + vec2(TIME * 0.1, 0.0)));
	vec3 col = mix(vec3(0.12, 0.1, 0.09), vec3(0.02, 0.015, 0.012), smoothstep(0.05, 0.3, k));
	col += vec3(1.0, 0.32, 0.06) * glow * 0.9;
	ALBEDO = col;
}

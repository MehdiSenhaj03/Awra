// The oasis water: turquoise, sky reflections, ripples.
shader_type spatial;
#include "res://src/render/shaders/noise.gdshaderinc"
void fragment() {
	vec3 w = (INV_VIEW_MATRIX * vec4(VERTEX, 1.0)).xyz;
	float n = fbm2(w.xz * 0.15 + vec2(TIME * 0.2, TIME * 0.1));
	ALBEDO = mix(vec3(0.05, 0.35, 0.38), vec3(0.2, 0.62, 0.6), n);
	ROUGHNESS = 0.08;
	METALLIC = 0.2;
	SPECULAR = 0.8;
	NORMAL = normalize(NORMAL + (VIEW_MATRIX * vec4((n - 0.5) * 0.3, 0.0, (fbm2(w.xz * 0.2 - TIME * 0.15) - 0.5) * 0.3, 0.0)).xyz);
}

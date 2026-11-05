// Soft billboards of dust (storm walls, footstep puffs, smoke): a noisy disc, fading at the edges and close up.
shader_type spatial;
render_mode blend_mix, depth_draw_never, cull_disabled, unshaded;
#include "res://src/render/shaders/noise.gdshaderinc"
uniform vec3 col : source_color = vec3(0.55, 0.47, 0.40);
uniform float alpha = 0.6;
uniform float soft = 1.0;
uniform float speed = 0.1;
void vertex() {
	MODELVIEW_MATRIX = VIEW_MATRIX * mat4(INV_VIEW_MATRIX[0], INV_VIEW_MATRIX[1], INV_VIEW_MATRIX[2], MODEL_MATRIX[3]);
	MODELVIEW_MATRIX = MODELVIEW_MATRIX * mat4(vec4(length(MODEL_MATRIX[0].xyz), 0, 0, 0), vec4(0, length(MODEL_MATRIX[1].xyz), 0, 0), vec4(0, 0, 1, 0), vec4(0, 0, 0, 1));
}
void fragment() {
	vec2 c = UV * 2.0 - 1.0;
	float n = fbm2(UV * 3.0 + vec2(TIME * speed, -TIME * speed * 0.7) + vec2(INSTANCE_ID));
	float r = length(c) + (n - 0.5) * 0.6;
	float a = smoothstep(1.0, 0.25, r) * alpha * (0.6 + 0.6 * n) * COLOR.a;
	a *= clamp(-VERTEX.z / (8.0 * soft), 0.0, 1.0);
	ALBEDO = col * COLOR.rgb * (0.85 + 0.3 * n);
	ALPHA = a;
}

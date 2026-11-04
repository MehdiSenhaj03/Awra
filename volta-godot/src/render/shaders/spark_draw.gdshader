// Additive point sparks: coloured halo, white-hot core, fading with age.
shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_disabled, fog_disabled;

varying vec3 v_col;

void vertex() {
	// Billboard.
	MODELVIEW_MATRIX = VIEW_MATRIX * mat4(INV_VIEW_MATRIX[0], INV_VIEW_MATRIX[1], INV_VIEW_MATRIX[2], MODEL_MATRIX[3]);
	MODELVIEW_MATRIX = MODELVIEW_MATRIX * mat4(vec4(length(MODEL_MATRIX[0].xyz), 0.0, 0.0, 0.0), vec4(0.0, length(MODEL_MATRIX[1].xyz), 0.0, 0.0), vec4(0.0, 0.0, length(MODEL_MATRIX[2].xyz), 0.0), vec4(0.0, 0.0, 0.0, 1.0));
	float k = 1.0 - INSTANCE_CUSTOM.y / max(INSTANCE_CUSTOM.z, 0.001);
	v_col = COLOR.rgb * clamp(k, 0.0, 1.0);
}

void fragment() {
	vec2 d = UV * 2.0 - 1.0;
	float r2 = dot(d, d);
	float a = exp(-r2 * 4.0);
	float core = exp(-r2 * 24.0);
	ALBEDO = v_col * a + vec3(core) * length(v_col) * 0.35;
}

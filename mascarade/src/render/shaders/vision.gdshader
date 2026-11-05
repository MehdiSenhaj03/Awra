// Visibility polygons of guards and cameras: additive light on the floor, fading with distance, rim at the end.
shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_disabled, fog_disabled;

varying vec4 v_col;
varying float v_d;

void vertex() {
	v_col = COLOR;
	v_d = UV.x;
}

void fragment() {
	float d = v_d;
	float body = (1.0 - d) * 0.32 + 0.05;
	float rim = smoothstep(0.92, 1.0, d) * 0.5;
	ALBEDO = v_col.rgb * (body + rim) * v_col.a;
}

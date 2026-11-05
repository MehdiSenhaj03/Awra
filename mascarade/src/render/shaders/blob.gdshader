// Soft contact shadow under each guest; COLOR.a = strength, COLOR.rgb tints (blood pools).
shader_type spatial;
render_mode unshaded, blend_mix, depth_draw_never, cull_disabled;
varying vec4 col;
void vertex() { col = COLOR; }
void fragment() {
	float r = length(UV - 0.5) * 2.0;
	float a = (1.0 - smoothstep(0.35, 1.0, r)) * col.a;
	ALBEDO = col.rgb;
	ALPHA = a;
}

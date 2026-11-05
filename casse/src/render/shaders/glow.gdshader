// Pure light (unshaded, HDR colour so the glow catches it). Hit flash, shield tint and gain are per instance.
shader_type spatial;
render_mode unshaded, cull_disabled;

uniform vec3 color = vec3(1.0);
instance uniform float flash = 0.0;
instance uniform float shield = 0.0;
instance uniform float gain = 1.0;

void fragment() {
	vec3 c = mix(color, vec3(8.0), flash);
	ALBEDO = mix(c, vec3(0.5, 4.0, 2.0), shield * 0.4) * gain;
}

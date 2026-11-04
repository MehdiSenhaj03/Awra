shader_type spatial;
render_mode blend_mix, depth_draw_always;

uniform vec3 tint : source_color = vec3(0.08, 0.04, 0.14);
uniform float rough = 0.2;
uniform float metal = 0.8;
instance uniform vec3 rim_color = vec3(0.0);
instance uniform float shield = 0.0;
instance uniform float flash = 0.0;
instance uniform float alpha = 1.0;

void fragment() {
	ALBEDO = tint;
	METALLIC = metal;
	ROUGHNESS = rough;
	float fr = clamp(1.0 - abs(dot(normalize(NORMAL), normalize(VIEW))), 0.0, 1.0);
	vec3 rim = mix(mix(rim_color, vec3(10.0), flash), vec3(0.5, 4.0, 2.0), shield * 0.5);
	EMISSION = rim * fr * fr * fr;
	ALPHA = alpha;
}

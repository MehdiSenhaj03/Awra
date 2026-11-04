// Mirror-black stone with a neon fresnel rim. Rim colour and hit flash are per instance (shared material).
shader_type spatial;

uniform vec3 tint : source_color = vec3(0.043, 0.047, 0.078);
uniform float rough = 0.16;
uniform float metal = 0.92;
instance uniform vec3 rim_color = vec3(0.0);
instance uniform float shield = 0.0;
instance uniform float flash = 0.0;

void fragment() {
	ALBEDO = tint;
	METALLIC = metal;
	ROUGHNESS = rough;
	float fr = clamp(1.0 - abs(dot(normalize(NORMAL), normalize(VIEW))), 0.0, 1.0);
	vec3 rim = mix(mix(rim_color, vec3(10.0), flash), vec3(0.5, 4.0, 2.0), shield * 0.5);
	EMISSION = rim * fr * fr * fr;
}

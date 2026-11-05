// The river: slow brown water under a grey sky.
shader_type spatial;
uniform float t = 0.0;
void fragment() {
	vec2 p = UV * vec2(4.0, 200.0);
	float w = sin(p.y * 0.6 + t * 0.8 + sin(p.x * 3.0)) * 0.5 + 0.5;
	ALBEDO = mix(vec3(0.16, 0.14, 0.10), vec3(0.24, 0.22, 0.16), w);
	ROUGHNESS = 0.08;
	METALLIC = 0.1;
	NORMAL_MAP = normalize(vec3(0.5 + 0.08 * sin(p.y * 2.0 + t), 0.5 + 0.08 * cos(p.x * 7.0 - t), 1.0));
}

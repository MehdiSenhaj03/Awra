shader_type canvas_item;
uniform float pulse = 0.0;
void fragment() {
	vec3 a = vec3(0.48, 0.36, 1.0);
	vec3 b = vec3(1.0, 0.18, 0.72);
	vec3 c = vec3(1.0, 0.76, 0.29);
	vec3 col = UV.x < 0.5 ? mix(a, b, UV.x * 2.0) : mix(b, c, UV.x * 2.0 - 1.0);
	float k = 1.0 + pulse * (0.4 + 0.4 * sin(TIME * 7.85));
	COLOR = vec4(col * k, 1.0);
}

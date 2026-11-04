// A hairline fading from cyan through magenta to nothing.
shader_type canvas_item;
void fragment() {
	vec3 a = vec3(0.18, 0.95, 1.0);
	vec3 b = vec3(1.0, 0.18, 0.72);
	float x = UV.x;
	vec3 c = x < 0.5 ? mix(a, b, x * 2.0) : b;
	COLOR = vec4(c, x < 0.5 ? 1.0 : 1.0 - (x - 0.5) * 2.0);
}

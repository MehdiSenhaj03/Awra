shader_type canvas_item;
void fragment() {
	COLOR = vec4(mix(vec3(1.0, 0.18, 0.72), vec3(1.0), UV.x), 1.0);
}

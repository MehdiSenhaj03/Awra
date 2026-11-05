// Faint scan lines over the title art.
shader_type canvas_item;
void fragment() {
	float l = step(mod(FRAGCOORD.y, 3.0), 1.0);
	COLOR = vec4(vec3(1.0), 0.03 * l);
}

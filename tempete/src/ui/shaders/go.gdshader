// The PLAY button: a cyan to magenta slab with a slanted end; brighter when focused.
shader_type canvas_item;
uniform float on = 0.0;
void fragment() {
	vec2 px = UV / SCREEN_PIXEL_SIZE;
	float cut = UV.x > 1.0 - (1.0 - UV.y) * 0.0 ? 1.0 : 1.0;
	vec3 c = mix(vec3(1.0, 0.66, 0.28), vec3(0.75, 0.08, 0.15), UV.x) * (1.0 + 0.25 * on);
	COLOR = vec4(c, 1.0);
}

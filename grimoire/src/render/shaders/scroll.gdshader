// One half of the river painting (rows v0..v1), scrolling sideways as the mage walks; mirrored at every repeat so
// the seam never shows.
shader_type canvas_item;
uniform float v0 = 0.0;
uniform float v1 = 0.5;
uniform float offset = 0.0;
void fragment() {
	// The painting keeps its proportions: 1920 px of screen show 1.185 widths of it.
	float x = UV.x * 1.185 + offset;
	float m = mod(x, 2.0);
	x = m < 1.0 ? m : 2.0 - m;
	vec2 uv = vec2(x, mix(v0, v1, UV.y));
	COLOR = texture(TEXTURE, uv);
}

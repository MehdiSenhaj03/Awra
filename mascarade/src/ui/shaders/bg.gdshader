// Painted backgrounds: optional blur (mip level), brightness and grayscale, as the CSS filters did.
shader_type canvas_item;
uniform float blur = 0.0;
uniform float bright = 1.0;
uniform float gray = 0.0;
void fragment() {
	vec4 c = blur > 0.0 ? textureLod(TEXTURE, UV, blur) : texture(TEXTURE, UV);
	vec3 rgb = mix(c.rgb, vec3(dot(c.rgb, vec3(0.299, 0.587, 0.114))), gray) * bright;
	COLOR = vec4(rgb, c.a) * COLOR;
}

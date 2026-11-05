// Heads: skin in front, hair (COLOR) on the top and back, in the head's own space (+Z is the face).
shader_type spatial;
varying vec3 ln;
varying vec4 col;
void vertex() {
	ln = NORMAL;
	col = COLOR;
}
void fragment() {
	float hair = smoothstep(-0.05, -0.25, ln.z) + smoothstep(0.45, 0.65, ln.y);
	vec3 skin = vec3(0.86, 0.68, 0.56);
	ALBEDO = mix(skin, col.rgb, clamp(hair, 0.0, 1.0));
	ROUGHNESS = mix(0.55, 0.4, clamp(hair, 0.0, 1.0));
}

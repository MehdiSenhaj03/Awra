// Text filled with a moving gradient (the VOLTA logo, rank letters, grades).
shader_type canvas_item;
uniform vec4 c0 : source_color = vec4(1.0);
uniform vec4 c1 : source_color = vec4(0.75, 0.97, 1.0, 1.0);
uniform vec4 c2 : source_color = vec4(0.18, 0.95, 1.0, 1.0);
uniform vec4 c3 : source_color = vec4(1.0, 0.18, 0.72, 1.0);
uniform vec4 c4 : source_color = vec4(1.0, 0.76, 0.29, 1.0);
uniform float speed = 0.0;
uniform float vertical = 0.0;
uniform vec2 size = vec2(800.0, 200.0);
varying vec2 v_local;
void vertex() {
	v_local = VERTEX;
}
vec3 ramp(float x) {
	x = fract(x);
	if (x < 0.2) return mix(c0.rgb, c1.rgb, x / 0.2);
	if (x < 0.4) return mix(c1.rgb, c2.rgb, (x - 0.2) / 0.2);
	if (x < 0.6) return mix(c2.rgb, c3.rgb, (x - 0.4) / 0.2);
	if (x < 0.8) return mix(c3.rgb, c4.rgb, (x - 0.6) / 0.2);
	return mix(c4.rgb, c0.rgb, (x - 0.8) / 0.2);
}
void fragment() {
	vec4 t = texture(TEXTURE, UV);
	vec3 col;
	if (vertical > 0.5) {
		float y = clamp(v_local.y / size.y, 0.0, 1.0);
		col = mix(c0.rgb, c1.rgb, smoothstep(0.2, 1.0, y));
	} else {
		col = ramp(v_local.x / size.x / 3.0 + TIME * speed);
	}
	COLOR = vec4(col, t.a * COLOR.a);
}

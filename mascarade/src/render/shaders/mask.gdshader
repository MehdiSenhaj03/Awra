// Venetian masks from the 8-mask atlas; INSTANCE_CUSTOM.x = which mask, .y = flash.
shader_type spatial;
render_mode cull_back;
uniform sampler2D atlas : source_color, filter_linear_mipmap;
varying float idx;
varying float flash;
void vertex() {
	idx = INSTANCE_CUSTOM.x;
	flash = INSTANCE_CUSTOM.y;
	// Curve the quad around the face.
	VERTEX.z -= VERTEX.x * VERTEX.x * 2.2;
}
void fragment() {
	vec2 uv = vec2((floor(idx + 0.5) + UV.x) / 8.0, UV.y);
	vec4 t = texture(atlas, uv);
	if (t.a < 0.45) discard;
	ALBEDO = t.rgb;
	ROUGHNESS = 0.35;
	METALLIC = 0.2;
	EMISSION = t.rgb * 0.12 + vec3(1.0, 0.2, 0.15) * flash;
}

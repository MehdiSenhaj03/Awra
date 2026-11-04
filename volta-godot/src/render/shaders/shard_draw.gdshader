shader_type spatial;

varying float v_lit;
varying vec3 v_col;

void vertex() {
	v_lit = INSTANCE_CUSTOM.w;
	float k = clamp((INSTANCE_CUSTOM.z - INSTANCE_CUSTOM.y) * 0.8, 0.0, 1.0);
	v_col = COLOR.rgb * k;
}

void fragment() {
	if (v_lit > 0.5) {
		ALBEDO = vec3(0.0);
		METALLIC = 0.0;
		ROUGHNESS = 1.0;
		EMISSION = v_col;
	} else {
		ALBEDO = vec3(0.06, 0.06, 0.09);
		METALLIC = 0.92;
		ROUGHNESS = 0.2;
	}
}

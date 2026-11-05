// Vault walls: black lacquer with a brass band and a thin neon baseboard; brass caps on top.
shader_type spatial;

uniform vec3 u_accent : source_color = vec3(0.18, 0.95, 1.0);
varying vec3 v_w;
varying vec3 v_n;
varying float v_mask;

void vertex() {
	v_w = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	v_n = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);
	v_mask = floor(INSTANCE_CUSTOM.x * 15.0 + 0.5);
}

void fragment() {
	if (v_n.y > 0.5) {
		vec2 f = fract(v_w.xz);
		// Brass edge only where the wall ends (no wall neighbour on that side).
		float m = v_mask;
		float px = mod(m, 2.0); m = floor(m / 2.0);
		float nx = mod(m, 2.0); m = floor(m / 2.0);
		float pz = mod(m, 2.0); m = floor(m / 2.0);
		float nz = mod(m, 2.0);
		float e = 1.0;
		if (px < 0.5) e = min(e, 1.0 - f.x);
		if (nx < 0.5) e = min(e, f.x);
		if (pz < 0.5) e = min(e, 1.0 - f.y);
		if (nz < 0.5) e = min(e, f.y);
		float edge = 1.0 - smoothstep(0.0, 0.07, e);
		ALBEDO = vec3(0.05, 0.045, 0.04);
		METALLIC = 0.6;
		ROUGHNESS = 0.35;
		EMISSION = vec3(1.0, 0.72, 0.3) * edge * 0.35;
	} else {
		float y = v_w.y;
		float band = smoothstep(0.02, 0.0, abs(y - 0.92)) ;
		float base = smoothstep(0.05, 0.0, abs(y - 0.06));
		// Art deco fluting: soft vertical grooves.
		float u = fract((v_w.x + v_w.z) * 3.0);
		float flute = 0.5 + 0.5 * cos(u * 6.2831);
		ALBEDO = vec3(0.022, 0.024, 0.035) * (0.75 + 0.25 * flute) + vec3(0.5, 0.36, 0.12) * band;
		METALLIC = 0.3 + band * 0.6;
		ROUGHNESS = 0.25 + 0.2 * flute;
		EMISSION = vec3(1.0, 0.7, 0.28) * band * 0.5 + u_accent * base * 0.6;
	}
}

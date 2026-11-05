// Section roads: paved (dark asphalt, dashed yellow centre line) or dirt (ochre, tyre ruts), wet and glossy in the
// rain. UV.x runs across the road, UV.y along it (in metres).
shader_type spatial;
uniform float paved = 0.0;
uniform sampler2D wet : filter_linear;
uniform float size = 12000.0;
varying vec3 wpos;
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
void vertex() { wpos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz; }
void fragment() {
	float across = UV.x;
	float along = UV.y;
	float w = texture(wet, vec2(wpos.x, -wpos.z) / size).r;
	vec3 col;
	float rough;
	if (paved > 0.5) {
		col = vec3(0.12, 0.12, 0.13) * (0.85 + 0.3 * noise(vec2(across * 8.0, along * 0.5)));
		float line = step(abs(across - 0.5), 0.012) * step(0.5, fract(along / 6.0));
		col = mix(col, vec3(0.85, 0.7, 0.2), line);
		float edge = step(abs(across - 0.5), 0.45);
		col = mix(vec3(0.6, 0.58, 0.5), col, edge);
		rough = mix(0.75, 0.12, w);
	} else {
		col = vec3(0.50, 0.40, 0.27) * (0.8 + 0.3 * noise(vec2(across * 10.0, along * 0.8)));
		float rut = smoothstep(0.06, 0.0, abs(abs(across - 0.5) - 0.2));
		col *= 1.0 - 0.18 * rut;
		col = mix(col, col * vec3(0.55, 0.5, 0.45), w);
		rough = mix(0.95, 0.25, w);
	}
	ALBEDO = col;
	ROUGHNESS = rough;
	SPECULAR = 0.5;
}

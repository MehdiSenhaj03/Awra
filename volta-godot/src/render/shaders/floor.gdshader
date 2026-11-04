// Arena floor: black glass (a planar mirror rendered by a second camera), a fine hexagonal lattice that breathes
// with the beat, light near the hero, shock ripples, burning rim. Cut away outside the arena.
shader_type spatial;
render_mode unshaded, cull_disabled, blend_mix, depth_draw_never, fog_disabled;

uniform float u_time = 0.0;
uniform float u_beat = 0.0;
uniform float u_pulse = 0.0;
uniform float u_level = 0.0;
uniform float u_overdrive = 0.0;
uniform vec3 u_accent : source_color = vec3(0.18, 0.95, 1.0);
uniform vec3 u_accent2 : source_color = vec3(1.0, 0.18, 0.72);
uniform vec2 u_player = vec2(0.0);
uniform float u_shape = 0.0;
uniform vec3 u_size = vec3(20.0, 22.0, 14.0);
uniform vec4 u_ripples[16];
uniform vec3 u_pillars[8];
uniform sampler2D u_refl : filter_linear, repeat_disable;
uniform float u_has_refl = 0.0;
uniform mat4 u_mirror;

varying vec2 v_p;

void vertex() {
	v_p = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xz;
}

float edge_dist(vec2 p) {
	if (u_shape < 0.5) return length(p) - u_size.x;
	vec2 d = abs(p) - u_size.yz;
	return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

vec4 hex(vec2 p) {
	const vec2 s = vec2(1.0, 1.7320508);
	vec4 hc = floor(vec4(p, p - vec2(0.5, 1.0)) / s.xyxy) + 0.5;
	vec4 h = vec4(p - hc.xy * s, p - (hc.zw + 0.5) * s);
	vec4 r = dot(h.xy, h.xy) < dot(h.zw, h.zw) ? vec4(h.xy, hc.xy) : vec4(h.zw, hc.zw + 9.73);
	vec2 a = abs(r.xy);
	float e = 0.5 - max(dot(a, s * 0.5), a.x);
	return vec4(e, r.zw, 0.0);
}

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }

void fragment() {
	float ed = edge_dist(v_p);
	if (ed > 0.35) discard;
	vec4 h = hex(v_p / 1.6);
	float cell = hash(h.yz);
	float dp = length(v_p - u_player);
	vec3 col = vec3(0.002, 0.003, 0.006) + vec3(0.004, 0.005, 0.01) * cell;
	// Mirror: each glass tile is tilted a hair, so the reflection breaks along the lattice.
	if (u_has_refl > 0.5) {
		vec2 tilt = (vec2(hash(h.yz + 3.1), hash(h.yz + 7.7)) - 0.5) * 0.012;
		vec4 clip = u_mirror * vec4(v_p.x, 0.0, v_p.y, 1.0);
		vec2 ndc = clip.xy / clip.w;
		vec2 ruv = vec2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5) + tilt;
		vec3 refl = texture(u_refl, ruv).rgb;
		vec3 V = normalize(CAMERA_POSITION_WORLD - vec3(v_p.x, 0.0, v_p.y));
		float fres = 0.07 + 0.45 * pow(1.0 - max(V.y, 0.0), 3.0);
		col += refl * fres * smoothstep(0.0, 0.08, h.x);
	}
	col += u_accent * 0.025 * exp(-dp * 0.25);
	float line = smoothstep(0.016, 0.0, h.x);
	float w = 0.5 + 0.5 * sin(length(v_p) * 0.45 - u_beat * 6.2831);
	float wave = w * w * w * w * w * w;
	float lit = 0.012 + 0.45 * u_pulse * wave + 0.6 * exp(-dp * 0.5);
	col += mix(u_accent, u_accent2, cell) * line * lit;
	float on = step(0.965 - u_level * 0.05, fract(cell * 7.13 + floor(u_beat) * 0.618));
	float fb = 1.0 - fract(u_beat);
	col += mix(u_accent2, u_accent, cell) * on * 0.08 * fb * fb * smoothstep(0.0, 0.2, h.x);
	for (int i = 0; i < 16; i++) {
		vec4 r = u_ripples[i];
		float age = u_time - r.z;
		if (age < 0.0 || age > 1.0) continue;
		float d = length(v_p - r.xy);
		float front = age * 18.0 * (0.6 + r.w * 0.5);
		float k = 1.0 - age;
		float q = (d - front) * 2.4;
		col += mix(u_accent, vec3(1.0), 0.3) * exp(-q * q) * k * k * r.w * (0.06 + line * 1.8);
	}
	for (int i = 0; i < 8; i++) {
		vec3 pq = u_pillars[i];
		if (pq.z <= 0.0) continue;
		float d = length(v_p - pq.xy) - pq.z;
		col *= mix(0.25, 1.0, smoothstep(0.0, 1.2, d));
		col += u_accent * 1.6 * exp(-abs(d) * 12.0);
	}
	col += vec3(1.0, 0.75, 0.3) * line * u_overdrive * 0.5 * wave;
	float rim = exp(-abs(ed + 0.1) * 7.0);
	col += u_accent * rim * (2.2 + 2.5 * u_pulse);
	col += vec3(1.0) * exp(-abs(ed + 0.05) * 30.0) * 1.2;
	col *= mix(1.0, 0.55, smoothstep(-3.0, 0.0, ed) * (1.0 - rim));
	ALBEDO = col;
	ALPHA = 1.0 - smoothstep(0.0, 0.35, ed);
}

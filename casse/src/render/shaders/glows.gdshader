// All glowing quads lying on the floor plane, in one MultiMesh draw: bullets, shots, shock rings, lasers,
// telegraphs. The 12 floats of each instance transform carry the data directly (the basis is not a rotation):
// MODEL_MATRIX[0] = (style, angle, length), [1] = (width, param, -), [2] = (r, g, b), [3] = position.
shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_disabled, fog_disabled, skip_vertex_transform;

uniform float u_time = 0.0;
varying vec2 v_uv;
varying vec3 v_col;
varying float v_st;
varying float v_prm;
varying float v_len;

void vertex() {
	vec3 a = MODEL_MATRIX[0].xyz;
	vec3 b = MODEL_MATRIX[1].xyz;
	v_col = MODEL_MATRIX[2].xyz;
	vec3 o = MODEL_MATRIX[3].xyz;
	v_st = a.x;
	v_len = a.z;
	v_prm = b.y;
	v_uv = VERTEX.xz;
	vec2 p = VERTEX.xz * vec2(a.z, b.x);
	float c = cos(a.y);
	float s = sin(a.y);
	vec3 w = vec3(o.x + p.x * c - p.y * s, o.y, o.z + p.x * s + p.y * c);
	VERTEX = (VIEW_MATRIX * vec4(w, 1.0)).xyz;
	NORMAL = vec3(0.0, 0.0, 1.0);
}

void fragment() {
	vec2 uv = v_uv;
	float st = v_st;
	float prm = v_prm;
	vec3 vc = v_col;
	vec3 c = vec3(0.0);
	if (st < 0.5) {
		float r = length(uv);
		float halo = exp(-r * r * 5.0) * 0.9;
		float rim = smoothstep(0.62, 0.5, r) * smoothstep(0.3, 0.45, r);
		float core = smoothstep(0.42, 0.2, r);
		c = vc * (halo + rim * 1.6) + vec3(1.0) * core * 2.2;
	} else if (st < 1.5) {
		float ax = abs(uv.x);
		float ay = abs(uv.y);
		float body = exp(-ay * ay * 9.0) * smoothstep(1.0, 0.55, ax);
		float core = exp(-ay * ay * 60.0) * smoothstep(1.0, 0.3, ax);
		c = vc * body * 1.3 + vec3(1.0) * core * 2.0 * length(vc) * 0.5;
	} else if (st < 2.5) {
		float r = length(uv);
		float t = max(prm, 0.02);
		float q = (r - (1.0 - t)) / t;
		c = vc * exp(-q * q * 2.5) * step(r, 1.0);
	} else if (st < 3.5) {
		float ay = abs(uv.y);
		float fl = 0.85 + 0.15 * sin(u_time * 60.0 + uv.x * 20.0);
		c = (vc * exp(-ay * ay * 4.0) * 1.4 + vec3(1.0) * exp(-ay * ay * 40.0) * 2.5) * fl * smoothstep(1.0, 0.97, abs(uv.x));
	} else if (st < 4.5) {
		float ay = abs(uv.y);
		float dash = step(0.5, fract(uv.x * v_len * 0.6 - u_time * 3.0));
		c = vc * exp(-ay * ay * 30.0) * (0.25 + 0.75 * prm) * (0.45 + 0.55 * dash) * 1.5;
	} else if (st < 5.5) {
		float r = length(uv);
		float qe = (r - 0.96) * 20.0;
		float qf = (r - prm) * 18.0;
		float edge = exp(-qe * qe) * 1.4;
		float fill = step(r, prm) * 0.22 + exp(-qf * qf) * 0.9 * step(r, 1.0);
		float stripes = 0.6 + 0.4 * step(0.5, fract((uv.x + uv.y) * 4.0 - u_time * 1.5));
		c = vc * (edge + fill * stripes) * step(r, 1.02);
	} else if (st < 6.5) {
		float r = length(uv);
		c = vc * exp(-r * r * 3.5) * step(r, 1.0);
	} else if (st < 7.5) {
		// Vision cone: a sector along +x, prm = cosine of the half-angle, fading with distance; edges brighter.
		float r = length(uv);
		float ca = r > 0.0001 ? uv.x / r : 1.0;
		float inside = step(prm, ca) * step(r, 1.0) * step(0.0, uv.x);
		float edge = smoothstep(prm + 0.03, prm, ca) * 0.8;
		float rim = smoothstep(0.93, 1.0, r) * 0.6;
		c = vc * inside * ((1.0 - r) * 0.35 + edge + rim + 0.06);
	} else {
		// Dashed dot (trajectories).
		float r = length(uv);
		c = vc * smoothstep(1.0, 0.4, r);
	}
	ALBEDO = min(c, vec3(40.0));
}

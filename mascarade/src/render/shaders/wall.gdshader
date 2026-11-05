// Palace walls, all from world position: dark carved wainscot, crimson silk damask above, gilded mouldings and
// pilasters (salon, opera); iron and glass panes over the night garden (greenhouse).
shader_type spatial;

uniform int style = 0;
uniform vec3 silk : source_color = vec3(0.45, 0.04, 0.06);
uniform vec3 wood : source_color = vec3(0.16, 0.07, 0.04);
uniform vec3 gold : source_color = vec3(0.86, 0.63, 0.26);
uniform float height = 4.5;

varying vec3 wpos;
varying vec3 wnrm;

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }

void vertex() {
	wpos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	wnrm = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);
}

// A damask motif: a repeated fleuron made of lobes, in a diamond lattice.
float damask(vec2 q) {
	vec2 g = fract(q) - 0.5;
	vec2 g2 = fract(q + 0.5) - 0.5;
	float m = 0.0;
	for (int k = 0; k < 2; k++) {
		vec2 d = k == 0 ? g : g2;
		float r = length(d * vec2(1.0, 0.8));
		float a = atan(d.y, d.x);
		float lobes = 0.22 + 0.08 * cos(a * 3.0 + 1.57) + 0.03 * cos(a * 8.0);
		m = max(m, smoothstep(0.02, 0.0, r - lobes) - smoothstep(0.02, 0.0, r - lobes * 0.45) * 0.6);
	}
	return m;
}

void fragment() {
	float u = abs(wnrm.x) > 0.5 ? wpos.z : wpos.x;
	float y = wpos.y;
	vec3 col;
	float rough = 0.6;
	float metal = 0.0;
	vec3 emit = vec3(0.0);
	if (wnrm.y > 0.5) {
		// Top of the wall: dark cornice.
		col = wood * 0.6;
	} else if (style == 1) {
		// Greenhouse: cast-iron frame, tall glass panes with the night garden behind (dark blue, a few stars).
		float pane_u = fract(u / 1.5);
		float pane_y = fract((y - 0.6) / 1.3);
		float frame = step(pane_u, 0.04) + step(0.96, pane_u) + step(pane_y, 0.05) + step(0.95, pane_y);
		vec3 night = mix(vec3(0.02, 0.05, 0.08), vec3(0.05, 0.10, 0.14), y / height);
		float star = step(0.995, hash(floor(vec2(u, y) * 14.0))) * (y > 1.5 ? 1.0 : 0.0);
		col = night;
		emit = night * 0.6 + vec3(star) * 0.8;
		rough = 0.05;
		if (frame > 0.5 || y < 0.6) {
			col = vec3(0.06, 0.09, 0.08);
			emit = vec3(0.0);
			rough = 0.5;
			metal = 0.6;
		}
	} else {
		if (y < 1.1) {
			// Wainscot: raised panels.
			float pu = fract(u / 1.2);
			float panel = smoothstep(0.06, 0.09, pu) * smoothstep(0.94, 0.91, pu) * smoothstep(0.15, 0.2, y) * smoothstep(0.95, 0.9, y);
			col = wood * (0.7 + 0.5 * panel) * (0.9 + 0.2 * hash(vec2(floor(u / 1.2), 3.0)));
			rough = 0.35;
		} else {
			vec2 q = vec2(u, y) * vec2(1.6, 1.3);
			float d = damask(q);
			col = silk * (0.7 + 0.25 * sin(u * 40.0) * 0.1) + silk * d * 0.9;
			rough = 0.75 - d * 0.3;
			// Pilasters every 4 m (opera: gilded, salon: ivory).
			float pil = abs(fract(u / 4.0 + 0.5) - 0.5) * 4.0;
			if (pil < 0.28) {
				col = style == 2 ? gold * (0.7 + 0.3 * smoothstep(0.0, 0.28, pil)) : vec3(0.78, 0.72, 0.62) * (0.8 + 0.2 * smoothstep(0.0, 0.28, pil));
				metal = style == 2 ? 0.85 : 0.0;
				rough = 0.3;
			}
		}
		// Gilded mouldings: chair rail and cornice.
		float m = step(abs(y - 1.12), 0.05) + step(abs(y - (height - 0.25)), 0.12);
		if (m > 0.5) {
			col = gold;
			metal = 0.9;
			rough = 0.28;
		}
	}
	ALBEDO = col;
	ROUGHNESS = rough;
	METALLIC = metal;
	EMISSION = emit;
}

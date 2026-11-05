// The ballroom floor, one plane over the whole venue. A tile map (R = tile type) chooses between polished marble
// checkerboard (salon), flagstones and gravel (greenhouse), star-inlaid marble (opera) and a parquet dance floor
// with a gilded border. Polished, so screen-space reflections pick up the guests and the candles.
shader_type spatial;
render_mode cull_back;

uniform sampler2D tiles : filter_nearest;
uniform vec2 size = vec2(36.0, 22.0);
uniform int style = 0;
uniform vec3 marble_a : source_color = vec3(0.70, 0.64, 0.56);
uniform vec3 marble_b : source_color = vec3(0.30, 0.15, 0.13);
uniform vec3 wood_a : source_color = vec3(0.46, 0.31, 0.17);
uniform vec3 wood_b : source_color = vec3(0.27, 0.16, 0.08);
uniform vec3 gold : source_color = vec3(0.85, 0.62, 0.25);

varying vec3 wpos;

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), f.x), mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), f.x), f.y);
}

float fbm(vec2 p) {
	float v = 0.0;
	float a = 0.5;
	for (int i = 0; i < 5; i++) {
		v += a * noise(p);
		p = p * 2.03 + vec2(1.7, 9.2);
		a *= 0.5;
	}
	return v;
}

// Marble veins: thin dark lines along a warped sine.
float veins(vec2 p) {
	float w = fbm(p * 0.9) * 6.0;
	float v = abs(sin(p.x * 1.3 + p.y * 0.6 + w));
	return 1.0 - smoothstep(0.0, 0.09, v);
}

int tile_at(vec2 t) {
	if (t.x < 0.0 || t.y < 0.0 || t.x >= size.x || t.y >= size.y) return 2;
	return int(texelFetch(tiles, ivec2(t), 0).r * 255.0 + 0.5);
}

void vertex() {
	wpos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

void fragment() {
	vec2 p = wpos.xz;
	vec2 cell = floor(p);
	vec2 f = fract(p);
	int t = tile_at(cell);
	vec3 col;
	float rough = 0.26;
	float metal = 0.0;
	if (t == 1) {
		// Parquet: basket weave, four oak strips per half-metre square, alternating direction.
		vec2 q = p * 2.0;
		vec2 sq = floor(q);
		vec2 fq = fract(q);
		bool turn = mod(sq.x + sq.y, 2.0) > 0.5;
		float along = turn ? fq.y : fq.x;
		float across = turn ? fq.x : fq.y;
		float strip = floor(across * 4.0);
		float k = hash(sq * 7.0 + strip);
		vec3 w = mix(wood_b, wood_a, 0.35 + 0.65 * k);
		w *= 0.85 + 0.25 * noise(vec2(along * 3.0, strip * 7.0 + sq.x * 3.0) * vec2(1.0, 1.0) + fbm(vec2(along * 18.0, strip)) * 0.6);
		float seam = smoothstep(0.0, 0.06, fract(across * 4.0)) * smoothstep(0.0, 0.025, min(fq.x, fq.y));
		col = w * (0.6 + 0.4 * seam);
		// Gilded border where the dance floor meets the marble.
		float b = 1.0;
		if (tile_at(cell + vec2(-1.0, 0.0)) != 1) b = min(b, f.x);
		if (tile_at(cell + vec2(1.0, 0.0)) != 1) b = min(b, 1.0 - f.x);
		if (tile_at(cell + vec2(0.0, -1.0)) != 1) b = min(b, f.y);
		if (tile_at(cell + vec2(0.0, 1.0)) != 1) b = min(b, 1.0 - f.y);
		if (b < 0.12) {
			col = gold * (0.75 + 0.25 * noise(p * 30.0));
			metal = 0.9;
			rough = 0.25;
			if (b < 0.035 || (b > 0.085)) col *= 0.55;
		}
		rough = max(rough, 0.28);
	} else if (style == 1) {
		// Greenhouse: large pale flagstones and dark joints, mossy edges.
		vec2 q = p * 0.5;
		vec2 fq = fract(q + vec2(mod(floor(q.y), 2.0) * 0.5, 0.0));
		float j = min(min(fq.x, 1.0 - fq.x), min(fq.y, 1.0 - fq.y));
		float k = hash(floor(q + vec2(mod(floor(q.y), 2.0) * 0.5, 0.0)));
		col = mix(vec3(0.38, 0.37, 0.32), vec3(0.5, 0.48, 0.42), k) * (0.8 + 0.2 * fbm(p * 3.0));
		col = mix(vec3(0.10, 0.14, 0.08), col, smoothstep(0.0, 0.04, j));
		rough = 0.35;
	} else {
		// Marble checkerboard (salon) or ivory marble with compass stars (opera).
		// Slabs of half a metre laid on the diagonal.
		vec2 dg = vec2(p.x + p.y, p.x - p.y) * 0.70710678 / 0.6;
		vec2 dcell = floor(dg);
		f = fract(dg);
		bool dark = mod(dcell.x + dcell.y, 2.0) > 0.5;
		vec3 base = marble_a;
		if (style == 0 && dark) base = marble_b;
		float n = fbm(p * 2.2 + (dark ? 31.0 : 0.0));
		col = base * (0.82 + 0.25 * n);
		col = mix(col, col * (dark ? 1.6 : 0.7), veins(p * 1.7 + dcell * 3.1) * 0.45);
		if (style == 2) {
			vec2 g = fract(p / 4.0) - 0.5;
			float r = length(g);
			float a = atan(g.y, g.x);
			float star = smoothstep(0.02, 0.0, r - (0.07 + 0.05 * pow(abs(cos(a * 4.0)), 8.0)));
			col = mix(col, marble_b * 1.2, star * 0.8);
			float ring = smoothstep(0.008, 0.0, abs(r - 0.15)) * 0.7;
			col = mix(col, gold, ring);
			metal = ring * 0.9;
		}
		// Fine joints between the slabs.
		float j = min(min(f.x, 1.0 - f.x), min(f.y, 1.0 - f.y));
		col *= 0.75 + 0.25 * smoothstep(0.0, 0.015, j);
	}
	// Soft edge darkening near the walls (dust, shade).
	float edge = min(min(p.x - 1.0, size.x - 1.0 - p.x), min(p.y - 1.0, size.y - 1.0 - p.y));
	col *= 0.55 + 0.45 * smoothstep(0.0, 2.5, edge);
	ALBEDO = col;
	ROUGHNESS = rough;
	METALLIC = metal;
	SPECULAR = 0.6;
}

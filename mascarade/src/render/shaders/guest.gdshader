// Guests (one MultiMesh per part). COLOR = the part's colour; INSTANCE_CUSTOM: r = flash (shot reveal),
// g = accent hue index, b = 0..1 sheen, a = unused. `kind` picks the tailoring: 0 frock coat, 1 ball gown,
// 2 sleeve with glove, 3 plain (hats, plumes).
shader_type spatial;

uniform int kind = 0;
varying vec3 lp;
varying vec4 col;
varying vec4 cust;

void vertex() {
	lp = VERTEX;
	col = COLOR;
	cust = INSTANCE_CUSTOM;
}

void fragment() {
	vec3 c = col.rgb;
	float rough = 0.7;
	float metal = 0.0;
	vec3 gold = vec3(0.85, 0.62, 0.25);
	if (kind == 0) {
		if (lp.y < 0.52) {
			c = vec3(0.03, 0.025, 0.03);       // trousers
			rough = 0.6;
		}
		// Gold piping on the hem and cuffs of the coat, buttons down the front, white jabot at the collar.
		if (abs(lp.y - 0.56) < 0.02) { c = gold; metal = 0.8; rough = 0.3; }
		if (lp.z > 0.1 && abs(lp.x) < 0.035 && lp.y > 0.6 && lp.y < 1.15 && fract(lp.y * 12.0) < 0.35) { c = gold; metal = 0.9; rough = 0.25; }
		if (lp.z > 0.08 && abs(lp.x) < 0.07 && lp.y > 1.12) c = vec3(0.92, 0.9, 0.86);
		// Velvet sheen.
		rough = min(rough, 0.55 + 0.2 * cust.b);
	} else if (kind == 1) {
		// Lace hem, gold sash at the waist, embroidered skirt.
		if (lp.y < 0.07) c = vec3(0.92, 0.89, 0.82);
		if (abs(lp.y - 0.95) < 0.03) { c = gold; metal = 0.8; rough = 0.3; }
		float emb = step(0.93, fract(lp.y * 9.0 + 0.5 * sin(atan(lp.z, lp.x) * 8.0)));
		if (lp.y > 0.1 && lp.y < 0.8) c = mix(c, gold, emb * 0.6);
		if (lp.y > 1.15 && lp.z > 0.05) c = mix(c, vec3(0.93, 0.85, 0.75), 0.6);   // décolleté
		rough = 0.45;
	} else if (kind == 2) {
		if (lp.y < -0.4) { c = vec3(0.9, 0.88, 0.82); rough = 0.6; }
		if (abs(lp.y + 0.38) < 0.02) { c = gold; metal = 0.8; }
	}
	vec3 flash = vec3(1.0, 0.2, 0.15) * cust.r;
	ALBEDO = c;
	ROUGHNESS = rough;
	METALLIC = metal;
	EMISSION = flash * 2.0;
	RIM = 0.25;
	RIM_TINT = 0.4;
}

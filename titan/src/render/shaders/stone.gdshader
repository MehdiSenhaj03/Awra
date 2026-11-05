// A titan's skin: living rock, lumpy and faceted, moss and lichen where rain would sit (Ama), or cracked red hide
// with lava in the cracks (a wild titan). Textured in object space (scaled) so it sticks to the walking body.
shader_type spatial;
#include "res://src/render/shaders/noise.gdshaderinc"
uniform vec3 rock : source_color = vec3(0.6, 0.58, 0.54);
uniform vec3 rock_dark : source_color = vec3(0.27, 0.25, 0.23);
uniform vec3 moss : source_color = vec3(0.36, 0.47, 0.20);
uniform float moss_amt = 0.6;
uniform vec3 crack_col : source_color = vec3(1.0, 0.36, 0.08);
uniform float crack_glow = 0.0;
uniform float bump = 1.5;
uniform float facet = 0.92;
uniform float hit = 0.0;
uniform vec3 hit_col : source_color = vec3(1.0, 0.85, 0.6);
uniform float seed = 0.0;
varying vec3 opos;
varying vec3 onrm;

void vertex() {
	vec3 sc = vec3(length(MODEL_MATRIX[0].xyz), length(MODEL_MATRIX[1].xyz), length(MODEL_MATRIX[2].xyz));
	vec3 p = VERTEX * sc + vec3(seed * 13.1);
	float d = (fbm3(p * 0.1) - 0.5) * 2.0 * bump + (vnoise(p * 0.45) - 0.5) * bump * 0.6;
	VERTEX += NORMAL * d / max(sc, vec3(0.001));
	opos = p;
	onrm = NORMAL;
}

void fragment() {
	vec3 fn = normalize(cross(dFdx(VERTEX), dFdy(VERTEX)));
	if (dot(fn, NORMAL) < 0.0) fn = -fn;
	NORMAL = normalize(mix(NORMAL, fn, facet));
	vec3 wn = (INV_VIEW_MATRIX * vec4(NORMAL, 0.0)).xyz;
	float n = fbm3(opos * 0.25);
	float strata = sin(opos.y * 1.3 + n * 4.0) * 0.5 + 0.5;
	vec3 col = mix(rock_dark, rock, smoothstep(0.25, 0.75, n) * 0.8 + strata * 0.2);
	// Lichen speckles.
	float lich = smoothstep(0.74, 0.8, vnoise(opos * 1.3));
	col = mix(col, vec3(0.72, 0.68, 0.45), lich * 0.25 * moss_amt);
	// Moss on what faces the sky, in patches.
	float up = smoothstep(0.2, 0.7, wn.y);
	float patchy = smoothstep(0.3, 0.55, fbm3(opos * 0.15 + vec3(5.0)));
	float m = up * patchy * moss_amt;
	col = mix(col, moss * (0.75 + 0.5 * vnoise(opos * 2.3)), clamp(m, 0.0, 1.0));
	// Cavities are darker.
	col *= 0.55 + 0.55 * smoothstep(0.2, 0.75, n);
	// Lava cracks: thin ridged lines.
	float r = abs(fbm3(opos * 0.35) - 0.5);
	float crack = smoothstep(0.035, 0.0, r) * crack_glow;
	ALBEDO = mix(col, crack_col * 0.4, crack * 0.6);
	ROUGHNESS = 0.95;
	SPECULAR = 0.2;
	EMISSION = crack_col * crack * (2.5 + 1.5 * sin(TIME * 2.0 + opos.x)) + hit_col * hit * 0.8;
}

// The Ash Sea: grey and ochre dunes scrolling under the titan (world offset), ripples, dark troughs, ember-flecked
// dark ash where storms passed, faint tracks. A gentle dune height away from the titan's path.
shader_type spatial;
#include "res://src/render/shaders/noise.gdshaderinc"
uniform vec2 offset = vec2(0.0);     // render position of the world origin (x, z)
uniform vec3 ash : source_color = vec3(0.56, 0.55, 0.53);
uniform vec3 ochre : source_color = vec3(0.6, 0.52, 0.42);
uniform vec3 dark : source_color = vec3(0.30, 0.28, 0.27);
uniform vec3 tint : source_color = vec3(1.0);
uniform float green = 0.0;           // an oasis nearby
uniform vec2 oasis = vec2(0.0, -99999.0);
varying vec2 wp;
varying float hgt;

float dunes(vec2 p) {
	float d = sin(p.x * 0.012 + p.y * 0.004 + fbm2(p * 0.003) * 6.0) * 0.5 + 0.5;
	d = d * d;
	return d * 0.8 + fbm2(p * 0.008) * 0.6;
}

void vertex() {
	vec3 w = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	wp = w.xz - offset;
	float amp = mix(0.0, 9.0, smoothstep(45.0, 260.0, abs(w.x)));
	hgt = dunes(wp);
	VERTEX.y += (hgt - 0.5) * amp;
}

void fragment() {
	float n = fbm2(wp * 0.05);
	float ripple = sin(wp.x * 1.6 + wp.y * 0.6 + n * 8.0) * 0.5 + 0.5;
	vec3 col = mix(ash, ochre, smoothstep(0.35, 0.8, fbm2(wp * 0.004 + vec2(7.0)) + hgt * 0.2));
	col = mix(dark, col, 0.65 + 0.35 * smoothstep(0.1, 0.7, hgt));
	col *= 0.9 + 0.1 * ripple;
	col *= 0.85 + 0.3 * fbm2(wp * 0.6);
	float g = green * smoothstep(90.0, 30.0, length(wp - oasis)) * (0.7 + 0.3 * fbm2(wp * 0.2));
	col = mix(col, vec3(0.26, 0.42, 0.16), g);
	ALBEDO = col * tint;
	ROUGHNESS = 0.97;
	// Bumpy normal from the ripples.
	float e = 0.4;
	float hx = sin((wp.x + e) * 1.6 + wp.y * 0.6 + n * 8.0) - sin(wp.x * 1.6 + wp.y * 0.6 + n * 8.0);
	NORMAL = normalize(NORMAL + (VIEW_MATRIX * vec4(-hx * 0.08, 0.0, 0.0, 0.0)).xyz);
}

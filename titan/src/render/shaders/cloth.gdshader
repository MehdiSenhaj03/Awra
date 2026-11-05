// Sails, awnings and banners: weathered canvas waving in the wind (tied along the top edge).
shader_type spatial;
render_mode cull_disabled;
#include "res://src/render/shaders/noise.gdshaderinc"
uniform vec3 col : source_color = vec3(0.8, 0.7, 0.55);
uniform vec3 stripe : source_color = vec3(0.65, 0.25, 0.15);
uniform float stripes = 0.0;
uniform float wind = 1.0;
uniform float seed = 0.0;
void vertex() {
	float w = sin(TIME * 2.4 + VERTEX.x * 1.3 + seed * 5.0) * 0.5 + sin(TIME * 3.7 + VERTEX.y * 2.1 + seed) * 0.25;
	VERTEX.z += w * UV.y * 0.6 * wind;
}
void fragment() {
	float n = fbm2(UV * vec2(6.0, 9.0) + vec2(seed));
	vec3 c = col;
	if (stripes > 0.0 && fract(UV.x * stripes) < 0.5) c = stripe;
	c *= 0.75 + 0.35 * n;
	// Frayed lower edge.
	if (UV.y > 0.92 + 0.08 * vnoise2(vec2(UV.x * 30.0, seed))) discard;
	ALBEDO = c;
	ROUGHNESS = 0.95;
	BACKLIGHT = c * 0.5;
}

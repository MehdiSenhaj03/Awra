// Screen post-processing on top of the 3D image: shock-wave distortions, radial chromatic aberration,
// vignette, film grain, full-screen flash.
shader_type canvas_item;

uniform sampler2D screen_tex : hint_screen_texture, filter_linear_mipmap;
uniform vec4 u_shocks[3];   // xy = screen uv, z = age (0..1, <0 inactive), w = amplitude
uniform float u_chroma = 0.0006;
uniform float u_time = 0.0;
uniform vec4 u_flash = vec4(0.0);
uniform float u_vignette = 0.72;
uniform float u_grain = 0.06;
uniform float u_aspect = 1.7777;

void fragment() {
	vec2 uv = SCREEN_UV;
	for (int i = 0; i < 3; i++) {
		vec4 s = u_shocks[i];
		if (s.z < 0.0 || s.z > 1.0) continue;
		vec2 d = uv - s.xy;
		d.x *= u_aspect;
		float r = length(d);
		float front = s.z * 0.6;
		float band = 1.0 - smoothstep(0.0, 0.11, abs(r - front));
		uv -= normalize(d + 1e-5) * band * s.w * (1.0 - s.z) * vec2(1.0 / u_aspect, 1.0);
	}
	vec2 dc = uv - 0.5;
	float rr = dot(dc, dc);
	vec2 off = dc * u_chroma * (0.2 + rr * 8.0);
	vec3 col;
	col.r = textureLod(screen_tex, uv + off, 0.0).r;
	col.g = textureLod(screen_tex, uv, 0.0).g;
	col.b = textureLod(screen_tex, uv - off, 0.0).b;
	float v = smoothstep(0.95, 0.28, length(dc * vec2(1.0, 0.85))) ;
	col *= mix(1.0 - u_vignette, 1.0, v);
	float g = fract(sin(dot(uv * 1000.0 + u_time, vec2(12.9898, 78.233))) * 43758.5453) - 0.5;
	col += g * u_grain * 0.5;
	col = mix(col, max(col, u_flash.rgb), u_flash.a);
	COLOR = vec4(col, 1.0);
}

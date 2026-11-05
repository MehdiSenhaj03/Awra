// A studio of neon strips: what the obsidian and the glass reflect (never shown as background).
shader_type sky;

vec3 strip(vec3 d, vec3 pos, float w, float h, float ry, vec3 col) {
	float dist = length(pos);
	vec3 z = pos / dist;
	float dn = dot(d, z);
	if (dn <= 0.0) return vec3(0.0);
	vec3 hit = d * (dist / dn) - pos;
	vec3 up = abs(z.y) > 0.99 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
	vec3 x = normalize(cross(up, z));
	vec3 y = cross(z, x);
	float c = cos(ry);
	float s = sin(ry);
	vec3 xr = x * c + y * s;
	vec3 yr = -x * s + y * c;
	float u = abs(dot(hit, xr)) - w * 0.5;
	float v = abs(dot(hit, yr)) - h * 0.5;
	float k = smoothstep(0.4, -0.2, max(u, v));
	return col * k;
}

void sky() {
	vec3 d = EYEDIR;
	vec3 c = mix(vec3(0.004, 0.005, 0.012), vec3(0.03, 0.04, 0.09), smoothstep(-0.2, 1.0, d.y));
	c += strip(d, vec3(-30.0, 18.0, 10.0), 40.0, 1.6, 0.3, vec3(0.03, 0.89, 1.0) * 6.0);
	c += strip(d, vec3(30.0, 12.0, -15.0), 36.0, 1.4, -0.2, vec3(1.0, 0.03, 0.48) * 5.0);
	c += strip(d, vec3(0.0, 40.0, 0.0), 30.0, 6.0, 0.0, vec3(8.0));
	c += strip(d, vec3(10.0, 6.0, 35.0), 30.0, 1.0, 0.1, vec3(1.0, 0.55, 0.06) * 3.0);
	c += strip(d, vec3(-20.0, 4.0, -35.0), 30.0, 1.2, 0.0, vec3(0.33, 0.1, 1.0) * 4.0);
	c += strip(d, vec3(35.0, 25.0, 20.0), 2.0, 30.0, 0.0, vec3(0.03, 0.89, 1.0) * 3.0);
	COLOR = c;
}

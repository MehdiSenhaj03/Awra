class_name Shapes
## Procedural meshes for the palace and its guests: lathes (columns, gowns, vases, chandeliers), boxes, tori,
## cached by key so every guest and every column share one GPU buffer.

static var _cache: Dictionary = {}

static func cached(key: String, make: Callable) -> Mesh:
	var m: Mesh = _cache.get(key)
	if m == null:
		m = make.call()
		_cache[key] = m
	return m

## Surface of revolution around Y of a profile of (radius, height) points, bottom to top. Smooth normals.
## `squash` scales the depth (Z) to flatten round shapes (a coat is wider than deep).
static func lathe(profile: PackedVector2Array, seg: int = 16, squash: float = 1.0, cap_top: bool = true, cap_bottom: bool = false) -> ArrayMesh:
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var n := profile.size()
	for i in n - 1:
		var a := profile[i]
		var b := profile[i + 1]
		var slope := Vector2(b.y - a.y, -(b.x - a.x)).normalized()   # outward normal in (r, y)
		for s in seg:
			var t0 := TAU * s / seg
			var t1 := TAU * (s + 1) / seg
			var p00 := Vector3(cos(t0) * a.x, a.y, sin(t0) * a.x * squash)
			var p01 := Vector3(cos(t1) * a.x, a.y, sin(t1) * a.x * squash)
			var p10 := Vector3(cos(t0) * b.x, b.y, sin(t0) * b.x * squash)
			var p11 := Vector3(cos(t1) * b.x, b.y, sin(t1) * b.x * squash)
			var n0 := Vector3(cos(t0) * slope.x, slope.y, sin(t0) * slope.x / maxf(squash, 0.01)).normalized()
			var n1 := Vector3(cos(t1) * slope.x, slope.y, sin(t1) * slope.x / maxf(squash, 0.01)).normalized()
			var v0 := float(i) / (n - 1)
			var v1 := float(i + 1) / (n - 1)
			st.set_normal(n0); st.set_uv(Vector2(float(s) / seg, v0)); st.add_vertex(p00)
			st.set_normal(n0); st.set_uv(Vector2(float(s) / seg, v1)); st.add_vertex(p10)
			st.set_normal(n1); st.set_uv(Vector2(float(s + 1) / seg, v1)); st.add_vertex(p11)
			st.set_normal(n0); st.set_uv(Vector2(float(s) / seg, v0)); st.add_vertex(p00)
			st.set_normal(n1); st.set_uv(Vector2(float(s + 1) / seg, v1)); st.add_vertex(p11)
			st.set_normal(n1); st.set_uv(Vector2(float(s + 1) / seg, v0)); st.add_vertex(p01)
	if cap_top:
		_cap(st, profile[n - 1], seg, squash, true)
	if cap_bottom:
		_cap(st, profile[0], seg, squash, false)
	return st.commit()

static func _cap(st: SurfaceTool, p: Vector2, seg: int, squash: float, up: bool) -> void:
	if p.x <= 0.0001:
		return
	var nr := Vector3.UP if up else Vector3.DOWN
	for s in seg:
		var t0 := TAU * s / seg
		var t1 := TAU * (s + 1) / seg
		var c := Vector3(0, p.y, 0)
		var a := Vector3(cos(t0) * p.x, p.y, sin(t0) * p.x * squash)
		var b := Vector3(cos(t1) * p.x, p.y, sin(t1) * p.x * squash)
		st.set_normal(nr); st.set_uv(Vector2(0.5, 0.5)); st.add_vertex(c)
		if up:
			st.set_normal(nr); st.set_uv(Vector2(0.5, 0.5)); st.add_vertex(b)
			st.set_normal(nr); st.set_uv(Vector2(0.5, 0.5)); st.add_vertex(a)
		else:
			st.set_normal(nr); st.set_uv(Vector2(0.5, 0.5)); st.add_vertex(a)
			st.set_normal(nr); st.set_uv(Vector2(0.5, 0.5)); st.add_vertex(b)

static func box(size: Vector3) -> BoxMesh:
	var b := BoxMesh.new()
	b.size = size
	return b

static func cyl(r_bottom: float, r_top: float, h: float, seg: int = 16) -> CylinderMesh:
	var c := CylinderMesh.new()
	c.bottom_radius = r_bottom
	c.top_radius = r_top
	c.height = h
	c.radial_segments = seg
	c.rings = 1
	return c

static func sphere(r: float, seg: int = 16, rings: int = 8) -> SphereMesh:
	var s := SphereMesh.new()
	s.radius = r
	s.height = r * 2.0
	s.radial_segments = seg
	s.rings = rings
	return s

static func torus(inner: float, outer: float, seg: int = 24) -> TorusMesh:
	var t := TorusMesh.new()
	t.inner_radius = inner
	t.outer_radius = outer
	t.rings = seg
	t.ring_segments = 8
	return t

static func quad(size: Vector2) -> QuadMesh:
	var q := QuadMesh.new()
	q.size = size
	return q

static func mat(c: Color, rough: float = 0.6, metal: float = 0.0, emit: Color = Color.BLACK, emit_k: float = 0.0) -> StandardMaterial3D:
	var m := StandardMaterial3D.new()
	m.albedo_color = c
	m.roughness = rough
	m.metallic = metal
	if emit_k > 0.0:
		m.emission_enabled = true
		m.emission = emit
		m.emission_energy_multiplier = emit_k
	return m

## Places a mesh as a child of `parent`.
static func put(parent: Node3D, mesh: Mesh, m: Material, pos: Vector3, rot_y: float = 0.0, scale: Vector3 = Vector3.ONE) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	mi.mesh = mesh
	mi.material_override = m
	mi.position = pos
	mi.rotation.y = rot_y
	mi.scale = scale
	parent.add_child(mi)
	return mi

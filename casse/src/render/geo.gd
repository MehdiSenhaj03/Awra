class_name Geo
## Procedural meshes matching the shapes (and orientations) of the original three.js geometries, cached by key so
## every model sharing a shape shares one GPU buffer.

static var _cache: Dictionary = {}

static func _cget(key: String) -> Mesh:
	return _cache.get(key) as Mesh

static func _cput(key: String, m: Mesh) -> Mesh:
	_cache[key] = m
	return m

# ---- Convex polyhedra (flat shaded) ------------------------------------------------------------------------------

static func _hull(verts: PackedVector3Array) -> ArrayMesh:
	# Faces of a convex polytope whose vertices all lie on its hull: every plane through three vertices that leaves the
	# others behind it. Coplanar vertices are gathered and fanned (pentagons of the dodecahedron).
	var n := verts.size()
	var planes: Array = []
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	for i in n:
		for j in range(i + 1, n):
			for k in range(j + 1, n):
				var nr := (verts[j] - verts[i]).cross(verts[k] - verts[i])
				if nr.length() < 1e-6:
					continue
				nr = nr.normalized()
				var d := nr.dot(verts[i])
				if d < 0.0:
					nr = -nr
					d = -d
				var ok := true
				for q in n:
					if nr.dot(verts[q]) > d + 1e-4:
						ok = false
						break
				if not ok:
					continue
				var dup := false
				for p in planes:
					if (p as Vector3).dot(nr) > 0.9999:
						dup = true
						break
				if dup:
					continue
				planes.append(nr)
				var face: Array[Vector3] = []
				for q in n:
					if absf(nr.dot(verts[q]) - d) < 1e-4:
						face.append(verts[q])
				var c := Vector3.ZERO
				for v in face:
					c += v
				c /= face.size()
				var u := (face[0] - c).normalized()
				var w := nr.cross(u)
				face.sort_custom(func(a: Vector3, b: Vector3) -> bool:
					return atan2((a - c).dot(w), (a - c).dot(u)) > atan2((b - c).dot(w), (b - c).dot(u)))
				# Clockwise seen from outside = Godot front face.
				for t in range(1, face.size() - 1):
					for v in [face[0], face[t], face[t + 1]]:
						st.set_normal(nr)
						st.add_vertex(v)
	return st.commit()

static var _verts: Dictionary = {}

## Edge lines of a polyhedron (pairs of vertices at the shortest distance: every edge of a regular solid).
static func edges(shape: String) -> Mesh:
	var key := "edges|" + shape
	var m := _cget(key)
	if m: return m
	match shape:
		"tetra": tetra()
		"octa": octa()
		"ico": ico()
		"dodeca": dodeca()
	var v: PackedVector3Array = _verts[shape]
	var dmin := 1e9
	for i in v.size():
		for j in range(i + 1, v.size()):
			dmin = minf(dmin, v[i].distance_to(v[j]))
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_LINES)
	for i in v.size():
		for j in range(i + 1, v.size()):
			if v[i].distance_to(v[j]) < dmin * 1.01:
				st.add_vertex(v[i])
				st.add_vertex(v[j])
	return _cput(key, st.commit())

static func _poly(name: String, v: PackedVector3Array) -> Mesh:
	_verts[name] = v
	return _cput(name, _hull(v))

static func tetra() -> Mesh:
	var m := _cget("tetra")
	if m: return m
	var s := 1.0 / sqrt(3.0)
	return _poly("tetra", (PackedVector3Array([Vector3(1, 1, 1) * s, Vector3(-1, -1, 1) * s, Vector3(-1, 1, -1) * s, Vector3(1, -1, -1) * s])))

static func octa() -> Mesh:
	var m := _cget("octa")
	if m: return m
	return _poly("octa", (PackedVector3Array([Vector3(1, 0, 0), Vector3(-1, 0, 0), Vector3(0, 1, 0), Vector3(0, -1, 0), Vector3(0, 0, 1), Vector3(0, 0, -1)])))

static func ico() -> Mesh:
	var m := _cget("ico")
	if m: return m
	var t := (1.0 + sqrt(5.0)) / 2.0
	var v := PackedVector3Array()
	for p in [[-1, t, 0], [1, t, 0], [-1, -t, 0], [1, -t, 0], [0, -1, t], [0, 1, t], [0, -1, -t], [0, 1, -t], [t, 0, -1], [t, 0, 1], [-t, 0, -1], [-t, 0, 1]]:
		v.append(Vector3(p[0], p[1], p[2]).normalized())
	return _poly("ico", v)

static func dodeca() -> Mesh:
	var m := _cget("dodeca")
	if m: return m
	var t := (1.0 + sqrt(5.0)) / 2.0
	var r := 1.0 / t
	var v := PackedVector3Array()
	for x in [-1, 1]:
		for y in [-1, 1]:
			for z in [-1, 1]:
				v.append(Vector3(x, y, z).normalized())
	for a in [-1, 1]:
		for b in [-1, 1]:
			v.append(Vector3(0, a * r, b * t).normalized())
			v.append(Vector3(a * r, b * t, 0).normalized())
			v.append(Vector3(a * t, 0, b * r).normalized())
	return _poly("dodeca", v)

# ---- Surfaces of revolution and the like (smooth) ----------------------------------------------------------------

static func sphere(r: float, ws: int = 20, hs: int = 14) -> Mesh:
	var key := "sphere|%s|%d|%d" % [r, ws, hs]
	var m := _cget(key)
	if m: return m
	var s := SphereMesh.new()
	s.radius = r
	s.height = r * 2.0
	s.radial_segments = ws
	s.rings = hs
	return _cput(key, s)

static func box(x: float, y: float, z: float) -> Mesh:
	var key := "box|%s|%s|%s" % [x, y, z]
	var m := _cget(key)
	if m: return m
	var b := BoxMesh.new()
	b.size = Vector3(x, y, z)
	return _cput(key, b)

## three.js CylinderGeometry: radius at +y (top) and -y (bottom), centred, optionally open and partial.
static func cylinder(rt: float, rb: float, h: float, seg: int, open: bool = false, t0: float = 0.0, tl: float = TAU) -> Mesh:
	var key := "cyl|%s|%s|%s|%d|%s|%s|%s" % [rt, rb, h, seg, open, t0, tl]
	var m := _cget(key)
	if m: return m
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var slope := (rb - rt) / h
	var ring := func(i: int, top: bool) -> Vector3:
		var th := t0 + float(i) / seg * tl
		var rr := rt if top else rb
		return Vector3(rr * sin(th), h / 2.0 if top else -h / 2.0, rr * cos(th))
	var nrm := func(i: int) -> Vector3:
		var th := t0 + float(i) / seg * tl
		return Vector3(sin(th), slope, cos(th)).normalized()
	for i in seg:
		var a: Vector3 = ring.call(i, true)
		var b: Vector3 = ring.call(i, false)
		var c: Vector3 = ring.call(i + 1, false)
		var d: Vector3 = ring.call(i + 1, true)
		var na: Vector3 = nrm.call(i)
		var nb: Vector3 = nrm.call(i + 1)
		# Outward faces, clockwise seen from outside.
		for e in [[a, na], [d, nb], [b, na], [d, nb], [c, nb], [b, na]]:
			st.set_normal(e[1])
			st.add_vertex(e[0])
	if not open:
		for top in [true, false]:
			var y := h / 2.0 if top else -h / 2.0
			var nr := Vector3(0, 1 if top else -1, 0)
			if (rt if top else rb) <= 0.0:
				continue
			for i in seg:
				var p: Vector3 = ring.call(i, top)
				var q: Vector3 = ring.call(i + 1, top)
				var tri := [Vector3(0, y, 0), q, p] if top else [Vector3(0, y, 0), p, q]
				for v in tri:
					st.set_normal(nr)
					st.add_vertex(v)
	return _cput(key, st.commit())

static func cone(r: float, h: float, seg: int) -> Mesh:
	return cylinder(0.0, r, h, seg)

## three.js TorusGeometry: in the XY plane (axis +z), optionally a partial arc starting at +x.
static func torus(R: float, tube: float, rs: int, ts: int, arc: float = TAU) -> Mesh:
	var key := "torus|%s|%s|%d|%d|%s" % [R, tube, rs, ts, arc]
	var m := _cget(key)
	if m: return m
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var pts: Array[Vector3] = []
	var nrs: Array[Vector3] = []
	for j in rs + 1:
		for i in ts + 1:
			var u := float(i) / ts * arc
			var v := float(j) / rs * TAU
			var p := Vector3((R + tube * cos(v)) * cos(u), (R + tube * cos(v)) * sin(u), tube * sin(v))
			pts.append(p)
			nrs.append((p - Vector3(R * cos(u), R * sin(u), 0)).normalized())
	for j in range(1, rs + 1):
		for i in range(1, ts + 1):
			var a := (ts + 1) * j + i - 1
			var b := (ts + 1) * (j - 1) + i - 1
			var c := (ts + 1) * (j - 1) + i
			var d := (ts + 1) * j + i
			for idx in [a, d, b, b, d, c]:
				st.set_normal(nrs[idx])
				st.add_vertex(pts[idx])
	return _cput(key, st.commit())

## three.js LatheGeometry: profile points (radius, y) revolved around +y.
static func lathe(key: String, profile: PackedVector2Array, seg: int) -> Mesh:
	var m := _cget("lathe|" + key)
	if m: return m
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	for i in seg:
		var t0 := float(i) / seg * TAU
		var t1 := float(i + 1) / seg * TAU
		for j in profile.size() - 1:
			var p0 := profile[j]
			var p1 := profile[j + 1]
			var a := Vector3(p0.x * sin(t0), p0.y, p0.x * cos(t0))
			var b := Vector3(p1.x * sin(t0), p1.y, p1.x * cos(t0))
			var c := Vector3(p1.x * sin(t1), p1.y, p1.x * cos(t1))
			var d := Vector3(p0.x * sin(t1), p0.y, p0.x * cos(t1))
			var nr := (b - a).cross(d - a).normalized()
			for v in [a, b, d, b, c, d]:
				st.set_normal(nr)
				st.add_vertex(v)
	st.generate_normals()
	return _cput("lathe|" + key, st.commit())

## three.js CircleGeometry: a disc in the XY plane facing +z.
static func circle(r: float, seg: int) -> Mesh:
	var key := "circle|%s|%d" % [r, seg]
	var m := _cget(key)
	if m: return m
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	for i in seg:
		var a := float(i) / seg * TAU
		var b := float(i + 1) / seg * TAU
		for v in [Vector3.ZERO, Vector3(r * cos(b), r * sin(b), 0), Vector3(r * cos(a), r * sin(a), 0)]:
			st.set_normal(Vector3(0, 0, 1))
			st.add_vertex(v)
	return _cput(key, st.commit())

static func quad_y(size: float) -> Mesh:
	var key := "quad|%s" % size
	var m := _cget(key)
	if m: return m
	var q := QuadMesh.new()
	q.size = Vector2(size, size)
	q.orientation = PlaneMesh.FACE_Y
	return _cput(key, q)

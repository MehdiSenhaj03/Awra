class_name Palace
extends Node3D
## The venue in 3D, built from its tile map: floor, walls (the south side is the low balustrade of the gallery the
## camera — and the sniper — look from), columns or palm beds, banquet tables or the grand staircase, statues,
## the fountain, sofas, the writing desk, paintings, windows, doors and chandeliers with their candlelight.

const H := 4.5
const ART := ["key_art", "bg_salon", "bg_serre", "bg_opera", "cine_contrat", "cine_tir", "bg_title"]

var venue: Venue
var style := 0
var chandeliers: Array[Node3D] = []
var lights: Array[OmniLight3D] = []
var water: ShaderMaterial
var _t := 0.0

var gold_m: StandardMaterial3D
var marble_m: StandardMaterial3D
var wood_m: StandardMaterial3D
var velvet_m: StandardMaterial3D
var cloth_m: StandardMaterial3D
var candle_m: StandardMaterial3D
var flame_m: StandardMaterial3D
var crystal_m: StandardMaterial3D
var leaf_m: StandardMaterial3D
var stone_m: StandardMaterial3D

func _init(v: Venue, id: String) -> void:
	venue = v
	style = {"salon": 0, "serre": 1, "opera": 2}.get(id, 0)
	gold_m = Shapes.mat(Color(0.83, 0.6, 0.25), 0.3, 0.9)
	marble_m = Shapes.mat(Color(0.86, 0.82, 0.76), 0.18)
	wood_m = Shapes.mat(Color(0.2, 0.09, 0.05), 0.4)
	velvet_m = Shapes.mat(Color(0.42, 0.03, 0.06), 0.85)
	cloth_m = Shapes.mat(Color(0.92, 0.9, 0.84), 0.7)
	candle_m = Shapes.mat(Color(0.95, 0.92, 0.82), 0.5)
	flame_m = Shapes.mat(Color(1.0, 0.75, 0.35), 1.0, 0.0, Color(1.0, 0.62, 0.25), 6.0)
	crystal_m = Shapes.mat(Color(1.0, 0.95, 0.85), 0.05, 0.0, Color(1.0, 0.85, 0.6), 1.6)
	leaf_m = Shapes.mat(Color(0.08, 0.22, 0.08), 0.6)
	stone_m = Shapes.mat(Color(0.5, 0.47, 0.42), 0.8)
	_floor()
	_walls()
	_furniture()
	_wall_things()
	_chandeliers()

func _process(dt: float) -> void:
	_t += dt
	# Candlelight breathes.
	for i in lights.size():
		var l := lights[i]
		l.light_energy = float(l.get_meta("e")) * (0.92 + 0.05 * sin(_t * 7.3 + i * 1.7) + 0.03 * sin(_t * 17.0 + i))
	if water:
		water.set_shader_parameter("t", _t)

# ---- Floor and walls ---------------------------------------------------------------------------------------------------

func _floor() -> void:
	var img := Image.create(venue.w, venue.h, false, Image.FORMAT_R8)
	for y in venue.h:
		for x in venue.w:
			img.set_pixel(x, y, Color(venue.tile(x, y) / 255.0, 0, 0))
	var sm := ShaderMaterial.new()
	sm.shader = preload("res://src/render/shaders/floor.gdshader")
	sm.set_shader_parameter("tiles", ImageTexture.create_from_image(img))
	sm.set_shader_parameter("size", Vector2(venue.w, venue.h))
	sm.set_shader_parameter("style", style)
	if style == 2:
		sm.set_shader_parameter("marble_a", Color(0.74, 0.7, 0.63))
		sm.set_shader_parameter("marble_b", Color(0.35, 0.05, 0.07))
	var pm := PlaneMesh.new()
	pm.size = Vector2(venue.w, venue.h)
	pm.subdivide_width = 0
	Shapes.put(self, pm, sm, Vector3(venue.w * 0.5, 0, venue.h * 0.5))

func _walls() -> void:
	var sm := ShaderMaterial.new()
	sm.shader = preload("res://src/render/shaders/wall.gdshader")
	sm.set_shader_parameter("style", style)
	sm.set_shader_parameter("height", H)
	if style == 2:
		sm.set_shader_parameter("silk", Color(0.36, 0.02, 0.05))
	var w := float(venue.w)
	var h := float(venue.h)
	Shapes.put(self, Shapes.box(Vector3(w, H, 1)), sm, Vector3(w * 0.5, H * 0.5, 0.5))
	Shapes.put(self, Shapes.box(Vector3(1, H, h)), sm, Vector3(0.5, H * 0.5, h * 0.5))
	Shapes.put(self, Shapes.box(Vector3(1, H, h)), sm, Vector3(w - 0.5, H * 0.5, h * 0.5))
	# The gallery's balustrade on the south side: a marble rail on turned balusters.
	var rail_m := marble_m if style != 1 else Shapes.mat(Color(0.12, 0.16, 0.14), 0.5, 0.6)
	Shapes.put(self, Shapes.box(Vector3(w, 0.12, 0.5)), rail_m, Vector3(w * 0.5, 0.06, h - 0.5))
	Shapes.put(self, Shapes.box(Vector3(w, 0.1, 0.55)), rail_m, Vector3(w * 0.5, 0.92, h - 0.5))
	var bal := Shapes.cached("baluster", func() -> Mesh:
		return Shapes.lathe(PackedVector2Array([Vector2(0.07, 0), Vector2(0.05, 0.1), Vector2(0.1, 0.3), Vector2(0.11, 0.45), Vector2(0.05, 0.6), Vector2(0.04, 0.75), Vector2(0.07, 0.87)]), 10))
	var mm := MultiMesh.new()
	mm.transform_format = MultiMesh.TRANSFORM_3D
	mm.mesh = bal
	var n := int(w / 0.35)
	mm.instance_count = n
	for i in n:
		mm.set_instance_transform(i, Transform3D(Basis(), Vector3(0.2 + i * 0.35, 0.05, h - 0.5)))
	var mmi := MultiMeshInstance3D.new()
	mmi.multimesh = mm
	mmi.material_override = rail_m
	add_child(mmi)

# ---- Furniture -----------------------------------------------------------------------------------------------------------

## Connected groups of tiles of one type: [[Vector2i...], ...]
func _clusters(t: int) -> Array:
	var seen := {}
	var out: Array = []
	for y in venue.h:
		for x in venue.w:
			if venue.tile(x, y) != t or seen.has(Vector2i(x, y)):
				continue
			var group: Array[Vector2i] = []
			var stack: Array[Vector2i] = [Vector2i(x, y)]
			seen[Vector2i(x, y)] = true
			while stack.size() > 0:
				var c: Vector2i = stack.pop_back()
				group.append(c)
				for d: Vector2i in [Vector2i(1, 0), Vector2i(-1, 0), Vector2i(0, 1), Vector2i(0, -1)]:
					var nb := c + d
					if venue.tile(nb.x, nb.y) == t and not seen.has(nb):
						seen[nb] = true
						stack.append(nb)
			out.append(group)
	return out

func _bbox(group: Array) -> Rect2:
	var mn := Vector2(1e9, 1e9)
	var mx := Vector2(-1e9, -1e9)
	for c: Vector2i in group:
		mn = mn.min(Vector2(c))
		mx = mx.max(Vector2(c) + Vector2.ONE)
	return Rect2(mn, mx - mn)

func _furniture() -> void:
	for g: Array in _clusters(3):
		var r := _bbox(g)
		if r.size.x >= 2 and r.size.y >= 2:
			_palm_bed(r)
		else:
			_column(r.get_center())
	for g: Array in _clusters(4):
		var r := _bbox(g)
		if r.size.x >= 3 and r.size.y >= 3:
			_staircase(r)
		else:
			_table(r)
	for g: Array in _clusters(5):
		_statue(_bbox(g).get_center(), g.size())
	for g: Array in _clusters(6):
		_fountain(_bbox(g))
	for g: Array in _clusters(7):
		_sofa(_bbox(g))
	for g: Array in _clusters(8):
		_desk(_bbox(g))

## A marble torchère: a short column crowned with a gilded lamp (tall columns would hide the crowd from the gallery).
func _column(c: Vector2) -> void:
	var ch := 2.3
	var shaft := Shapes.cached("column", func() -> Mesh:
		return Shapes.lathe(PackedVector2Array([Vector2(0.36, 0), Vector2(0.36, 0.16), Vector2(0.29, 0.26), Vector2(0.25, 0.36), Vector2(0.21, ch - 0.4), Vector2(0.25, ch - 0.3), Vector2(0.33, ch - 0.15), Vector2(0.36, ch)]), 20))
	var m := marble_m if style == 0 else Shapes.mat(Color(0.5, 0.1, 0.1), 0.15)
	Shapes.put(self, shaft, m, Vector3(c.x, 0, c.y))
	Shapes.put(self, Shapes.cached("col_ring", func() -> Mesh: return Shapes.torus(0.23, 0.29)), gold_m, Vector3(c.x, ch - 0.38, c.y))
	Shapes.put(self, Shapes.cached("col_ring", func() -> Mesh: return Shapes.torus(0.23, 0.29)), gold_m, Vector3(c.x, 0.3, c.y))
	var urn := Shapes.cached("urn", func() -> Mesh:
		return Shapes.lathe(PackedVector2Array([Vector2(0.1, 0), Vector2(0.06, 0.08), Vector2(0.2, 0.2), Vector2(0.24, 0.3), Vector2(0.18, 0.36)]), 16))
	Shapes.put(self, urn, gold_m, Vector3(c.x, ch, c.y))
	Shapes.put(self, Shapes.cached("globe", func() -> Mesh: return Shapes.sphere(0.17, 14, 8)), Shapes.mat(Color(1.0, 0.9, 0.75), 0.2, 0.0, Color(1.0, 0.75, 0.45), 2.5), Vector3(c.x, ch + 0.48, c.y))
	var l := OmniLight3D.new()
	l.light_color = Color(1.0, 0.74, 0.46)
	l.light_energy = 0.6
	l.omni_range = 5.0
	l.position = Vector3(c.x, ch + 0.5, c.y)
	l.set_meta("e", 0.6)
	add_child(l)
	lights.append(l)

func _palm_bed(r: Rect2) -> void:
	var c := r.get_center()
	Shapes.put(self, Shapes.box(Vector3(r.size.x - 0.1, 0.55, r.size.y - 0.1)), stone_m, Vector3(c.x, 0.275, c.y))
	Shapes.put(self, Shapes.box(Vector3(r.size.x - 0.4, 0.05, r.size.y - 0.4)), Shapes.mat(Color(0.12, 0.08, 0.05), 1.0), Vector3(c.x, 0.56, c.y))
	var trunk := Shapes.cached("palm", func() -> Mesh:
		return Shapes.lathe(PackedVector2Array([Vector2(0.2, 0), Vector2(0.16, 0.6), Vector2(0.13, 2.0), Vector2(0.11, 3.2), Vector2(0.14, 3.4)]), 10))
	Shapes.put(self, trunk, Shapes.mat(Color(0.3, 0.22, 0.14), 0.9), Vector3(c.x, 0.55, c.y))
	var frond := Shapes.cached("frond", func() -> Mesh:
		return Shapes.lathe(PackedVector2Array([Vector2(0.02, 0), Vector2(0.22, 0.4), Vector2(0.2, 1.2), Vector2(0.0, 1.9)]), 6, 0.15))
	for k in 9:
		var mi := Shapes.put(self, frond, leaf_m, Vector3(c.x, 3.9, c.y))
		mi.rotation = Vector3(0, TAU * k / 9.0 + r.position.x, 0)
		mi.rotate_object_local(Vector3.RIGHT, 1.25 + 0.2 * sin(k * 2.3))
	# Ferns at the foot.
	for k in 6:
		var mi := Shapes.put(self, frond, leaf_m, Vector3(c.x + cos(k) * 0.7, 0.55, c.y + sin(k * 1.3) * 0.7), 0, Vector3(0.6, 0.45, 0.6))
		mi.rotate_object_local(Vector3.RIGHT, 0.9)
		mi.rotate_y(k * 1.1)

func _table(r: Rect2) -> void:
	var c := r.get_center()
	var long_x := r.size.x >= r.size.y
	var sz := Vector3(r.size.x - 0.1, 0.06, r.size.y - 0.1)
	Shapes.put(self, Shapes.box(Vector3(sz.x, 0.78, sz.z)), cloth_m, Vector3(c.x, 0.39, c.y))
	Shapes.put(self, Shapes.box(Vector3(sz.x + 0.04, 0.12, sz.z + 0.04)), velvet_m, Vector3(c.x, 0.7, c.y))
	# Along the table: candelabra, fruit pyramids, a champagne tower.
	var n := int(maxf(r.size.x, r.size.y))
	for i in n:
		var p := Vector3(r.position.x + 0.5 + (i if long_x else 0), 0.78, r.position.y + 0.5 + (0 if long_x else i))
		match i % 3:
			0: _candelabra(p)
			1:
				Shapes.put(self, Shapes.cached("fruit", func() -> Mesh: return Shapes.lathe(PackedVector2Array([Vector2(0.2, 0), Vector2(0.18, 0.08), Vector2(0.12, 0.2), Vector2(0.05, 0.3), Vector2(0.0, 0.33)]), 10)), Shapes.mat(Color(0.55, 0.12, 0.15), 0.4), p)
				Shapes.put(self, Shapes.cached("plate", func() -> Mesh: return Shapes.cyl(0.24, 0.24, 0.02)), gold_m, p)
			2:
				var tower := Shapes.cached("tower", func() -> Mesh: return Shapes.lathe(PackedVector2Array([Vector2(0.22, 0), Vector2(0.22, 0.06), Vector2(0.14, 0.08), Vector2(0.14, 0.15), Vector2(0.08, 0.17), Vector2(0.08, 0.25), Vector2(0.03, 0.27), Vector2(0.03, 0.34)]), 12))
				Shapes.put(self, tower, Shapes.mat(Color(0.95, 0.85, 0.55), 0.05, 0.0, Color(1.0, 0.8, 0.4), 0.3), p)

func _candelabra(p: Vector3) -> void:
	var stem := Shapes.cached("cdl", func() -> Mesh: return Shapes.lathe(PackedVector2Array([Vector2(0.09, 0), Vector2(0.03, 0.05), Vector2(0.02, 0.35), Vector2(0.04, 0.38)]), 8))
	Shapes.put(self, stem, gold_m, p)
	for k in 3:
		var o := Vector3((k - 1) * 0.12, 0.38, 0)
		Shapes.put(self, Shapes.cached("candle", func() -> Mesh: return Shapes.cyl(0.02, 0.02, 0.14, 6)), candle_m, p + o + Vector3(0, 0.07, 0))
		Shapes.put(self, Shapes.cached("flame", func() -> Mesh: return Shapes.sphere(0.022, 6, 4)), flame_m, p + o + Vector3(0, 0.17, 0), 0, Vector3(1, 1.8, 1))

func _staircase(r: Rect2) -> void:
	# The opera's grand staircase rising north, red carpet, gilded rails.
	var steps := int(r.size.y * 3)
	for i in steps:
		var z := r.end.y - (i + 0.5) / 3.0
		var y := (i + 1) * 0.16
		Shapes.put(self, Shapes.box(Vector3(r.size.x, y, 1.0 / 3.0)), marble_m, Vector3(r.get_center().x, y * 0.5, z))
		Shapes.put(self, Shapes.box(Vector3(r.size.x * 0.5, 0.02, 1.0 / 3.0)), velvet_m, Vector3(r.get_center().x, y + 0.01, z))
	for side in [r.position.x + 0.1, r.end.x - 0.1]:
		var rail := Shapes.put(self, Shapes.box(Vector3(0.12, 0.12, r.size.y * 1.15)), gold_m, Vector3(side, steps * 0.08 + 0.9, r.get_center().y))
		rail.rotation.x = atan2(steps * 0.16, r.size.y)

func _statue(c: Vector2, _n: int) -> void:
	Shapes.put(self, Shapes.box(Vector3(0.75, 1.0, 0.75)), marble_m, Vector3(c.x, 0.5, c.y))
	Shapes.put(self, Shapes.box(Vector3(0.85, 0.1, 0.85)), marble_m, Vector3(c.x, 1.02, c.y))
	var fig := Shapes.cached("statue", func() -> Mesh:
		return Shapes.lathe(PackedVector2Array([Vector2(0.26, 0), Vector2(0.24, 0.15), Vector2(0.15, 0.6), Vector2(0.13, 0.95), Vector2(0.19, 1.15), Vector2(0.16, 1.3), Vector2(0.07, 1.36), Vector2(0.11, 1.45), Vector2(0.1, 1.56), Vector2(0.0, 1.62)]), 14, 0.7))
	var white := Shapes.mat(Color(0.95, 0.94, 0.9), 0.3)
	Shapes.put(self, fig, white, Vector3(c.x, 1.07, c.y), c.x * 0.7)
	var arm := Shapes.put(self, Shapes.cached("statue_arm", func() -> Mesh: return Shapes.cyl(0.04, 0.035, 0.6, 6)), white, Vector3(c.x + 0.12, 2.3, c.y))
	arm.rotation = Vector3(0.3, 0, -0.5)

func _fountain(r: Rect2) -> void:
	var c := r.get_center()
	var rad := minf(r.size.x, r.size.y) * 0.5
	var basin := Shapes.lathe(PackedVector2Array([Vector2(rad, 0), Vector2(rad + 0.05, 0.45), Vector2(rad - 0.15, 0.5), Vector2(rad - 0.2, 0.3)]), 32)
	Shapes.put(self, basin, marble_m, Vector3(c.x, 0, c.y))
	water = ShaderMaterial.new()
	water.shader = Shader.new()
	water.shader.code = """
shader_type spatial;
uniform float t = 0.0;
void fragment() {
	vec2 p = UV * 2.0 - 1.0;
	float r = length(p);
	float w = sin(r * 30.0 - t * 4.0) * 0.5 + 0.5;
	ALBEDO = mix(vec3(0.02, 0.06, 0.07), vec3(0.08, 0.18, 0.2), w * 0.4);
	ROUGHNESS = 0.02;
	METALLIC = 0.2;
	NORMAL_MAP = normalize(vec3(sin(p.x * 20.0 + t * 3.0) * 0.1 + 0.5, cos(p.y * 22.0 - t * 2.5) * 0.1 + 0.5, 1.0));
	EMISSION = vec3(0.02, 0.05, 0.05) * w;
}
"""
	var disc := Shapes.cyl(rad - 0.18, rad - 0.18, 0.02, 32)
	Shapes.put(self, disc, water, Vector3(c.x, 0.38, c.y))
	var tier := Shapes.lathe(PackedVector2Array([Vector2(0.18, 0), Vector2(0.14, 0.9), Vector2(0.8, 1.0), Vector2(0.85, 1.12), Vector2(0.12, 1.15), Vector2(0.1, 1.7), Vector2(0.45, 1.8), Vector2(0.45, 1.86), Vector2(0.06, 1.9), Vector2(0.05, 2.3), Vector2(0.0, 2.4)]), 24)
	Shapes.put(self, tier, marble_m, Vector3(c.x, 0.3, c.y))
	var l := OmniLight3D.new()
	l.light_color = Color(0.5, 0.8, 0.85)
	l.light_energy = 0.6
	l.omni_range = 4.0
	l.position = Vector3(c.x, 0.8, c.y)
	l.set_meta("e", 0.6)
	add_child(l)
	lights.append(l)

func _sofa(r: Rect2) -> void:
	var c := r.get_center()
	# The seat faces the open side (where its 'h' spots are).
	var face_x := 1.0 if venue.walkable(int(r.end.x), int(c.y)) else -1.0
	Shapes.put(self, Shapes.box(Vector3(r.size.x * 0.9, 0.42, r.size.y - 0.1)), velvet_m, Vector3(c.x, 0.21, c.y))
	Shapes.put(self, Shapes.box(Vector3(0.18, 0.9, r.size.y - 0.1)), velvet_m, Vector3(c.x - face_x * r.size.x * 0.4, 0.45, c.y))
	for e in [r.position.y + 0.1, r.end.y - 0.1]:
		Shapes.put(self, Shapes.box(Vector3(r.size.x * 0.9, 0.62, 0.16)), velvet_m, Vector3(c.x, 0.31, e))
	Shapes.put(self, Shapes.box(Vector3(0.08, 0.06, r.size.y)), gold_m, Vector3(c.x - face_x * r.size.x * 0.4, 0.92, c.y))

func _desk(r: Rect2) -> void:
	var c := r.get_center()
	Shapes.put(self, Shapes.box(Vector3(0.9, 0.08, 0.9)), wood_m, Vector3(c.x, 0.76, c.y))
	for k in 4:
		Shapes.put(self, Shapes.box(Vector3(0.06, 0.74, 0.06)), wood_m, Vector3(c.x + (0.38 if k % 2 == 0 else -0.38), 0.37, c.y + (0.38 if k < 2 else -0.38)))
	Shapes.put(self, Shapes.box(Vector3(0.3, 0.01, 0.4)), cloth_m, Vector3(c.x - 0.1, 0.81, c.y))
	_candelabra(Vector3(c.x + 0.25, 0.8, c.y - 0.25))
	var l := OmniLight3D.new()
	l.light_color = Color(1.0, 0.7, 0.4)
	l.light_energy = 0.8
	l.omni_range = 3.0
	l.position = Vector3(c.x, 1.4, c.y)
	l.set_meta("e", 0.8)
	add_child(l)
	lights.append(l)

# ---- On the walls --------------------------------------------------------------------------------------------------------

## Direction (dx, dy) towards the nearest wall next to a tile, or ZERO.
func _wall_dir(x: int, y: int) -> Vector2i:
	for d: Vector2i in [Vector2i(0, -1), Vector2i(-1, 0), Vector2i(1, 0), Vector2i(0, 1)]:
		if venue.tile(x + d.x, y + d.y) == 2:
			return d
	return Vector2i.ZERO

func _wall_things() -> void:
	var k := 0
	for p: Dictionary in venue.pois:
		var x: int = p.x
		var y: int = p.y
		var d := _wall_dir(x, y)
		if d == Vector2i.ZERO or d.y == 1:
			continue
		var pos := Vector3(x + 0.5 + d.x * 0.48, 0, y + 0.5 + d.y * 0.48)
		var rot := atan2(-float(d.x), -float(d.y))
		match p.kind:
			"painting":
				_painting(pos, rot, k)
				k += 1
			"window":
				_window(pos, rot)
			"entrance":
				_door(pos, rot)

func _framed(pos: Vector3, rot: float, size: Vector2, m: Material, y: float) -> void:
	var root := Node3D.new()
	root.position = pos + Vector3(0, y, 0)
	root.rotation.y = rot
	add_child(root)
	Shapes.put(root, Shapes.quad(size), m, Vector3(0, 0, 0.03))
	var fw := 0.09
	for s in [[Vector3(0, size.y * 0.5 + fw * 0.5, 0.04), Vector3(size.x + fw * 2, fw, 0.08)], [Vector3(0, -size.y * 0.5 - fw * 0.5, 0.04), Vector3(size.x + fw * 2, fw, 0.08)], [Vector3(size.x * 0.5 + fw * 0.5, 0, 0.04), Vector3(fw, size.y, 0.08)], [Vector3(-size.x * 0.5 - fw * 0.5, 0, 0.04), Vector3(fw, size.y, 0.08)]]:
		Shapes.put(root, Shapes.box(s[1]), gold_m, s[0])

func _painting(pos: Vector3, rot: float, k: int) -> void:
	var tex: Texture2D = load("res://assets/art/%s.jpg" % ART[k % ART.size()])
	var m := StandardMaterial3D.new()
	m.albedo_texture = tex
	m.roughness = 0.55
	var portrait := k % 2 == 1
	var size := Vector2(1.1, 1.5) if portrait else Vector2(1.7, 1.15)
	# A crop of the canvas with the canvas' own aspect.
	var cw := 0.42 if portrait else 0.6
	var ch := cw * (size.y / size.x) * 1.5
	m.uv1_scale = Vector3(cw, ch, 1)
	m.uv1_offset = Vector3(fmod(0.13 + k * 0.29, 1.0 - cw), fmod(0.08 + k * 0.17, maxf(0.01, 1.0 - ch)), 0)
	_framed(pos, rot, size, m, 2.4)

func _window(pos: Vector3, rot: float) -> void:
	var sm := ShaderMaterial.new()
	sm.shader = Shader.new()
	sm.shader.code = """
shader_type spatial;
render_mode unshaded;
void fragment() {
	vec2 p = UV;
	vec3 sky = mix(vec3(0.10, 0.16, 0.30), vec3(0.01, 0.02, 0.06), p.y * -1.0 + 1.0);
	sky = mix(vec3(0.02, 0.03, 0.08), vec3(0.09, 0.14, 0.28), p.y);
	float moon = smoothstep(0.09, 0.08, length((p - vec2(0.62, 0.28)) * vec2(1.0, 2.2)));
	sky += moon * vec3(0.9, 0.9, 0.8);
	float lights = step(0.86, p.y) * step(0.5, fract(p.x * 9.0)) * 0.6;
	sky += vec3(1.0, 0.6, 0.2) * lights * step(0.93, fract(sin(floor(p.x * 9.0) * 91.0) * 437.0));
	float mull = step(abs(p.x - 0.5), 0.02) + step(abs(fract(p.y * 4.0) - 0.5), 0.02);
	ALBEDO = mix(sky, vec3(0.08, 0.05, 0.03), clamp(mull, 0.0, 1.0));
}
"""
	_framed(pos, rot, Vector2(1.3, 2.7), sm, 2.15)

func _door(pos: Vector3, rot: float) -> void:
	var sm := ShaderMaterial.new()
	sm.shader = Shader.new()
	sm.shader.code = """
shader_type spatial;
render_mode unshaded;
void fragment() {
	float g = pow(1.0 - UV.y, 1.5);
	ALBEDO = mix(vec3(0.03, 0.01, 0.0), vec3(1.0, 0.62, 0.28), g * 0.85);
}
"""
	_framed(pos, rot, Vector2(1.4, 2.6), sm, 1.3)

# ---- Chandeliers ---------------------------------------------------------------------------------------------------------

func _chandeliers() -> void:
	var spots: Array[Vector2] = []
	for g: Array in _clusters(1):
		spots.append(_bbox(g).get_center())
	for f in [Vector2(0.18, 0.3), Vector2(0.82, 0.3), Vector2(0.18, 0.72), Vector2(0.82, 0.72), Vector2(0.5, 0.28), Vector2(0.5, 0.8)]:
		var p := Vector2(venue.w * f.x, venue.h * f.y)
		var ok := true
		for s in spots:
			if s.distance_to(p) < 7.0:
				ok = false
		if ok:
			spots.append(p)
	var ring := Shapes.cached("chand_ring", func() -> Mesh: return Shapes.torus(0.62, 0.7, 28))
	var ring2 := Shapes.cached("chand_ring2", func() -> Mesh: return Shapes.torus(0.32, 0.38, 20))
	var drop := Shapes.cached("chand_drop", func() -> Mesh: return Shapes.lathe(PackedVector2Array([Vector2(0.0, -0.9), Vector2(0.18, -0.55), Vector2(0.3, -0.2), Vector2(0.12, 0.0)]), 10))
	for s in spots:
		var root := Node3D.new()
		root.position = Vector3(s.x, 4.6, s.y)
		add_child(root)
		Shapes.put(root, ring, gold_m, Vector3.ZERO)
		Shapes.put(root, ring2, gold_m, Vector3(0, 0.3, 0))
		Shapes.put(root, drop, crystal_m, Vector3.ZERO)
		Shapes.put(root, Shapes.cyl(0.02, 0.02, 2.0, 4), gold_m, Vector3(0, 1.2, 0))
		for k in 10:
			var a := TAU * k / 10.0
			var o := Vector3(cos(a) * 0.66, 0.08, sin(a) * 0.66)
			Shapes.put(root, Shapes.cached("candle", func() -> Mesh: return Shapes.cyl(0.02, 0.02, 0.14, 6)), candle_m, o)
			Shapes.put(root, Shapes.cached("flame", func() -> Mesh: return Shapes.sphere(0.022, 6, 4)), flame_m, o + Vector3(0, 0.1, 0), 0, Vector3(1.3, 2.2, 1.3))
		chandeliers.append(root)
		var l := OmniLight3D.new()
		l.light_color = Color(1.0, 0.72, 0.42)
		l.light_energy = 2.2
		l.omni_range = 11.0
		l.omni_attenuation = 1.2
		l.position = Vector3(s.x, 3.6, s.y)
		l.set_meta("e", 2.2)
		add_child(l)
		lights.append(l)

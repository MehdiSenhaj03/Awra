class_name Models
## Procedural models, ported shape for shape. Everything the Silence made is obsidian with a core of light; Volta is
## a prism of pure light. Glowing parts use colours above 1 so the glow pass catches them.

const PI := 3.14159265358979

# ---- Builders ----------------------------------------------------------------------------------------------------

static func mesh(parent: Node3D, m: Mesh, mat: Material, pos: Vector3 = Vector3.ZERO, rot: Vector3 = Vector3.ZERO, scl: Vector3 = Vector3.ONE) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	mi.mesh = m
	mi.material_override = mat
	mi.rotation_order = EULER_ORDER_XYZ
	mi.position = pos
	mi.rotation = rot
	mi.scale = scl
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	mi.gi_mode = GeometryInstance3D.GI_MODE_DISABLED
	parent.add_child(mi)
	return mi

static func group(parent: Node3D, pos: Vector3 = Vector3.ZERO, rot: Vector3 = Vector3.ZERO) -> Node3D:
	var g := Node3D.new()
	g.rotation_order = EULER_ORDER_XYZ
	g.position = pos
	g.rotation = rot
	parent.add_child(g)
	return g

static func s3(s: float) -> Vector3:
	return Vector3(s, s, s)

## Obsidian whose rim glows in the foe's colour; registered for hit flashes.
static func rimmed(m: VModel, parent: Node3D, me: Mesh, color: int, tint: int = 0x0b0c14, rough: float = 0.16, pos: Vector3 = Vector3.ZERO, rot: Vector3 = Vector3.ZERO, scl: Vector3 = Vector3.ONE, ds: bool = false) -> MeshInstance3D:
	var mi := mesh(parent, me, Mats.obsidian(tint, rough, 0.92, ds), pos, rot, scl)
	mi.set_instance_shader_parameter(&"rim_color", Mats.lin3(color, 1.1))
	m.rims.append(mi)
	return mi

static func core(m: VModel, parent: Node3D, color: int, k: float, me: Mesh, name: String = "core", pos: Vector3 = Vector3.ZERO, rot: Vector3 = Vector3.ZERO, scl: Vector3 = Vector3.ONE) -> MeshInstance3D:
	var mi := mesh(parent, me, Mats.glow(color, k), pos, rot, scl)
	m.flash.append(mi)
	m.parts[name] = mi
	return mi

static func edges(parent: Node3D, shape: String, color: int, k: float, scl: Vector3) -> MeshInstance3D:
	return mesh(parent, Geo.edges(shape), Mats.glow(color, k), Vector3.ZERO, Vector3.ZERO, scl)

# ---- Volta -------------------------------------------------------------------------------------------------------

## A slender figure in black with neon seams, silver light for hair, a tuning-fork halo and a prism gauntlet.
static func player() -> VModel:
	var m := VModel.new()
	var body := group(m.root)
	var suit := Mats.glow(0x7fe9ff, 1.25)
	var dark := Mats.obsidian(0x05060c, 0.2)
	mesh(body, Geo.octa(), suit, Vector3(0, 1.35, 0), Vector3.ZERO, Vector3(0.26, 0.62, 0.2))
	var hips := mesh(body, Geo.octa(), dark, Vector3(0, 0.82, 0), Vector3.ZERO, Vector3(0.2, 0.3, 0.16))
	hips.set_instance_shader_parameter(&"rim_color", Mats.lin3(0x2ef2ff, 2.2))
	for sx in [-1.0, 1.0]:
		mesh(body, Geo.cone(0.075, 0.75, 5), suit, Vector3(sx * 0.09, 0.42, 0), Vector3(0, 0, PI))
	mesh(body, Geo.box(0.03, 1.0, 0.21), Mats.glow(0xffffff, 5), Vector3(0, 1.25, 0))
	var head := mesh(body, Geo.sphere(0.15, 16, 12), Mats.glow(0xe8f4ff, 3.2), Vector3(0, 2.08, 0))
	m.flash.append(head)
	mesh(body, Geo.cone(0.13, 0.42, 6), Mats.glow(0xffd6f4, 2.6), Vector3(-0.04, 2.05, -0.14), Vector3(-1.1, 0, 0))
	m.parts["halo"] = mesh(body, Geo.torus(0.36, 0.014, 8, 48), Mats.glow(0xffd890, 4), Vector3(0, 2.02, -0.2))
	var shards := group(body)
	for i in 3:
		var a := float(i) / 3.0 * TAU
		mesh(shards, Geo.tetra(), Mats.glow(0xff2fb9 if i == 1 else 0x2ef2ff, 3), Vector3(cos(a) * 0.7, 1.5, sin(a) * 0.7), Vector3.ZERO, s3(0.09))
	m.parts["shards"] = shards
	var aim := group(body)
	mesh(aim, Geo.octa(), Mats.glow(0xffffff, 6), Vector3(0.45, 1.4, 0), Vector3.ZERO, Vector3(0.16, 0.1, 0.1))
	mesh(aim, Geo.octa(), Mats.glow(0x2ef2ff, 3), Vector3(0.45, 1.4, 0), Vector3.ZERO, Vector3(0.26, 0.16, 0.16))
	mesh(aim, Geo.cone(0.05, 0.7, 4), Mats.glow(0xbff8ff, 3), Vector3(0.95, 1.4, 0), Vector3(0, 0, -PI / 2))
	m.parts["aim"] = aim
	m.parts["body"] = body
	return m

# ---- Foes --------------------------------------------------------------------------------------------------------

static func _crown(m: VModel, r: float, y: float) -> void:
	var g := group(m.root)
	for i in 5:
		var a := float(i) / 5.0 * TAU
		mesh(g, Geo.cone(0.08, 0.35, 4), Mats.glow(0xffc34a, 4), Vector3(cos(a) * r * 0.55, y, sin(a) * r * 0.55))
	m.parts["crown"] = g

static func _mote(m: VModel, r: float, color: int) -> void:
	var g := group(m.root, Vector3(0, 0.7, 0))
	rimmed(m, g, Geo.tetra(), color, 0x0b0c14, 0.16, Vector3.ZERO, Vector3.ZERO, s3(r * 1.15))
	core(m, g, color, 4, Geo.octa(), "core", Vector3.ZERO, Vector3.ZERO, s3(r * 0.42))
	edges(g, "tetra", color, 2, s3(r * 1.16))
	m.parts["spin"] = g

static func _spitter(m: VModel, r: float, color: int) -> void:
	var g := group(m.root, Vector3(0, 0.9, 0))
	for i in 4:
		var a := float(i) / 4.0 * TAU + PI / 4
		rimmed(m, g, Geo.tetra(), color, 0x10111c, 0.16, Vector3(-r * 0.3, sin(a) * r * 0.8, cos(a) * r * 0.8), Vector3.ZERO, Vector3(r * 0.5, r * 0.25, r * 0.12))
	rimmed(m, g, Geo.octa(), color, 0x0b0c14, 0.16, Vector3.ZERO, Vector3.ZERO, Vector3(r, r * 0.8, r))
	core(m, g, color, 4, Geo.torus(0.42, 0.09, 8, 24), "core", Vector3(r * 0.75, 0, 0), Vector3(0, PI / 2, 0), s3(r))
	edges(g, "octa", color, 1.6, Vector3(r * 1.01, r * 0.81, r * 1.01))
	m.parts["face"] = g

static func _spinner(m: VModel, r: float, color: int) -> void:
	var g := group(m.root, Vector3(0, 0.8, 0))
	for i in 6:
		var a := float(i) / 6.0 * TAU
		rimmed(m, g, Geo.cone(r * 0.16, r * 0.7, 4), color, 0x12121e, 0.16, Vector3(cos(a) * r * 1.3, 0, sin(a) * r * 1.3), Vector3(0, -a, -PI / 2))
	rimmed(m, g, Geo.torus(r, r * 0.28, 12, 48), color, 0x0b0c14, 0.16, Vector3.ZERO, Vector3(PI / 2, 0, 0))
	core(m, g, color, 4, Geo.torus(r * 0.62, r * 0.07, 8, 48), "core", Vector3.ZERO, Vector3(PI / 2, 0, 0))
	core(m, g, 0xffffff, 3, Geo.sphere(1.0), "eye", Vector3.ZERO, Vector3.ZERO, s3(r * 0.22))
	m.parts["spin"] = g

static func _lancer(m: VModel, r: float, color: int) -> void:
	var g := group(m.root, Vector3(0, 0.8, 0))
	rimmed(m, g, Geo.octa(), color, 0x0b0c14, 0.16, Vector3.ZERO, Vector3.ZERO, Vector3(r * 2.1, r * 0.5, r * 0.65))
	edges(g, "octa", color, 3, Vector3(r * 2.12, r * 0.51, r * 0.66))
	core(m, g, color, 4, Geo.cone(r * 0.22, r * 1.1, 4), "core", Vector3(r * 1.7, 0, 0), Vector3(0, 0, -PI / 2))
	m.parts["face"] = g

static func _splitter(m: VModel, r: float, color: int) -> void:
	var g := group(m.root, Vector3(0, 0.9, 0))
	rimmed(m, g, Geo.ico(), color, 0x0a1218, 0.1, Vector3.ZERO, Vector3.ZERO, s3(r))
	edges(g, "ico", color, 3, s3(r * 1.01))
	core(m, g, color, 3.5, Geo.ico(), "core", Vector3.ZERO, Vector3.ZERO, s3(r * 0.45))
	m.parts["spin"] = g

static func _bulwark(m: VModel, r: float, color: int) -> void:
	var g := group(m.root)
	rimmed(m, g, Geo.cylinder(r * 0.75, r * 0.9, r * 1.6, 6), color, 0x0b0c14, 0.16, Vector3(0, r * 0.8, 0))
	core(m, g, color, 3, Geo.cylinder(r * 0.3, r * 0.3, r * 1.62, 6), "core", Vector3(0, r * 0.8, 0))
	var sh := group(g, Vector3.ZERO, Vector3(0, PI / 2, 0))
	rimmed(m, sh, Geo.cylinder(r * 1.45, r * 1.45, r * 1.5, 24, true, -1.05, 2.1), color, 0x161208, 0.1, Vector3(0, r * 0.8, 0), Vector3.ZERO, Vector3.ONE, true)
	var lip := mesh(sh, Geo.torus(r * 1.45, 0.05, 6, 24, 2.1), Mats.glow(0xffc34a, 4), Vector3(0, r * 1.55, 0), Vector3(PI / 2, 0, -1.05))
	m.flash.append(lip)
	m.parts["shield"] = sh
	m.parts["face"] = g

static func _mortar(m: VModel, r: float, color: int) -> void:
	var g := group(m.root)
	var pts := PackedVector2Array()
	for i in 13:
		var t := float(i) / 12.0
		pts.append(Vector2(r * (0.35 + 0.75 * pow(t, 2.2)), r * 1.6 * (1.0 - t)))
	rimmed(m, g, Geo.lathe("mortar%s" % r, pts, 24), color, 0x0b0c14, 0.16, Vector3.ZERO, Vector3.ZERO, Vector3.ONE, true)
	core(m, g, color, 4, Geo.circle(r * 1.05, 24), "core", Vector3(0, 0.05, 0), Vector3(-PI / 2, 0, 0))
	core(m, g, color, 3, Geo.sphere(1.0), "top", Vector3(0, r * 1.75, 0), Vector3.ZERO, s3(r * 0.3))
	m.parts["spin"] = g

static func _sniper(m: VModel, r: float, color: int) -> void:
	var g := group(m.root, Vector3(0, 1.1, 0))
	rimmed(m, g, Geo.sphere(1.0), color, 0x0b0c14, 0.16, Vector3.ZERO, Vector3.ZERO, s3(r))
	core(m, g, color, 5, Geo.torus(r * 0.5, r * 0.08, 8, 32), "core", Vector3(r * 0.86, 0, 0), Vector3(0, PI / 2, 0))
	mesh(g, Geo.sphere(1.0), Mats.glow(0xffffff, 6), Vector3(r * 0.9, 0, 0), Vector3.ZERO, s3(r * 0.22))
	m.parts["ring"] = mesh(g, Geo.torus(r * 1.4, 0.03, 6, 48), Mats.glow(color, 2), Vector3.ZERO, Vector3(PI / 2, 0, 0))
	m.parts["face"] = g

static func _mirror(m: VModel, r: float, color: int) -> void:
	var g := group(m.root, Vector3(0, 1.1, 0))
	var mat := Mats.obsidian(0xffffff, 0.02, 1.0)
	for i in 2:
		mesh(g, Geo.octa(), mat, Vector3(0, 0, (i - 0.5) * r * 0.5), Vector3((i - 0.5) * 0.5, 0, 0), Vector3(r * 0.15, r * 1.2, r * 0.8))
	core(m, g, color, 3, Geo.octa(), "core", Vector3(r * 0.1, 0, 0), Vector3.ZERO, Vector3(r * 0.12, r * 0.5, r * 0.3))
	m.parts["face"] = g

static func _phantom(m: VModel, r: float, color: int) -> void:
	var g := group(m.root, Vector3(0, 1.0, 0))
	var mat := Mats.obsidian_fade(0x140a24, 0.2, 0.8)
	for i in 6:
		var a := float(i) / 6.0 * TAU
		m.fade.append(mesh(g, Geo.tetra(), mat, Vector3(cos(a) * r * 0.6, sin(i * 1.7) * r * 0.4, sin(a) * r * 0.6), Vector3(i, i * 2, i * 3), s3(r * 0.45)))
	var c := mesh(g, Geo.ico(), Mats.glow_fade(color, 4), Vector3.ZERO, Vector3.ZERO, s3(r * 0.35))
	m.flash.append(c)
	m.fade.append(c)
	m.parts["core"] = c
	m.parts["spin"] = g

static func _choir(m: VModel, r: float, color: int) -> void:
	var g := group(m.root, Vector3(0, 0.6, 0))
	for i in 3:
		rimmed(m, g, Geo.torus(r * (0.9 - i * 0.2), 0.08, 8, 32), color, 0x0b0c14, 0.16, Vector3(0, i * 0.4, 0), Vector3(PI / 2, 0, 0))
	core(m, g, color, 4, Geo.sphere(1.0), "core", Vector3(0, 0.4, 0), Vector3.ZERO, s3(r * 0.35))
	m.parts["spin"] = g

static func _weaver(m: VModel, r: float, color: int) -> void:
	var g := group(m.root, Vector3(0, 0.9, 0))
	for i in 4:
		var holder := group(g, Vector3.ZERO, Vector3(0, float(i) / 4.0 * TAU, 0))
		rimmed(m, holder, Geo.box(1, 1, 1), color, 0x101420, 0.16, Vector3(r * 0.9, 0, 0), Vector3.ZERO, Vector3(r * 1.6, 0.12, 0.22))
	rimmed(m, g, Geo.dodeca(), color, 0x0b0c14, 0.16, Vector3.ZERO, Vector3.ZERO, s3(r * 0.8))
	core(m, g, color, 4, Geo.ico(), "core", Vector3.ZERO, Vector3.ZERO, s3(r * 0.35))
	m.parts["spin"] = g

static func _echo(m: VModel, r: float, color: int) -> void:
	var g := group(m.root, Vector3(0, 1.1, 0))
	core(m, g, color, 5, Geo.dodeca(), "core", Vector3.ZERO, Vector3.ZERO, s3(r * 0.7))
	edges(g, "dodeca", 0xffffff, 4, s3(r * 0.95))
	m.parts["spin"] = g

# ---- Conductors ----------------------------------------------------------------------------------------------------

static func _rim_b(m: VModel, parent: Node3D, me: Mesh, color: int, tint: int = 0x0a0b12, rough: float = 0.14, pos: Vector3 = Vector3.ZERO, rot: Vector3 = Vector3.ZERO, scl: Vector3 = Vector3.ONE) -> MeshInstance3D:
	return rimmed(m, parent, me, color, tint, rough, pos, rot, scl)

static func _ostinato(m: VModel, r: float, color: int) -> void:
	var g := group(m.root, Vector3(0, 2.2, 0))
	core(m, g, color, 4, Geo.octa(), "heart", Vector3.ZERO, Vector3.ZERO, Vector3(r * 0.32, r * 0.55, r * 0.32))
	mesh(g, Geo.octa(), Mats.glow(0xffffff, 6), Vector3.ZERO, Vector3.ZERO, Vector3(r * 0.12, r * 0.25, r * 0.12))
	var rings: Array[Node3D] = []
	for i in 3:
		var rg := group(g)
		var R := r * (0.8 + i * 0.32)
		_rim_b(m, rg, Geo.torus(R, r * 0.07, 10, 96), color)
		mesh(rg, Geo.torus(R, r * 0.012, 6, 96), Mats.glow(0xffc34a if i == 1 else color, 3), Vector3(0, 0, r * 0.07))
		if i == 2:
			for k in 12:
				var a := float(k) / 12.0 * TAU
				var h := r * (0.4 + (k % 3) * 0.2)
				var dir := Vector3(cos(a), sin(a), 0)
				_rim_b(m, rg, Geo.cylinder(r * 0.05, r * 0.06, h, 6), color, 0x0c0c14, 0.14, dir * (R + h / 2.0), Vector3(0, 0, a - PI / 2))
				mesh(rg, Geo.octa(), Mats.glow(color, 5), dir * (R + h + r * 0.06), Vector3.ZERO, Vector3(r * 0.04, r * 0.09, r * 0.04))
		rings.append(rg)
	m.lists["rings"] = rings
	m.parts["body"] = g

static func _crescendo(m: VModel, r: float, color: int) -> void:
	var g := group(m.root)
	_rim_b(m, g, Geo.cylinder(r * 0.35, r * 0.75, r * 2.2, 7), color, 0x0a0b12, 0.14, Vector3(0, r * 1.1, 0))
	var branches := group(g)
	for i in 9:
		var a := float(i) / 9.0 * TAU
		var ln := r * (1.1 + (i % 3) * 0.45)
		var pos := Vector3(cos(a) * r * 0.5, r * 2.2 + ln * 0.3, sin(a) * r * 0.5)
		var rot := Vector3(sin(a) * 0.8, 0, cos(a) * -0.8)
		if i % 2 == 1:
			_rim_b(m, branches, Geo.cone(r * 0.13, ln, 5), color, 0x0a140f, 0.14, pos, rot)
		else:
			mesh(branches, Geo.cone(r * 0.13, ln, 5), Mats.glow_fade(color, 2.2, 0.85), pos, rot)
	m.parts["spin"] = branches
	core(m, g, color, 4, Geo.ico(), "heart", Vector3(0, r * 2.2, 0), Vector3.ZERO, s3(r * 0.4))
	for i in 7:
		var a := float(i) / 7.0 * TAU + 0.3
		mesh(g, Geo.cone(r * 0.12, r * 1.6, 4), Mats.glow_fade(color, 1.4, 0.85), Vector3(cos(a) * r * 0.9, r * 0.15, sin(a) * r * 0.9), Vector3(0, -a, PI / 2))
	m.parts["body"] = g

static func _crystal(m: VModel, r: float, color: int) -> void:
	var g := group(m.root)
	for i in 4:
		core(m, g, color, 2.4, Geo.octa(), "c%d" % i, Vector3(cos(i * 2.1) * r * 0.35, r * 0.5, sin(i * 2.1) * r * 0.35), Vector3(cos(i) * 0.4, i, sin(i) * 0.4), Vector3(r * 0.28, r * (1.1 - i * 0.18), r * 0.28))
	m.parts["body"] = g

static func _fugue(m: VModel, r: float, color: int) -> void:
	var g := group(m.root)
	for i in 4:
		var cp := Vector3((i - 1.5) * r * 0.3, r * 2.5 + (i % 2) * r * 0.2, -r * 0.15)
		_rim_b(m, g, Geo.cylinder(r * 0.07, r * 0.09, r * 0.6, 6), color, 0x100c08, 0.14, cp)
		mesh(g, Geo.octa(), Mats.glow(0xff60c0, 5), cp + Vector3(0, r * 0.38, 0), Vector3.ZERO, Vector3(r * 0.06, r * 0.12, r * 0.06))
	var arms: Array[Node3D] = []
	for s in [-1.0, 1.0]:
		var arm := group(g, Vector3(s * r * 0.95, r * 2.0, 0))
		_rim_b(m, arm, Geo.octa(), color, 0x0a0b12, 0.14, Vector3(0, -r * 0.6, 0), Vector3.ZERO, Vector3(r * 0.28, r * 0.75, r * 0.28))
		core(m, arm, color, 3, Geo.sphere(1.0), "fist%d" % int(s), Vector3(0, -r * 1.3, 0), Vector3.ZERO, s3(r * 0.22))
		arms.append(arm)
	for s in [-1.0, 1.0]:
		_rim_b(m, g, Geo.octa(), color, 0x0a0b12, 0.14, Vector3(s * r * 0.4, r * 0.6, 0), Vector3.ZERO, Vector3(r * 0.3, r * 0.65, r * 0.3))
	_rim_b(m, g, Geo.octa(), color, 0x0a0b12, 0.14, Vector3(0, r * 1.6, 0), Vector3.ZERO, Vector3(r * 0.75, r * 1.0, r * 0.6))
	core(m, g, color, 5, Geo.octa(), "heart", Vector3(0, r * 1.6, r * 0.42), Vector3.ZERO, Vector3(r * 0.28, r * 0.36, r * 0.2))
	_rim_b(m, g, Geo.cone(r * 0.35, r * 0.7, 5), color, 0x0a0b12, 0.14, Vector3(0, r * 2.75, 0))
	m.lists["arms"] = arms
	m.parts["body"] = g
	m.parts["face"] = g

static func _chrome(m: VModel, parent: Node3D, me: Mesh, pos: Vector3, rot: Vector3, scl: Vector3, fade: bool) -> MeshInstance3D:
	var mat: Material = Mats.obsidian_fade(0xc8ccd8, 0.04, 1.0) if fade else Mats.obsidian(0xc8ccd8, 0.04, 1.0)
	var mi := mesh(parent, me, mat, pos, rot, scl)
	mi.set_instance_shader_parameter(&"rim_color", Mats.lin3(0x9b5cff, 0.8))
	if fade:
		m.fade.append(mi)
	return mi

static func _rubato(m: VModel, r: float, color: int) -> void:
	var g := group(m.root, Vector3(0, 1.3, 0))
	for s in [-1.0, 1.0]:
		_chrome(m, g, Geo.box(r * 1.6, r * 0.18, r * 0.18), Vector3(-r * 0.6, r * 0.55, s * r * 0.45), Vector3(0, 0, 0.35), Vector3.ONE, true)
	for s in [-1.0, 1.0]:
		var eye := mesh(g, Geo.sphere(1.0), Mats.glow_fade(color, 6), Vector3(r * 0.85, r * 0.2, s * r * 0.4), Vector3.ZERO, s3(r * 0.16))
		m.flash.append(eye)
		m.fade.append(eye)
	var head := _chrome(m, g, Geo.sphere(r, 24, 16), Vector3.ZERO, Vector3.ZERO, Vector3(1.3, 0.85, 1), true)
	m.rims.append(head)
	m.parts["face"] = g

static func _segment(m: VModel, r: float, color: int) -> void:
	var g := group(m.root, Vector3(0, 1.2, 0))
	var ring := _chrome(m, g, Geo.sphere(r, 20, 14), Vector3.ZERO, Vector3.ZERO, Vector3(0.9, 0.75, 0.9), false)
	m.rims.append(ring)
	core(m, g, color, 3, Geo.torus(r * 0.92, r * 0.06, 6, 32), "band", Vector3.ZERO, Vector3(0, PI / 2, 0))
	m.parts["face"] = g

static func _staccato(m: VModel, r: float, color: int) -> void:
	var g := group(m.root)
	for i in 4:
		var h := r * (1.4 - i * 0.22)
		var p := Vector3(0, r * 0.7 + i * r * 1.05, 0)
		_rim_b(m, g, Geo.box(r * (1.1 - i * 0.18), h, r * (0.9 - i * 0.15)), color, 0x0a0b12, 0.14, p, Vector3(0, i * 0.35, 0))
		mesh(g, Geo.box(r * (1.12 - i * 0.18), h * 0.06, r * (0.92 - i * 0.15)), Mats.glow(color, 2.5), p, Vector3(0, i * 0.35, 0))
	var blades: Array[Node3D] = []
	for s in [-1.0, 1.0]:
		blades.append(mesh(g, Geo.box(r * 0.08, r * 3, r * 0.5), Mats.glow(color, 4), Vector3(s * r * 1.1, r * 1.8, 0), Vector3(0, 0, s * 0.35)))
	core(m, g, color, 5, Geo.octa(), "heart", Vector3(0, r * 4.6, 0), Vector3.ZERO, Vector3(r * 0.25, r * 0.4, r * 0.25))
	m.lists["blades"] = blades
	m.parts["body"] = g

static func _fermata(m: VModel, r: float, _color: int) -> void:
	var g := group(m.root, Vector3(0, 3, 0))
	mesh(g, Geo.sphere(r, 48, 32), Mats.glow(0x000000, 1))
	var halo := mesh(g, Geo.torus(r * 1.04, r * 0.05, 12, 96), Mats.glow(0xffffff, 6), Vector3.ZERO, Vector3(PI / 2.4, 0, 0))
	m.flash.append(halo)
	mesh(g, Geo.torus(r * 1.12, r * 0.14, 12, 96), Mats.glow_fade(0xffd890, 1.4, 0.6), Vector3.ZERO, Vector3(PI / 2.4, 0, 0))
	m.parts["arc"] = _rim_b(m, g, Geo.torus(r * 1.9, r * 0.09, 8, 64, PI * 0.85), 0xffc34a, 0x14100a, 0.14, Vector3(0, r * 0.2, 0), Vector3(0, 0, PI * 0.075))
	mesh(g, Geo.torus(r * 1.9, r * 0.025, 6, 64, PI * 0.85), Mats.glow(0xffc34a, 5), Vector3(0, r * 0.2, 0), Vector3(0, 0, PI * 0.075))
	# The orbiting debris: two multimeshes (stone and light), one draw each.
	var orbit := group(g)
	var stone := MultiMesh.new()
	stone.transform_format = MultiMesh.TRANSFORM_3D
	stone.mesh = Geo.tetra()
	var lit := MultiMesh.new()
	lit.transform_format = MultiMesh.TRANSFORM_3D
	lit.mesh = Geo.tetra()
	var ts: Array[Transform3D] = []
	var tl: Array[Transform3D] = []
	for i in 40:
		var a := float(i) / 40.0 * TAU
		var d := r * (2.2 + (i % 5) * 0.25)
		var t := Transform3D(Basis.from_scale(s3(r * (0.05 + (i % 4) * 0.03))), Vector3(cos(a) * d, sin(i * 1.7) * r * 0.4, sin(a) * d))
		if i % 3 != 0:
			ts.append(t)
		else:
			tl.append(t)
	stone.instance_count = ts.size()
	for i in ts.size():
		stone.set_instance_transform(i, ts[i])
	lit.instance_count = tl.size()
	for i in tl.size():
		lit.set_instance_transform(i, tl[i])
	var ms := MultiMeshInstance3D.new()
	ms.multimesh = stone
	ms.material_override = Mats.obsidian(0x0a0a10, 0.14)
	ms.set_instance_shader_parameter(&"rim_color", Mats.lin3(0xffffff, 1.1))
	ms.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	orbit.add_child(ms)
	m.rims.append(ms)
	var ml := MultiMeshInstance3D.new()
	ml.multimesh = lit
	ml.material_override = Mats.glow(0xffe0b0, 3)
	ml.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	orbit.add_child(ml)
	m.parts["spin"] = orbit
	m.parts["body"] = g

# ---- Entry points --------------------------------------------------------------------------------------------------

const LOOKS := ["mote", "spitter", "spinner", "lancer", "splitter", "bulwark", "mortar", "sniper", "mirror", "phantom", "choir", "weaver", "echo", "ostinato", "crescendo", "crystal", "fugue", "rubato", "segment", "staccato", "fermata"]

static func enemy(look: String, r: float, color: int, elite: bool) -> VModel:
	var m := VModel.new()
	var fn := "_" + look
	if not LOOKS.has(look):
		fn = "_mote"
	match fn:
		"_mote": _mote(m, r, color)
		"_spitter": _spitter(m, r, color)
		"_spinner": _spinner(m, r, color)
		"_lancer": _lancer(m, r, color)
		"_splitter": _splitter(m, r, color)
		"_bulwark": _bulwark(m, r, color)
		"_mortar": _mortar(m, r, color)
		"_sniper": _sniper(m, r, color)
		"_mirror": _mirror(m, r, color)
		"_phantom": _phantom(m, r, color)
		"_choir": _choir(m, r, color)
		"_weaver": _weaver(m, r, color)
		"_echo": _echo(m, r, color)
		"_ostinato": _ostinato(m, r, color)
		"_crescendo": _crescendo(m, r, color)
		"_crystal": _crystal(m, r, color)
		"_fugue": _fugue(m, r, color)
		"_rubato": _rubato(m, r, color)
		"_segment": _segment(m, r, color)
		"_staccato": _staccato(m, r, color)
		"_fermata": _fermata(m, r, color)
	if elite:
		_crown(m, r, r * 2.0 + 0.6)
	return m

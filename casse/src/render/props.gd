class_name Props
## Procedural models for the heist: Odile's echoes, guards, doors and every device. Shared materials; per-object
## state through instance uniforms; all shapes kept simple and readable from above.

const PI := 3.14159265358979
const ECHO_COLORS := [0x2ef2ff, 0xff2fb9, 0xffb43a, 0x40ffb0, 0x9b5cff, 0xfff4e8]
const CHAN_COLORS := [0xffffff, 0x2ef2ff, 0xff2fb9, 0xffb43a, 0x40ffb0, 0x9b5cff, 0xfff4e8, 0xff4060, 0x5c8cff, 0xc8ff40]
const CARD_COLORS := [0xff4060, 0x5c8cff, 0x40ffb0, 0xffc34a]

static func mesh(parent: Node3D, m: Mesh, mat: Material, pos: Vector3 = Vector3.ZERO, rot: Vector3 = Vector3.ZERO, scl: Vector3 = Vector3.ONE) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	mi.mesh = m
	mi.material_override = mat
	mi.position = pos
	mi.rotation = rot
	mi.scale = scl
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	mi.gi_mode = GeometryInstance3D.GI_MODE_DISABLED
	parent.add_child(mi)
	return mi

static func node(parent: Node3D, pos: Vector3 = Vector3.ZERO) -> Node3D:
	var n := Node3D.new()
	n.position = pos
	parent.add_child(n)
	return n

static func rim(mi: GeometryInstance3D, color: int, k: float = 1.2) -> GeometryInstance3D:
	mi.set_instance_shader_parameter(&"rim_color", Mats.lin3(color, k))
	return mi

## Odile (or one of her echoes): long coat, scarf, short hair; the live one is solid, past echoes are light.
static func figure(color: int, ghost: bool) -> Node3D:
	var root := Node3D.new()
	var body := node(root)
	body.name = "body"
	var coat: Material
	var dark: Material
	if ghost:
		coat = Mats.glow_fade(color, 1.6, 0.55)
		dark = Mats.glow_fade(color, 0.9, 0.55)
	else:
		coat = Mats.obsidian(0x22242c, 0.35, 0.3)
		dark = Mats.obsidian(0x0c0d12, 0.3, 0.4)
	var c := mesh(body, Geo.cylinder(0.13, 0.27, 0.62, 10), coat, Vector3(0, 0.42, 0))
	var t := mesh(body, Geo.cylinder(0.14, 0.13, 0.28, 10), coat, Vector3(0, 0.84, 0))
	if not ghost:
		rim(c, color, 1.8)
		rim(t, color, 1.8)
	for s in [-1.0, 1.0]:
		mesh(body, Geo.box(0.07, 0.18, 0.07), dark, Vector3(s * 0.07, 0.09, 0))
	var head := mesh(body, Geo.sphere(0.11, 12, 8), Mats.glow(0xf2d8c4, 0.9) if not ghost else dark, Vector3(0, 1.08, 0))
	head.name = "head"
	mesh(body, Geo.sphere(0.125, 12, 8), dark if ghost else Mats.obsidian(0x0a0c18, 0.25, 0.5), Vector3(0, 1.12, -0.02), Vector3.ZERO, Vector3(1.0, 0.75, 1.05))
	# Scarf and the glowing watch.
	mesh(body, Geo.torus(0.12, 0.035, 6, 14), Mats.glow(0x8a1020, 1.2) if not ghost else coat, Vector3(0, 0.97, 0), Vector3(PI / 2, 0, 0))
	mesh(body, Geo.sphere(0.04, 8, 6), Mats.glow(0x2ef2ff, 6.0), Vector3(0.17, 0.62, 0.05))
	# Light under the feet: the echo's colour.
	return root

static func guard() -> Node3D:
	var root := Node3D.new()
	var body := node(root)
	body.name = "body"
	var uni := Mats.obsidian(0x1a2236, 0.4, 0.2)
	rim(mesh(body, Geo.cylinder(0.17, 0.22, 0.6, 10), uni, Vector3(0, 0.45, 0)), 0x5c8cff, 0.6)
	rim(mesh(body, Geo.cylinder(0.2, 0.17, 0.25, 10), uni, Vector3(0, 0.85, 0)), 0x5c8cff, 0.6)
	for s in [-1.0, 1.0]:
		mesh(body, Geo.box(0.09, 0.16, 0.09), Mats.obsidian(0x0a0b10, 0.3, 0.4), Vector3(s * 0.08, 0.08, 0))
	mesh(body, Geo.sphere(0.12, 12, 8), Mats.glow(0xd9b8a0, 0.7), Vector3(0, 1.1, 0))
	# Kepi: dark crown, brass badge, visor.
	mesh(body, Geo.cylinder(0.12, 0.13, 0.1, 12), Mats.obsidian(0x10141f, 0.3, 0.4), Vector3(0, 1.22, 0))
	mesh(body, Geo.box(0.16, 0.02, 0.1), Mats.obsidian(0x05060a, 0.2, 0.6), Vector3(0.1, 1.18, 0))
	mesh(body, Geo.sphere(0.025, 6, 4), Mats.glow(0xffc34a, 3.0), Vector3(0.12, 1.22, 0))
	# Flashlight held forward.
	mesh(body, Geo.cylinder(0.035, 0.035, 0.2, 8), Mats.obsidian(0x30333a, 0.3, 0.8), Vector3(0.25, 0.75, 0.08), Vector3(0, 0, PI / 2))
	mesh(body, Geo.sphere(0.04, 8, 6), Mats.glow(0xfff2c0, 6.0), Vector3(0.36, 0.75, 0.08))
	return root

static func door(kind: String, ch: int, color: int, along_x: bool) -> Node3D:
	var root := Node3D.new()
	var panel := node(root)
	panel.name = "panel"
	var col: int
	match kind:
		"chan": col = CHAN_COLORS[ch]
		"key": col = CARD_COLORS[color]
		"vault": col = 0xffc34a
		"time": col = 0xfff4e8
		_: col = 0x8a6a40
	var sx := 1.0 if along_x else 0.16
	var sz := 0.16 if along_x else 1.0
	if kind == "door":
		mesh(panel, Geo.box(sx * 0.98, 1.0, sz * 0.98), Mats.obsidian(0x2a1a10, 0.45, 0.2), Vector3(0, 0.5, 0))
		mesh(panel, Geo.box(sx * 0.7 if along_x else sx * 1.05, 0.04, sz * 1.05 if along_x else sz * 0.7), Mats.glow(0xffc34a, 1.5), Vector3(0, 0.55, 0))
	elif kind == "vault":
		mesh(panel, Geo.box(sx, 1.05, sz * 1.6 if along_x else sz), Mats.obsidian(0x3a2c14, 0.25, 0.9), Vector3(0, 0.52, 0))
		var ring := mesh(panel, Geo.torus(0.32, 0.05, 8, 24), Mats.glow(col, 2.5), Vector3(0, 0.55, 0), Vector3(0, 0 if along_x else PI / 2, 0))
		ring.name = "ring"
	else:
		rim(mesh(panel, Geo.box(sx * 0.98, 1.0, sz * 0.98), Mats.obsidian(0x161a24, 0.2, 0.8), Vector3(0, 0.5, 0)), col, 1.0)
		var stripe := mesh(panel, Geo.box(sx * 1.02, 0.08, sz * 1.02), Mats.glow(col, 3.0), Vector3(0, 0.7, 0))
		stripe.name = "stripe"
	return root

static func plate(ch: int) -> Node3D:
	var root := Node3D.new()
	var top := mesh(root, Geo.cylinder(0.34, 0.38, 0.06, 20), Mats.obsidian(0x2a2416, 0.3, 0.9), Vector3(0, 0.03, 0))
	top.name = "top"
	mesh(root, Geo.torus(0.36, 0.02, 6, 28), Mats.glow(CHAN_COLORS[ch], 2.0), Vector3(0, 0.05, 0), Vector3(PI / 2, 0, 0))
	return root

static func pedestal(parent: Node3D) -> void:
	mesh(parent, Geo.cylinder(0.14, 0.2, 0.7, 8), Mats.obsidian(0x14161e, 0.3, 0.6), Vector3(0, 0.35, 0))
	mesh(parent, Geo.cylinder(0.17, 0.17, 0.04, 8), Mats.obsidian(0x5a4520, 0.3, 0.9), Vector3(0, 0.72, 0))

static func button(ch: int) -> Node3D:
	var root := Node3D.new()
	pedestal(root)
	var b := mesh(root, Geo.sphere(0.09, 12, 6), Mats.glow(CHAN_COLORS[ch], 2.5), Vector3(0, 0.76, 0), Vector3.ZERO, Vector3(1, 0.6, 1))
	b.name = "cap"
	return root

static func lever(ch: int, vault: bool) -> Node3D:
	var root := Node3D.new()
	var col: int = 0xffc34a if vault else CHAN_COLORS[ch]
	mesh(root, Geo.box(0.36, 0.5 if vault else 0.35, 0.22), Mats.obsidian(0x1a1c24, 0.3, 0.6), Vector3(0, 0.25 if vault else 0.17, 0))
	var arm := node(root, Vector3(0, 0.5 if vault else 0.35, 0))
	arm.name = "arm"
	mesh(arm, Geo.cylinder(0.025, 0.025, 0.45, 6), Mats.obsidian(0x6a6a70, 0.3, 0.9), Vector3(0, 0.22, 0))
	mesh(arm, Geo.sphere(0.07, 10, 6), Mats.glow(col, 3.0), Vector3(0, 0.46, 0))
	return root

static func terminal(ch: int) -> Node3D:
	var root := Node3D.new()
	mesh(root, Geo.box(0.6, 0.7, 0.45), Mats.obsidian(0x14161e, 0.3, 0.6), Vector3(0, 0.35, 0))
	var scr := mesh(root, Geo.box(0.5, 0.32, 0.03), Mats.glow(CHAN_COLORS[ch], 1.6), Vector3(0, 0.95, -0.08), Vector3(-0.35, 0, 0))
	scr.name = "screen"
	mesh(root, Geo.box(0.52, 0.04, 0.22), Mats.obsidian(0x30333a, 0.3, 0.6), Vector3(0, 0.72, 0.08))
	return root

static func safe() -> Node3D:
	var root := Node3D.new()
	rim(mesh(root, Geo.box(0.82, 0.9, 0.82), Mats.obsidian(0x2c2a26, 0.25, 0.95), Vector3(0, 0.45, 0)), 0xffc34a, 0.8)
	var dial := mesh(root, Geo.torus(0.16, 0.04, 8, 20), Mats.glow(0xffc34a, 2.5), Vector3(0, 0.5, 0.42))
	dial.name = "dial"
	var loot := mesh(root, Geo.box(0.4, 0.22, 0.28), Mats.glow(0xffc34a, 3.0), Vector3(0, 1.05, 0))
	loot.name = "loot"
	return root

static func loot_bag() -> Node3D:
	var root := Node3D.new()
	mesh(root, Geo.box(0.36, 0.22, 0.26), Mats.obsidian(0x3a2a10, 0.3, 0.9), Vector3.ZERO)
	mesh(root, Geo.box(0.38, 0.05, 0.28), Mats.glow(0xffc34a, 4.0), Vector3(0, 0.03, 0))
	return root

static func card(color: int) -> Node3D:
	var root := Node3D.new()
	mesh(root, Geo.box(0.26, 0.02, 0.17), Mats.glow(CARD_COLORS[color], 3.0), Vector3.ZERO)
	return root

static func gem() -> Node3D:
	var root := Node3D.new()
	mesh(root, Geo.octa(), Mats.glow(0x9ff8ff, 3.5), Vector3.ZERO, Vector3.ZERO, Vector3(0.14, 0.2, 0.14))
	return root

static func camera() -> Node3D:
	var root := Node3D.new()
	mesh(root, Geo.cylinder(0.04, 0.05, 1.3, 6), Mats.obsidian(0x30333a, 0.3, 0.8), Vector3(0, 0.65, 0))
	var head := node(root, Vector3(0, 1.35, 0))
	head.name = "head"
	mesh(head, Geo.box(0.34, 0.16, 0.16), Mats.obsidian(0x1a1c24, 0.3, 0.7), Vector3(0.12, 0, 0))
	var eye := mesh(head, Geo.sphere(0.05, 8, 6), Mats.glow(0xff4060, 4.0), Vector3(0.3, 0, 0))
	eye.name = "eye"
	return root

static func emitter(color: int) -> Node3D:
	var root := Node3D.new()
	mesh(root, Geo.box(0.16, 0.9, 0.16), Mats.obsidian(0x1a1c24, 0.3, 0.7), Vector3(0, 0.45, 0))
	mesh(root, Geo.sphere(0.05, 8, 6), Mats.glow(color, 5.0), Vector3(0, 0.55, 0))
	return root

class_name Figure
extends Node3D
## A little person on the titan's back (crew or raider): a layered robe, a scarf in the lineage's colour, a hood or
## goggles, arms; walks with a bob, works at a post, falls. 1.8 m tall.

var robe_m: StandardMaterial3D
var scarf_m: StandardMaterial3D
var _arms: Array[Node3D] = []
var _body: Node3D
var _t := 0.0
var walk := 0.0                   # 0..1
var work := 0.0                   # 0..1 arms busy
var down := false
var tool: Node3D

func _init(robe: Color, scarf: Color, skin: Color = Color(0.62, 0.45, 0.33), hooded := false, spear := false) -> void:
	robe_m = Shapes.mat(robe, 0.95)
	scarf_m = Shapes.mat(scarf, 0.9)
	var skin_m := Shapes.mat(skin, 0.8)
	_body = Node3D.new()
	add_child(_body)
	var prof := PackedVector2Array([Vector2(0.36, 0.0), Vector2(0.33, 0.35), Vector2(0.26, 0.8), Vector2(0.22, 1.2), Vector2(0.25, 1.38), Vector2(0.12, 1.48)])
	Shapes.put(_body, Shapes.cached("robe", func() -> Mesh: return Shapes.lathe(prof, 10)), robe_m, Vector3.ZERO)
	Shapes.put(_body, Shapes.cached("scarf", func() -> Mesh: return Shapes.torus(0.12, 0.26, 10)), scarf_m, Vector3(0, 1.36, 0))
	Shapes.put(_body, Shapes.cached("head", func() -> Mesh: return Shapes.sphere(0.15, 10, 6)), skin_m, Vector3(0, 1.62, 0))
	if hooded:
		var hood := Shapes.put(_body, Shapes.cached("hood", func() -> Mesh: return Shapes.sphere(0.19, 10, 6)), scarf_m, Vector3(0, 1.66, 0.03))
		hood.scale = Vector3(1, 1.1, 1.05)
	else:
		Shapes.put(_body, Shapes.cached("goggles", func() -> Mesh: return Shapes.torus(0.13, 0.17, 10)), Shapes.mat(Color(0.35, 0.25, 0.15), 0.4, 0.5), Vector3(0, 1.7, 0)).rotation.x = 0.3
	for sd in [-1, 1]:
		var arm := Node3D.new()
		arm.position = Vector3(sd * 0.27, 1.28, 0)
		_body.add_child(arm)
		var a := Shapes.put(arm, Shapes.cached("arm", func() -> Mesh: return Shapes.cyl(0.07, 0.06, 0.6, 6)), robe_m, Vector3(0, -0.3, 0))
		a.position.y = -0.3
		_arms.append(arm)
	if spear:
		tool = Shapes.put(_arms[1], Shapes.cached("spear", func() -> Mesh: return Shapes.cyl(0.03, 0.02, 2.4, 5)), Shapes.mat(Color(0.3, 0.22, 0.15)), Vector3(0, -0.55, -0.1))
		tool.rotation.x = 1.2

func animate(dt: float) -> void:
	_t += dt
	var b := absf(sin(_t * 9.0)) * 0.08 * walk
	_body.position.y = b
	_body.rotation.z = sin(_t * 9.0) * 0.06 * walk
	for i in _arms.size():
		var sw := sin(_t * 9.0 + i * PI) * 0.6 * walk
		var wk := -1.2 * work + sin(_t * 7.0 + i * 1.7) * 0.4 * work
		_arms[i].rotation.x = sw + wk
	if down:
		_body.rotation.x = lerpf(_body.rotation.x, -PI / 2, minf(1.0, dt * 6.0))
		_body.position.y = 0.2
	else:
		_body.rotation.x = lerpf(_body.rotation.x, 0.0, minf(1.0, dt * 6.0))

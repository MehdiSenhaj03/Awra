class_name VModel
extends RefCounted
## A built model: its node tree, named parts for animation, and the instances whose per-object shader state
## (hit flash, shield tint, gain, fade) follows the simulation. State is pushed only when it changes.

var root := Node3D.new()
var parts: Dictionary = {}
var lists: Dictionary = {}
var key := ""
## Glowing cores: flash white when hit.
var flash: Array[GeometryInstance3D] = []
## Obsidian with a coloured rim: the rim flashes too.
var rims: Array[GeometryInstance3D] = []
## Fading parts (phantoms, the diving serpent).
var fade: Array[GeometryInstance3D] = []

var _f := 0.0
var _s := 0.0
var _g := 1.0
var _a := 1.0

func set_state(f: float, s: float, g: float) -> void:
	if f == _f and s == _s and g == _g:
		return
	if f != _f:
		for gi in flash:
			gi.set_instance_shader_parameter(&"flash", f)
		for gi in rims:
			gi.set_instance_shader_parameter(&"flash", f)
	if s != _s:
		for gi in flash:
			gi.set_instance_shader_parameter(&"shield", s)
		for gi in rims:
			gi.set_instance_shader_parameter(&"shield", s)
	if g != _g:
		for gi in flash:
			gi.set_instance_shader_parameter(&"gain", g)
	_f = f
	_s = s
	_g = g

func set_alpha(a: float) -> void:
	if a == _a:
		return
	_a = a
	for gi in fade:
		gi.set_instance_shader_parameter(&"alpha", a)

func reset() -> void:
	set_state(0.0, 0.0, 1.0)
	set_alpha(1.0)
	root.rotation = Vector3.ZERO
	root.scale = Vector3.ONE
	root.visible = true

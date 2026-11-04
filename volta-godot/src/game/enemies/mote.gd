class_name Mote
extends Enemy
## Grain: a small shard that hunts in packs.

var wob: float

func _init(px_: float, py_: float) -> void:
	super(px_, py_)
	kind = "mote"; look = "mote"
	r = 0.45; hp = 2.0; max_hp = 2.0; value = 40.0; notes = 1
	wob = randf() * U.TAU_F

func update(w: World, _k: float) -> void:
	var a := angle_to(w) + sin(t * 0.05 + wob) * 0.6
	var sp := 6.2 + minf(3.0, t / 120.0)
	steer(x + cos(a) * 5.0, y + sin(a) * 5.0, sp, 0.06)
	rot += 0.12

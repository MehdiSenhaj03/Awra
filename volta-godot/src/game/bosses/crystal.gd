class_name Crystal
extends Enemy
## A crystal grown by Crescendo: it rings louder every bar and shatters into a ring when it is full.

var grow := 0.0

func _init(px_: float, py_: float) -> void:
	super(px_, py_)
	kind = "crystal"; look = "crystal"
	r = 0.9; hp = 8.0; max_hp = 8.0; value = 60.0; notes = 1
	color = P.GREEN
	heavy = true
	contact = true

func update(w: World, _k: float) -> void:
	vx = 0.0
	vy = 0.0
	grow = minf(1.0, t / (w.beat.spb * 60.0 * 8.0))
	if grow >= 1.0 and w.beat.crossed(1) and w.beat.index(1) % 4 == 0:
		P.ring(w, x, y, 16, 3.0, w.rng.range_f(0.0, U.TAU_F), {"color": P.GREEN, "acc": 5.0, "r": 0.3})
		w.emit({"t": "boom", "x": x, "y": y, "r": 2.2, "color": P.GREEN})
		w.sfx("boom", 1.3, 0.6)
		dead = true

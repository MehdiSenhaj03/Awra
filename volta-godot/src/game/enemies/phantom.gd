class_name Phantom
extends Enemy
## Écho: only solid on every other beat; bursts into a ring when it appears.

func _init(px_: float, py_: float) -> void:
	super(px_, py_)
	kind = "phantom"; look = "phantom"
	r = 0.7; hp = 7.0; max_hp = 7.0; value = 230.0; notes = 3
	color = P.VIOLET

func update(w: World, _k: float) -> void:
	var b := w.beat.beat
	var solid := int(floorf(b)) % 2 == 0
	vis += ((1.0 if solid else 0.08) - vis) * 0.3
	hurtable = solid
	targetable = solid
	contact = solid
	if not solid:
		var a := angle_to(w) + sin(t * 0.03) * 1.2
		steer(w.player.x - cos(a) * 6.0, w.player.y - sin(a) * 6.0, 9.0, 0.08)
	else:
		vx *= 0.85
		vy *= 0.85
	if w.beat.crossed(1) and solid and t > 40.0:
		P.ring(w, x, y, 14 if elite else 10, 6.5, w.rng.range_f(0.0, U.TAU_F), {"color": P.VIOLET})
		w.sfx("phase", 1.0, 0.5)

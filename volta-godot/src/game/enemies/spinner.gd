class_name Spinner
extends Enemy
## Toupie: a spinning ring that sprays a double spiral on every eighth.

var a: float = randf() * U.TAU_F

func _init(px_: float, py_: float) -> void:
	super(px_, py_)
	kind = "spinner"; look = "spinner"
	r = 0.95; hp = 14.0; max_hp = 14.0; value = 190.0; notes = 3; heavy = true

func update(w: World, _k: float) -> void:
	steer(w.player.x * 0.4, w.player.y * 0.4, 1.4, 0.02)
	spin += 0.05
	if w.beat.crossed(2) and t > 60.0:
		var arms := 4 if elite else 2
		for i in arms:
			w.bullets.add(x, y, a + (float(i) / float(arms)) * U.TAU_F, 7.5, {"color": P.VIOLET, "r": 0.3})
		a += 0.42
		if w.beat.crossed(1):
			w.sfx("enemyShot", 0.8, 0.35)

class_name Spitter
extends Enemy
## Bouche: keeps its distance and spits aimed triplets on the beat.

var side: float = -1.0 if randf() < 0.5 else 1.0
var ph: int = int(floorf(randf() * 2.0))

func _init(px_: float, py_: float) -> void:
	super(px_, py_)
	kind = "spitter"; look = "spitter"
	r = 0.75; hp = 7.0; max_hp = 7.0; value = 110.0; notes = 2

func update(w: World, _k: float) -> void:
	var d := dist_to(w)
	var a := angle_to(w)
	var want := -1.0 if d < 8.0 else (1.0 if d > 13.0 else 0.0)
	var tx := x + cos(a) * want * 4.0 + cos(a + PI / 2.0) * side * 3.0
	var ty := y + sin(a) * want * 4.0 + sin(a + PI / 2.0) * side * 3.0
	steer(tx, ty, 4.2, 0.05)
	rot = a
	if w.beat.crossed(1) and (w.beat.index(1) + ph) % 2 == 0 and t > 40.0:
		var n := 5 if elite else 3
		P.fan(w, x, y, a, n, 0.7 if elite else 0.42, 10.5, {"color": P.MAGENTA})
		w.emit({"t": "ring", "x": x, "y": y, "r": 1.2, "color": P.MAGENTA, "life": 12})
		w.sfx("enemyShot", 1.0, 0.5)
	if randf() < 0.004:
		side *= -1.0

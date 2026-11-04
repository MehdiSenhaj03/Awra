class_name Choir
extends Enemy
## Chœur: binds up to three allies with beams of light that soak most of their damage.

var links: Array[int] = []

func _init(px_: float, py_: float) -> void:
	super(px_, py_)
	kind = "choir"; look = "choir"
	r = 0.85; hp = 16.0; max_hp = 16.0; value = 280.0; notes = 4
	color = P.GREEN

func update(w: World, _k: float) -> void:
	var a := angle_to(w)
	steer(w.player.x - cos(a) * 14.0, w.player.y - sin(a) * 14.0, 3.0, 0.03)
	spin += 0.03
	if fmod(t, 30.0) < 1.0:
		var cands: Array = []
		for e in w.enemies:
			if e != self and e.is_active() and e.kind != "choir" and not e.boss:
				cands.append(e)
		cands.sort_custom(func(p, q): return U.hypot(p.x - x, p.y - y) < U.hypot(q.x - x, q.y - y))
		links.clear()
		for i in mini(cands.size(), 5 if elite else 3):
			links.append(cands[i].id)
	for lid in links:
		var e := w.enemy_by_id(lid)
		if e and e.is_active():
			e.shielded = 3

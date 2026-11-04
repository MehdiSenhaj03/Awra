class_name Fermata
extends Boss
## FERMATA, the Silence itself: a black sun that holds every note forever. It pulls the hero in, freezes its
## bullets until the bar falls, takes the music away in phase 2 and finally plays every Conductor's song.

## 1 while the music is taken away.
var silence := 0.0
var _spiral := 0.0

func _init(_w: World) -> void:
	super(0.0, 0.0, 4200.0, 3.4)
	kind = "fermata"; look = "fermata"; boss_name = "fermata"
	color = P.WHITE
	intro_bars = 3

func move(w: World, k: float) -> void:
	vx = -x * 2.0
	vy = -y * 2.0
	spin += 0.004 * k
	# Gravity: the black sun pulls.
	var p := w.player
	if not p.dead and not p.is_dashing() and state == "fight":
		var dx := x - p.x
		var dy := y - p.y
		var d := U.hypot(dx, dy)
		if d == 0.0:
			d = 1.0
		var pull := (4.5 if phase == 3 else 3.0) * minf(1.0, 10.0 / d)
		p.vx += (dx / d) * pull * 0.016 * 60.0 * 0.05
		p.vy += (dy / d) * pull * 0.016 * 60.0 * 0.05
	silence += ((1.0 if phase == 2 and state == "fight" else 0.0) - silence) * 0.02

func attack(w: World, ph: int, bar: int, step: int) -> void:
	var aim := angle_to(w)
	var spb := w.beat.spb
	if ph == 1:
		# Held notes: rings appear and wait for the next downbeat.
		if step == 8:
			P.ring(w, x, y, 20, 6.0, _spiral, {"color": P.WHITE, "wait": (spb * 8.0) / 4.0, "r": 0.32})
			_spiral += 0.16
		if step % 4 == 0:
			for i in 3:
				w.bullets.add(x, y, _spiral * 2.0 + (float(i) / 3.0) * U.TAU_F, 6.0, {"color": P.GOLD, "turn": 0.7, "r": 0.3})
		if bar % 2 == 1 and step == 0:
			P.fan(w, x, y, aim, 5, 0.6, 11.0, {"color": P.WHITE, "kind": EnemyBullets.SHARD, "r": 0.3})
	elif ph == 2:
		if step % 4 == 0:
			P.ring(w, x, y, 12, 6.5, _spiral, {"color": P.WHITE, "r": 0.3})
			_spiral += 0.3 if step == 0 else 0.12
		if step == 6 or step == 14:
			P.column(w, x, y, aim, 4, 9.0, 1.6, {"color": P.GOLD, "r": 0.3})
	else:
		# The final chord: every Conductor's song, a bar each.
		var song := bar % 5
		if song == 0 and step % 4 == 0:
			P.ring(w, x, y, 16, 7.0, _spiral, {"color": P.MAGENTA, "r": 0.3})
			_spiral += 0.14
		if song == 1 and step % 4 == 0:
			P.fan(w, x, y, aim, 5 + step / 2, 0.8, 3.5, {"color": P.GREEN, "acc": 9.0, "r": 0.3})
		if song == 2 and step == 0:
			var p := w.player
			w.add_hazard(Hazard.new("zone", p.x, p.y, x, y, 3.2, U.jround(spb * 60.0 * 4.0), P.GOLD, 14))
			w.add_hazard(Hazard.new("zone", -p.x, p.y, x, y, 3.2, U.jround(spb * 60.0 * 4.0), P.GOLD, 14))
		if song == 3 and step % 2 == 0:
			P.ring(w, x, y, 6, 5.0, step * 0.4, {"color": P.VIOLET, "turn": 0.7 if step % 4 != 0 else -0.7, "r": 0.3})
		if song == 4 and step == 0:
			for i in 4:
				var o := -15.0 + i * 10.0
				w.add_hazard(Hazard.new("beam", o, -24.0, o, 24.0, 0.45, U.jround(spb * 60.0 * 2.0), P.WHITE, U.jround(spb * 60.0)))
		if step == 12:
			P.fan(w, x, y, aim, 3, 0.25, 14.0, {"color": P.WHITE, "kind": EnemyBullets.SHARD, "r": 0.3})

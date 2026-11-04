class_name Staccato
extends Boss
## STACCATO, Conductor of the Mute City: a tower of dead skyscrapers that cuts every sound short. It jumps
## between anchors on the bar, fires in hard bursts and lays grids of light that burn on the downbeat.

const ANCHORS := [Vector2(0, -10), Vector2(-12, -2), Vector2(12, -2), Vector2(0, 8)]
var _at := 0
var _tx := 0.0
var _ty := -10.0

func _init(_w: World) -> void:
	super(0.0, -10.0, 2600.0, 1.6)
	kind = "staccato"; look = "staccato"; boss_name = "staccato"
	color = P.MAGENTA

func move(w: World, _k: float) -> void:
	vx = (_tx - x) * 10.0
	vy = (_ty - y) * 10.0
	rot = angle_to(w)

func _jump(w: World) -> void:
	var p := w.player
	var best := _at
	var bd := -1.0
	for i in ANCHORS.size():
		if i == _at:
			continue
		var d := U.hypot(p.x - ANCHORS[i].x, p.y - ANCHORS[i].y)
		if d > bd and w.rng.chance(0.75):
			bd = d
			best = i
	if best == _at:
		best = (_at + 1) % ANCHORS.size()
	_at = best
	w.emit({"t": "spawn", "x": x, "y": y, "color": P.MAGENTA, "size": 1.6})
	_tx = ANCHORS[best].x
	_ty = ANCHORS[best].y
	w.emit({"t": "boom", "x": _tx, "y": _ty, "r": 3.0, "color": P.MAGENTA})
	w.sfx("phase", 0.8)

## Parallel beams across the arena, warned for a bar.
func _grid(w: World, vertical: bool, n: int, offset: float, angle: float = 0.0) -> void:
	var spb := w.beat.spb * 60.0
	var c := cos(angle)
	var s := sin(angle)
	for i in n:
		var o := -18.0 + ((i + offset) / float(n)) * 36.0
		var ax := o if vertical else -24.0
		var ay := -24.0 if vertical else o
		var bx := o if vertical else 24.0
		var by := 24.0 if vertical else o
		w.add_hazard(Hazard.new("beam", ax * c - ay * s, ax * s + ay * c, bx * c - by * s, bx * s + by * c, 0.45, U.jround(spb * 4.0), P.MAGENTA, U.jround(spb)))
	w.sfx("telegraph", 0.7, 0.6)

func on_phase(w: World, ph: int) -> void:
	if ph == 2:
		var placed := 0
		for i in ANCHORS.size():
			if i == _at or placed >= 2:
				continue
			var d := w.add(Decoy.new(ANCHORS[i].x, ANCHORS[i].y, self))
			d.spawn_t = 40
			placed += 1

func attack(w: World, ph: int, bar: int, step: int) -> void:
	var aim := angle_to(w)
	if step == 0 and bar % 2 == 1:
		_jump(w)
	# Staccato bursts: three short notes, rest, three more.
	if step in [0, 1, 2, 8, 9, 10]:
		w.bullets.add(x, y, aim + (step % 8 - 1) * 0.08, 17.0, {"color": P.WHITE, "kind": EnemyBullets.SHARD, "r": 0.3})
		if ph >= 2:
			P.fan(w, x, y, aim, 3, 0.9, 12.0, {"color": P.MAGENTA, "kind": EnemyBullets.SHARD, "r": 0.26})
	if step == 4 and bar % 2 == 0:
		if ph == 1:
			_grid(w, bar % 4 == 0, 4, float(bar % 8) / 8.0)
		elif ph == 2:
			_grid(w, true, 3, float(bar % 6) / 6.0)
			_grid(w, false, 3, float((bar + 3) % 6) / 6.0)
		else:
			_grid(w, bar % 4 == 0, 5, float(bar % 10) / 10.0, PI / 4.0)
	if ph == 3 and step == 12:
		P.ring(w, x, y, 20, 8.0, bar * 0.2, {"color": P.VIOLET, "r": 0.3})
	if ph == 3 and step % 4 == 2:
		w.bullets.add(x, y, (float(step) / 16.0) * U.TAU_F + bar, 9.0, {"color": P.MAGENTA, "r": 0.32})

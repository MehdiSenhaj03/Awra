class_name Fugue
extends Boss
## FUGUE, the twin Conductors of the Foundry. The leader plays; one bar later its twin answers, mirrored.
## Kill one and the survivor plays both voices.

var twin: Fugue = null
## -1 left (leader), 1 right (answer).
var side: float
var _chain: Hazard = null
var solo := false

func _init(_w: World, s: float) -> void:
	super(s * 8.0, -6.0, 1500.0, 1.9)
	side = s
	kind = "fugue"; look = "fugue"; boss_name = "fugue"
	color = P.ORANGE if s < 0.0 else P.GOLD

static func make(w: World) -> Fugue:
	var a := Fugue.new(w, -1.0)
	var b: Fugue = w.add(Fugue.new(w, 1.0))
	a.twin = b
	b.twin = a
	return a

func move(w: World, k: float) -> void:
	var b := w.beat.beat * (PI / 8.0)
	var tx := side * 8.0 + sin(b) * 3.0
	var ty := -6.0 + cos(b * 0.5) * 2.0
	if phase >= 2 or solo:
		var a := w.beat.beat * (PI / 16.0) + (0.0 if side < 0.0 else PI)
		tx = cos(a) * 11.0
		ty = sin(a) * 9.0
	vx += (tx - x) * 0.03 * k
	vy += (ty - y) * 0.03 * k
	vx *= 0.88
	vy *= 0.88
	rot = angle_to(w)
	if _chain and twin and not twin.dead and twin.dying == 0:
		_chain.x = x
		_chain.y = y
		_chain.x2 = twin.x
		_chain.y2 = twin.y
	elif _chain:
		_chain.done = true
		_chain = null

func on_phase(w: World, ph: int) -> void:
	if ph == 2 and side < 0.0 and twin and not twin.dead:
		_chain = Hazard.new("beam", x, y, x, y, 0.26, U.jround(w.beat.spb * 60.0 * 4.0), P.GOLD, 1e9)
		w.add_hazard(_chain)

func attack(w: World, ph: int, bar: int, step: int) -> void:
	if side > 0.0 and not solo:
		if bar >= 1:
			_voice(w, ph, bar - 1, step, true)
		return
	_voice(w, ph, bar, step, false)
	if solo and step % 8 == 4:
		_voice(w, ph, bar, (step + 8) % 16, true)

func _voice(w: World, ph: int, bar: int, step: int, mirror: bool) -> void:
	var p := w.player
	var aim := angle_to(w)
	var m := -1.0 if mirror else 1.0
	var motif := bar % 4
	if motif == 0 and step % 2 == 0:
		P.column(w, x, y, aim + m * (float(step) / 16.0 - 0.5) * 1.2, 3, 8.0, 1.5, {"color": color, "r": 0.3})
	if motif == 1 and step == 0:
		var tx := -p.x if mirror else p.x
		w.add_hazard(Hazard.new("zone", tx, p.y, x, y, 3.0, U.jround(w.beat.spb * 60.0 * 4.0), color, 12))
		w.sfx("mortarLaunch")
	if motif == 2 and step % 4 == 0:
		P.ring(w, x, y, 12 + ph * 2, 6.5, m * step * 0.13, {"color": color, "r": 0.3})
	if motif == 3 and (step == 0 or step == 8):
		P.fan(w, x, y, aim, 7 + ph * 2, 1.4, 9.0, {"color": P.WHITE, "kind": EnemyBullets.BIG, "r": 0.42})
	if ph == 3 and step == 12:
		for i in 6:
			w.bullets.add(x, y, (float(i) / 6.0) * U.TAU_F + bar, 5.0, {"color": color, "turn": m * 0.8, "r": 0.32})

func on_death(w: World) -> void:
	if twin and not twin.dead and twin.dying == 0:
		twin.solo = true
		twin.hp = minf(twin.max_hp, twin.hp + twin.max_hp * 0.15)
		w.emit({"t": "boss", "phase": 9, "name": "fugueSolo"})

func release() -> void:
	if twin:
		twin.twin = null
	twin = null
	_chain = null

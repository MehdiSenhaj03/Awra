class_name Boss
extends Enemy
## A Conductor: an intro (invulnerable, two bars), three phases by health with a breath between them, attacks
## called on every sixteenth with the bar of the current phase, and a long collapse.

var phase := 1
var phase_beat := 0.0
var intro_bars := 2
var dying := 0
var _breath := 0
var boss_name := ""
var thresholds := [0.66, 0.33]

func _init(px_: float, py_: float, h: float, rad: float) -> void:
	super(px_, py_)
	boss = true
	heavy = true
	hp = h
	max_hp = h
	r = rad
	value = 20000.0
	notes = 40
	spawn_t = 0
	state = "intro"

func bars(w: World) -> float:
	return (w.beat.beat - phase_beat) / 4.0

func is_active() -> bool:
	return not dead and dying == 0

func tick(w: World) -> void:
	if dying > 0:
		px = x
		py = y
		dying += 1
		if dying % 8 == 0 and dying < 150:
			var a := w.rng.range_f(0.0, PI * 2.0)
			var d := w.rng.range_f(0.0, r * 1.2)
			w.emit({"t": "boom", "x": x + cos(a) * d, "y": y + sin(a) * d, "r": 2.0 + w.rng.range_f(0.0, 2.0), "color": color})
			w.emit({"t": "shake", "a": 0.25})
			w.sfx("boom", 0.7 + w.rng.next() * 0.4)
		if dying == 160:
			dead = true
			w.killed(self)
			w.emit({"t": "bossDown", "x": x, "y": y})
			w.emit({"t": "kill", "x": x, "y": y, "size": r * 2.5, "color": color, "look": look, "heavy": true})
			w.emit({"t": "boom", "x": x, "y": y, "r": 10.0, "color": 0xffffff})
			w.emit({"t": "shake", "a": 1.0})
			w.emit({"t": "flash", "color": 0xffffff, "a": 1.0})
			w.sfx("bigBoom")
			w.hit_stop(16)
		return
	super.tick(w)

func update(w: World, k: float) -> void:
	if state == "intro":
		hurtable = false
		move(w, k)
		if st == 1.0:
			w.emit({"t": "boss", "phase": 0, "name": boss_name})
			w.sfx("roar")
		if w.beat.crossed(1) and w.beat.index(1) % 4 == 0 and st > w.beat.spb * 60.0 * 4.0 * intro_bars - 30.0:
			state = "fight"
			hurtable = true
			phase_beat = U.jround(w.beat.beat)
			w.emit({"t": "boss", "phase": 1, "name": boss_name})
		return
	if _breath > 0:
		move(w, k)
		if w.beat.crossed(1) and w.beat.index(1) % 4 == 0:
			_breath -= 1
			if _breath == 0:
				hurtable = true
				phase_beat = U.jround(w.beat.beat)
		return
	var f := hp / max_hp
	var nxt := 3 if f < thresholds[1] else (2 if f < thresholds[0] else 1)
	if nxt > phase:
		phase = nxt
		hurtable = false
		_breath = 1
		w.clear_bullets(x, y, 80.0, true)
		for h in w.hazards:
			h.done = true
		w.emit({"t": "boss", "phase": nxt, "name": boss_name})
		w.emit({"t": "boom", "x": x, "y": y, "r": r * 3.0, "color": color})
		w.emit({"t": "shake", "a": 0.8})
		w.emit({"t": "chroma", "a": 1.0})
		w.sfx("roar")
		w.hit_stop(10)
		on_phase(w, nxt)
		return
	move(w, k)
	if w.beat.crossed(4):
		var b := bars(w)
		attack(w, phase, int(floorf(b)), int(floorf(fmod(b * 16.0, 16.0) + 1e-6)))

func move(_w: World, _k: float) -> void:
	pass

func attack(_w: World, _phase: int, _bar: int, _step: int) -> void:
	pass

func on_phase(_w: World, _p: int) -> void:
	pass

## Conductors never die at once: a long collapse first.
func die(w: World) -> void:
	if dying > 0 or dead:
		return
	dying = 1
	hurtable = false
	contact = false
	w.clear_bullets(x, y, 100.0, true)
	for h in w.hazards:
		h.done = true
	w.emit({"t": "flash", "color": 0xffffff, "a": 0.9})
	w.emit({"t": "shake", "a": 1.0})
	w.sfx("roar")
	w.hit_stop(14)
	on_death(w)

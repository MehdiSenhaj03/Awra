class_name StageRunner
extends RefCounted
## Runs a stage: waves start on bar lines once the previous wave is cleared; the boss ends it.

var w: World
var def: Dictionary
var movement: int
var wave := -1
var _wave_beat := 0.0
var _pending: Array = []
## "intro", "run", "clear", "fail".
var state := "intro"
var _t := 0
var boss: Enemy = null
var total: int
var echo_spawned := false

func _init(world: World, d: Dictionary, start_movement: int = 0) -> void:
	w = world
	def = d
	movement = start_movement
	total = 0
	for m in d.movements:
		total += m.size()
	if d.get("boss"):
		total += 1

var wave_number: int:
	get:
		var n := 0
		for m in movement:
			if m < def.movements.size():
				n += def.movements[m].size()
		return n + maxi(0, wave) + 1

func update() -> void:
	_t += 1
	if state == "clear" or state == "fail":
		return
	if w.player.dead:
		if w.player.dead_t > 100:
			state = "fail"
			w.over = "lose"
		return
	if state == "intro":
		if _t > 60 and w.beat.crossed(1) and w.beat.index(1) % 4 == 0:
			state = "run"
			_next_wave()
		return
	var b := w.beat.beat
	var i := _pending.size() - 1
	while i >= 0:
		var s: Dictionary = _pending[i]
		if b >= _wave_beat + float(s.at):
			_spawn(s)
			_pending.remove_at(i)
		i -= 1
	if boss:
		var any := false
		for e in w.enemies:
			if e.boss and not e.dead:
				any = true
				break
		if not any:
			state = "clear"
			w.over = "win"
			w.emit({"t": "clear"})
		return
	var alive := false
	for e in w.enemies:
		if not e.dead and e.kind != "echo":
			alive = true
			break
	if _pending.is_empty() and not alive and w.beat.crossed(1) and w.beat.index(1) % 4 == 0:
		_next_wave()

func _next_wave() -> void:
	var mv: Array = def.movements
	wave += 1
	if movement < mv.size() and wave >= mv[movement].size():
		movement += 1
		wave = 0
		if movement < mv.size() or def.get("boss"):
			w.emit({"t": "checkpoint"})
	if movement >= mv.size():
		if def.get("boss") and boss == null:
			boss = Bosses.make(def.boss, w)
			if not w.enemies.has(boss):
				w.add(boss)
			w.emit({"t": "wave", "n": total, "of": total})
			return
		state = "clear"
		w.over = "win"
		w.emit({"t": "clear"})
		return
	_wave_beat = w.beat.beat
	_pending = mv[movement][wave].duplicate()
	w.emit({"t": "wave", "n": wave_number, "of": total})
	if w.mods.has.has("sourdine"):
		w.guard = true
	var f = def.get("fragment")
	if f and not echo_spawned and int(f.m) == movement and int(f.w) == wave:
		echo_spawned = true
		var e := w.arena.edge(w.rng.range_f(0.0, U.TAU_F), 4.0)
		_pending.append({"at": 4, "kind": "echo:" + str(f.id), "x": e.x, "y": e.y})

func _spawn(s: Dictionary) -> void:
	var kind: String = s.kind
	if kind.begins_with("boss:"):
		var b := Bosses.make(kind.substr(5), w)
		if not w.enemies.has(b):
			w.add(b)
		return
	var e: Enemy
	if kind.begins_with("echo:"):
		e = GoldenEcho.new(s.x, s.y, kind.substr(5))
	else:
		e = EnemyKinds.make(kind, s.x, s.y)
	if s.get("elite", false):
		e.make_elite()
	e.spawn_t = int(U.jround(w.beat.spb * 60.0))
	# Never on top of the hero: push the entry point out to a fair distance.
	var hero := w.player
	var dx := e.x - hero.x
	var dy := e.y - hero.y
	var d := U.hypot(dx, dy)
	var safe := 9.0 if e.heavy else 6.0
	if d < safe:
		var a := atan2(dy, dx) if d > 0.01 else w.rng.range_f(0.0, PI * 2.0)
		e.x = hero.x + cos(a) * safe
		e.y = hero.y + sin(a) * safe
	var c := w.arena.confine(e.x, e.y, e.r + 0.5)
	e.x = c.x
	e.px = c.x
	e.y = c.y
	e.py = c.y
	w.add(e)
	w.emit({"t": "spawn", "x": e.x, "y": e.y, "color": e.color, "size": e.r})

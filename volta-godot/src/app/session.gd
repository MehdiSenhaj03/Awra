class_name Session
extends RefCounted
## One stage being played: the world, its runner, the HUD and the event plumbing to sound and screen.

var world: World
var runner: StageRunner
var hud: Hud
var view: View
var def: Dictionary
var ended := false
var on_end: Callable
var on_events: Callable
var _last_aim := Vector2(1, 0)
var _end_t := -1.0
var _win := false
## Scripted aim for automation: null, "auto" or a Vector2.
var script_aim: Variant = null
var script_keys: Dictionary = {}

func _init(d: Dictionary, v: View, ui_parent: Control, checkpoint: int, setup: Callable) -> void:
	def = d
	view = v
	world = World.new(d.arena, d.bpm, 7)
	setup.call(world)
	runner = StageRunner.new(world, d, checkpoint)
	world.player.y = 4.0
	world.player.py = 4.0
	hud = Hud.new()
	ui_parent.add_child(hud)
	Sound.start(d.theme, d.bpm)
	Sound.intensity = 1
	world.beat.set_bpm(d.bpm)

func dispose() -> void:
	if is_instance_valid(hud):
		hud.queue_free()
	world.dispose()

func _down(a: String) -> bool:
	return script_keys.has(a) or Keys.down(a)

func _pressed(a: String) -> bool:
	return Keys.pressed(a) or script_keys.get(a, false) == true

func _controls() -> Controls:
	var c := Controls.new()
	var m := Keys.move()
	var p := world.player
	var aim := _last_aim
	var pad := Keys.pad_aim()
	if script_aim is String:
		var t := world.nearest_enemy(p.x, p.y, 60.0)
		if t:
			aim = Vector2(t.x - p.x, t.y - p.y)
	elif script_aim is Vector2:
		aim = script_aim
	elif pad.length() > 0.3:
		aim = pad
	elif Input.get_connected_joypads().size() > 0 and m.length() > 0.3 and not _mouse_moved:
		aim = m
	else:
		var hit: Variant = view.pick(view.get_viewport().get_mouse_position())
		if hit != null:
			aim = (hit as Vector2) - Vector2(p.x, p.y)
	if aim.length() < 1e-6:
		aim = _last_aim
	aim = aim.normalized()
	_last_aim = aim
	c.mx = m.x
	c.my = m.y
	c.ax = aim.x
	c.ay = aim.y
	c.fire = _down("fire") or pad.length() > 0.5
	c.fire_pressed = _pressed("fire")
	c.dash = _pressed("dash")
	c.pulse = _pressed("pulse")
	c.overdrive = _pressed("overdrive")
	return c

var _mouse_moved := true

## One simulation frame.
func step() -> void:
	var w := world
	var prev_beat := floori(w.beat.beat)
	w.update(_controls())
	runner.update()
	var events := w.events
	view.handle(events, w)
	_sound(events)
	if on_events.is_valid():
		on_events.call(events)
	w.events.clear()
	var b := floori(w.beat.beat)
	if b != prev_beat:
		view.on_beat(b % 4 == 0)
	Sound.intensity = 1 if runner.state == "intro" else (4 if runner.boss else (3 if w.rank >= 3 else 2))
	Sound.overdrive = w.player.is_overdrive()
	var sil := 0.0
	for e in w.enemies:
		if e is Fermata:
			sil = (e as Fermata).silence
	Sound.silence = sil
	if not ended and w.over != "":
		ended = true
		_win = w.over == "win"
		if not _win:
			Sound.muffle(true, 500.0)
		_end_t = 1.8 if _win else 0.6

func _sound(events: Array[Dictionary]) -> void:
	var b := world.beat.beat
	for e in events:
		match e.t:
			"shot":
				var wp: String = e.weapon
				if wp.begins_with("arcLink") or wp == "arcMiss":
					continue
				Sound.note("perfect" if wp.contains("Perfect") else ("accent" if e.accent else "shot"), b)
			"judge":
				if e.get("what", "") != "shot":
					Sound.note("perfect" if e.judge == "perfect" else "great", b)
			"kill":
				Sound.note("bigKill" if e.heavy else "kill", b)
				Sound.sfx("bigBoom" if e.heavy else "boom", 1.0 + randf() * 0.2)
			"parry":
				Sound.note("parry", b)
			"graze":
				Sound.note("graze", b)
			"spawn":
				Sound.sfx("spawn")
			"checkpoint":
				Sound.sfx("checkpoint")
				hud.banner("CHECKPOINT")
			"boss":
				Sound.sfx("roar")
			"sfx":
				Sound.sfx(e.name, e.pitch, e.vol)

## Every displayed frame.
func frame(alpha: float, dt: float) -> void:
	var w := world
	Sound.update(w.beat.beat, w.beat.bpm, dt)
	view.level = Sound.level()
	view.render(w, alpha, dt)
	hud.update_hud(w, runner, dt)
	hud.update_popups(view, dt)
	if _end_t >= 0.0:
		_end_t -= dt
		if _end_t < 0.0 and on_end.is_valid():
			on_end.call(_win)

class_name Heist
extends Screen
## Playing a vault: the loop, the live echo(es), rewinding, the timeline, alarms, the end of a loop, the win.

signal finished(result: Dictionary)
signal quit

enum S { READY, PLAY, FINISH, ALARM, OUT, WON }

var level: Level
var run: Run
var view: View
var state := S.READY
var coop := 1
## Who drives each live slot: "solo", "kb", "kb2", "pad0", "pad1", "net".
var sources: Array = ["solo"]
var meta: Dictionary = {}
var _prev_dev: Dictionary = {}
var _throw_prev := false
var _top_timer: Label
var _top_loop: Label
var _budget: HBoxContainer
var _name: Label
var _par: Label
var _timeline: Timeline
var _banner: Control
var _prompt: Label
var _hint: RichTextLabel
var _overlay: Control = null
var _menu: UIMenu = null
var _preview_frame := -1
var _preview: Sim = null
var _marks: Array = []
var _alarm_frame := -1
var _slow := 0.0
var _won := {}
## Duel: the opponent's progress, shown under the timer.
var opp_text := ""
var _opp: Label
var _out_wait := 0
## Automation: one plan per echo slot (see Planner), used for demos and tests.
var plans: Array = []
var _planners: Dictionary = {}

var replay := false
## Online co-op: the session feeds inputs in lockstep.
var net_mode := false

func _init(lv: Level, v: View, players: int = 1, srcs: Array = ["solo"], info: Dictionary = {}, replay_recs: Array = []) -> void:
	super()
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	level = lv
	view = v
	view.zoom = 1.0
	view.margin_top = 120.0
	coop = players
	sources = srcs
	meta = info
	if replay_recs.size() > 0:
		replay = true
		run = Run.from_replay(lv, replay_recs)
	else:
		run = Run.new(lv, coop)
	_build_ui()
	view.build(lv, run.sim)
	_enter_ready()

# ---- Interface -------------------------------------------------------------------------------------------------------

func _build_ui() -> void:
	var top := Control.new()
	top.mouse_filter = Control.MOUSE_FILTER_IGNORE
	top.size = Vector2(Kit.W, 120)
	add_child(top)
	_top_timer = Kit.label(top, "40.00", 54, Kit.CREAM, "display", 400, 0.04, Color(1, 0.75, 0.3, 0.5), 18)
	_top_timer.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_top_timer.size = Vector2(Kit.W, 70)
	_top_timer.position = Vector2(0, 18)
	_top_loop = Kit.label(top, "", 13, Kit.BRASS, "body", 600, 0.35)
	_top_loop.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_top_loop.size = Vector2(Kit.W, 20)
	_top_loop.position = Vector2(0, 88)
	_budget = Kit.hbox(top, 10)
	_budget.position = Vector2(40, 34)
	_name = Kit.label(top, level.data.get("name", ""), 26, Kit.CREAM, "deco", 400, 0.06)
	_name.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	_name.size = Vector2(700, 36)
	_name.position = Vector2(Kit.W - 740, 26)
	_opp = Kit.label(top, "", 14, Kit.MAGENTA, "body", 600, 0.1)
	_opp.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_opp.size = Vector2(Kit.W, 20)
	_opp.position = Vector2(0, 110)
	_par = Kit.label(top, "", 13, Kit.BRASS, "body", 600, 0.2)
	_par.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	_par.size = Vector2(700, 20)
	_par.position = Vector2(Kit.W - 740, 66)
	_timeline = Timeline.new()
	_timeline.run = run
	var th := 30.0 + run.budget * 20.0
	_timeline.position = Vector2((Kit.W - 1300) / 2, Kit.H - 30 - th)
	_timeline.size = Vector2(1300, th)
	view.margin_bottom = th + 80.0
	_timeline.hover.connect(_on_hover)
	_timeline.pick.connect(_on_pick)
	add_child(_timeline)
	_prompt = Kit.label(self, "", 15, Kit.CREAM, "body", 600, 0.1, Color(0, 0, 0, 0.9), 8)
	_prompt.visible = false
	_hint = Kit.rich(self, "", 16, Kit.PALE)
	_hint.fit_content = false
	_hint.size = Vector2(1300, 30)
	_hint.position = Vector2((Kit.W - 1300) / 2, Kit.H - 30 - th - 34)
	_banner = Control.new()
	_banner.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_banner.size = Vector2(Kit.W, 200)
	_banner.position = Vector2(0, Kit.H * 0.3)
	add_child(_banner)

func _refresh_budget() -> void:
	for c in _budget.get_children():
		c.queue_free()
	for i in run.budget:
		var d := EchoDot.new()
		d.color = Mats.srgb(Props.ECHO_COLORS[i % 6])
		d.state = 2 if run.live.has(i) else (1 if i < run.rec.size() else 0)
		_budget.add_child(d)
	var par := int(level.data.get("par", 0))
	var gems := run.sim.gems_total
	_par.text = ("%s %d" % [I18n.t("heist.par"), par] if par > 0 else "") + ("   ◆ %d/%d" % [run.sim.gems, gems] if gems > 0 else "")

func banner(text: String, sub: String = "", color: Color = Kit.CREAM, hold: float = 1.6) -> void:
	for c in _banner.get_children():
		c.queue_free()
	var box := Control.new()
	box.mouse_filter = Control.MOUSE_FILTER_IGNORE
	box.size = _banner.size
	box.pivot_offset = box.size / 2
	_banner.add_child(box)
	var l := Kit.label(box, text, 68, color, "display", 400, 0.12, Color(0, 0, 0, 0.8), 20)
	l.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	l.size = Vector2(Kit.W, 90)
	l.position.y = 40
	if sub != "":
		var s := Kit.label(box, sub, 18, Kit.BRASS, "body", 600, 0.3, Color(0, 0, 0, 0.8), 10)
		s.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		s.size = Vector2(Kit.W, 26)
		s.position.y = 130
	box.modulate.a = 0.0
	box.scale = Vector2(1.15, 1.15)
	var tw := box.create_tween()
	tw.set_parallel()
	tw.tween_property(box, "modulate:a", 1.0, 0.25)
	tw.tween_property(box, "scale", Vector2.ONE, 0.25).set_ease(Tween.EASE_OUT)
	if hold > 0.0:
		tw.chain().tween_interval(hold)
		tw.chain().tween_property(box, "modulate:a", 0.0, 0.5)
		tw.chain().tween_callback(box.queue_free)

func _hint_text(t: String) -> void:
	_hint.text = "[center]%s[/center]" % Kit.html(t)

func _keys_hint() -> String:
	return I18n.t("heist.keys", {"use": Keys.key_name("use"), "sprint": Keys.key_name("sprint"), "throw": Keys.key_name("throw"), "rewind": Keys.key_name("rewind"), "restart": Keys.key_name("restart"), "finish": Keys.key_name("finish"), "echoes": Keys.key_name("echoes")})

# ---- States ---------------------------------------------------------------------------------------------------------

func _enter_ready() -> void:
	if run.loops > 1 and run.sim.frame == 0:
		Sound.sfx("loop")
	if replay:
		state = S.PLAY
		_refresh_budget()
		_hint_text("")
		return
	state = S.READY
	_close_overlay()
	_refresh_budget()
	var names: Array[String] = []
	for s in run.live:
		names.append(I18n.t("echo.%d" % (s % 6)))
	banner(I18n.t("heist.loop", {"n": run.loops}), I18n.t("heist.echo_turn", {"name": ", ".join(names)}), Kit.CREAM, 1.4)
	_hint_text(I18n.t("heist.ready") + "  ·  " + _keys_hint())

func _commit_loop() -> void:
	# The loop ran out (or the live echoes are done): they become past echoes, then a new one starts.
	if run.can_add_echo():
		run.start_loop(coop)
		view.build(level, run.sim)
		_enter_ready()
	else:
		_out_of_echoes()

func _out_of_echoes() -> void:
	state = S.OUT
	banner(I18n.t("heist.out"), I18n.t("heist.out_sub"), Kit.BRASS, 0.0)
	_open_menu([
		[I18n.t("heist.rewind_loop"), func() -> void: _rewind_to(0)],
		[I18n.t("heist.echoes"), _echoes_menu],
		[I18n.t("heist.restart_all"), _restart_all],
		[I18n.t("heist.quit"), func() -> void: quit.emit()],
	])

func _restart_all() -> void:
	run = Run.new(level, coop)
	_timeline.run = run
	_marks.clear()
	view.build(level, run.sim)
	_enter_ready()

func _rewind_to(f: int) -> void:
	Sound.sfx("rewind", 1.0, 0.5)
	_alarm_frame = -1
	run.rewind_to(f)
	var keep: Array = []
	for m: Dictionary in _marks:
		if not run.live.has(int(m.slot)) or int(m.frame) < f:
			keep.append(m)
	_marks = keep
	view.build(level, run.sim)
	state = S.READY
	_close_overlay()
	for c in _banner.get_children():
		c.queue_free()
	_hint_text(I18n.t("heist.ready") + "  ·  " + _keys_hint())

func _echoes_menu() -> void:
	var items: Array = []
	for s in run.rec.size():
		var slot := s
		items.append([I18n.t("heist.rerecord", {"name": I18n.t("echo.%d" % (s % 6))}), func() -> void:
			run.start_loop(1, [slot])
			view.build(level, run.sim)
			_enter_ready()])
	if run.rec.size() > 1:
		var last := run.rec.size() - 1
		items.append([I18n.t("heist.erase_last"), func() -> void:
			run.erase(last)
			run.start_loop(coop)
			view.build(level, run.sim)
			_enter_ready()])
	items.append([I18n.t("common.back"), func() -> void:
		_close_overlay()
		if state == S.OUT:
			_out_of_echoes()])
	_open_menu(items)

func _open_menu(items: Array) -> void:
	_close_overlay()
	_overlay = Control.new()
	_overlay.mouse_filter = Control.MOUSE_FILTER_STOP
	Kit.full(_overlay)
	add_child(_overlay)
	var p := Kit.panel(_overlay)
	var m := Kit.margin(p, 36, 28, 36, 28)
	var v := Kit.vbox(m, 8)
	_menu = UIMenu.new(v)
	for it: Array in items:
		_menu.add(it[0], it[1], {"size": 16, "width": 560})
	p.ready.connect(func() -> void: p.position = Vector2((Kit.W - p.size.x) / 2, Kit.H * 0.52))

func _close_overlay() -> void:
	if _overlay:
		_overlay.queue_free()
		_overlay = null
		_menu = null

func _alarm() -> void:
	state = S.ALARM
	_alarm_frame = run.sim.frame
	var who := run.sim.alarm_by
	var by := I18n.t("heist.by_guard") if who < 100 else (I18n.t("heist.by_camera") if who < 200 else I18n.t("heist.by_laser"))
	banner(I18n.t("heist.alarm"), by, Kit.RED, 0.0)
	_hint_text(I18n.t("heist.alarm_keys", {"rewind": Keys.key_name("rewind"), "restart": Keys.key_name("restart"), "echoes": Keys.key_name("echoes")}))
	Sound.sfx("alarm")

func _win() -> void:
	state = S.WON
	if replay:
		banner(I18n.t("heist.won"), I18n.t("heist.won_sub", {"n": run.rec.size()}), Kit.GOLD, 0.0)
		get_tree().create_timer(2.0).timeout.connect(func() -> void: quit.emit())
		return
	_slow = 1.0
	var n := run.rec.size()
	var par := int(level.data.get("par", 0))
	var medal := 3 if par == 0 or n <= par else (2 if n == par + 1 else 1)
	_won = {"echoes": n, "par": par, "medal": medal, "frame": run.sim.win_frame, "gems": run.sim.gems, "gems_total": run.sim.gems_total, "loops": run.loops, "replay": run.replay_code()}
	banner(I18n.t("heist.won"), I18n.t("heist.won_sub", {"n": n}), Kit.GOLD, 0.9)
	Sound.sfx("win")
	get_tree().create_timer(1.6).timeout.connect(func() -> void: finished.emit(_won))

# ---- Hover preview ----------------------------------------------------------------------------------------------------

func _on_hover(f: int) -> void:
	if state == S.PLAY or state == S.FINISH:
		_preview_frame = -1
		return
	_preview_frame = f
	_preview = run.preview(f) if f >= 0 else null

func _on_pick(f: int) -> void:
	if state == S.WON:
		return
	if f <= run.sim.frame:
		_rewind_to(f)

# ---- Frames ---------------------------------------------------------------------------------------------------------

func _live_inputs() -> Array:
	var out: Array = []
	for li in run.live.size():
		var src: String = sources[li] if li < sources.size() else "solo"
		if src == "plan":
			var slot: int = run.live[li]
			if not _planners.has(slot) or (_planners[slot] as Planner).slot != slot or run.sim.frame == 0:
				_planners[slot] = Planner.new(plans[slot] if slot < plans.size() else [], slot)
			out.append((_planners[slot] as Planner).next(run.sim))
		elif src == "solo":
			var th := Keys.down("throw")
			out.append(Keys.solo_byte(th and not _throw_prev))
			_throw_prev = th
		else:
			out.append(Keys.device_byte(src, _prev_dev))
	return out

func _any_input(inp: Array) -> bool:
	for b: int in inp:
		if b != 0:
			return true
	return false

## One simulation frame (called at 60 Hz).
func step() -> void:
	if _out_wait > 0:
		_out_wait -= 1
		if _out_wait == 0:
			_commit_loop()
		return
	if _overlay != null:
		if _menu:
			_menu.handle()
		return
	if replay:
		if state == S.PLAY:
			_tick([])
		if Keys.pressed("pause") or Keys.pressed("back"):
			quit.emit()
		return
	if Keys.down("rewind") and (state == S.PLAY or state == S.READY or state == S.ALARM or state == S.FINISH):
		var f := maxi(0, run.sim.frame - 3)
		if run.sim.frame > 0:
			_rewind_to(f)
		return
	if Keys.pressed("restart") and state != S.WON:
		_rewind_to(0)
		return
	if Keys.pressed("echoes") and state != S.WON:
		_echoes_menu()
		return
	match state:
		S.READY:
			var inp := _live_inputs()
			if _any_input(inp) or sources.has("plan"):
				state = S.PLAY
				_hint_text(_keys_hint())
				_tick(inp)
		S.PLAY:
			if Keys.pressed("finish"):
				state = S.FINISH
				_hint_text(I18n.t("heist.finishing"))
				return
			_tick(_live_inputs())
		S.FINISH:
			for k in 6:
				var idle: Array = []
				for s in run.live:
					idle.append(0)
				_tick(idle)
				if state != S.FINISH:
					break

## Online co-op: one lockstep frame with both players' bytes and their control flags (OR-ed).
## Flags: 1 rewind held, 2 restart pressed, 4 finish pressed.
func step_net(bytes: Array, flags: int) -> void:
	if _out_wait > 0:
		_out_wait -= 1
		if _out_wait == 0:
			if run.can_add_echo():
				run.start_loop(coop)
				view.build(level, run.sim)
				_enter_ready()
			else:
				_rewind_to(0)
		return
	if (flags & 1) != 0 and state != S.WON:
		if run.sim.frame > 0:
			_rewind_to(maxi(0, run.sim.frame - 3))
		return
	if (flags & 2) != 0 and state != S.WON:
		_rewind_to(0)
		return
	match state:
		S.READY:
			if _any_input(bytes):
				state = S.PLAY
				_hint_text(_keys_hint())
				_tick(bytes)
		S.PLAY:
			if (flags & 4) != 0:
				state = S.FINISH
				return
			_tick(bytes)
		S.FINISH:
			for k in 6:
				_tick([0, 0])
				if state != S.FINISH:
					break

## This player's input byte and flags for the next lockstep frame.
func local_net_input() -> Array:
	var th := Keys.down("throw")
	var b := Keys.solo_byte(th and not _throw_prev)
	_throw_prev = th
	var f := (1 if Keys.down("rewind") else 0) | (2 if Keys.pressed("restart") else 0) | (4 if Keys.pressed("finish") else 0)
	return [b, f]

func _tick(inp: Array) -> void:
	view.capture(run.sim)
	run.tick(inp)
	view.handle(run.sim.events)
	for e in run.sim.events:
		if e.has("echo") and e.t in ["crack", "hack", "pick", "takedown", "drop", "throw", "gem"]:
			_marks.append({"slot": e.echo, "frame": run.sim.frame, "kind": e.t})
		_sound(e)
	if run.sim.over == "alarm":
		_alarm()
	elif run.sim.over == "win":
		_win()
	elif run.sim.frame >= run.loop_frames:
		if replay:
			quit.emit()
			return
		state = S.OUT
		_out_wait = 54
		banner(I18n.t("heist.loop_end"), "", Kit.CYAN, 0.9)

func _sound(e: Dictionary) -> void:
	match e.t:
		"door": Sound.sfx("door_open" if e.get("open", false) else "door_close")
		"plate": Sound.sfx("plate")
		"lever", "vlever", "button": Sound.sfx("click")
		"crack": Sound.sfx("crack")
		"hack": Sound.sfx("hack")
		"pick", "gem": Sound.sfx("pick")
		"takedown": Sound.sfx("takedown")
		"coin": Sound.sfx("coin")
		"spot": Sound.sfx("spot")
		"alert": Sound.sfx("alert")

## Every displayed frame.
func frame(dt: float) -> void:
	var sim := run.sim
	var shown: Sim = _preview if (_preview_frame >= 0 and _preview != null and state != S.PLAY) else sim
	var a := Engine.get_physics_interpolation_fraction() if state == S.PLAY or state == S.FINISH else 1.0
	view.render(shown, run, a, dt, run.live)
	Sound.layers(run.rec.size(), sim.frame, run.loop_frames, state == S.PLAY or state == S.FINISH)
	var left := maxf(0.0, (run.loop_frames - shown.frame) / 60.0)
	_top_timer.text = "%05.2f" % left
	_top_timer.label_settings.font_color = Kit.RED if left < 5.0 else Kit.CREAM
	_top_loop.text = I18n.t("heist.loop_line", {"n": run.loops, "echo": run.rec.size(), "budget": run.budget})
	_opp.text = opp_text
	_timeline.now = sim.frame
	_timeline.live = run.live
	_timeline.marks = _marks
	_timeline.alarm_frame = _alarm_frame
	_timeline.queue_redraw()
	_update_prompt(sim)

func duel_result(won: bool) -> void:
	if won:
		opp_text = I18n.t("online.duel_won")
	else:
		opp_text = I18n.t("online.duel_lost")
		banner(I18n.t("online.duel_lost"), "", Kit.MAGENTA, 2.0)

func _update_prompt(sim: Sim) -> void:
	_prompt.visible = false
	if state != S.PLAY and state != S.READY:
		return
	if run.live.is_empty() or run.live[0] >= sim.echoes.size():
		return
	var e: Sim.Echo = sim.echoes[run.live[0]]
	var text := ""
	for g in sim.guards:
		if g.mode != 3:
			var dx := e.x - g.x
			var dy := e.y - g.y
			if dx * dx + dy * dy < Sim.TAKEDOWN * Sim.TAKEDOWN and dx * Fx.cosa(g.face) + dy * Fx.sina(g.face) <= 0:
				text = I18n.t("act.takedown")
	if text == "":
		var best := -1
		var bd := Sim.REACH * Sim.REACH
		for vi in sim.devs.size():
			var v: Sim.Dev = sim.devs[vi]
			if v.gone or not (v.t in ["lever", "button", "vlever", "door", "term", "safe", "card", "loot"]):
				continue
			if v.t == "door" and not (v.k == "door" or (v.k == "key" and e.item == "card:%d" % v.color)):
				continue
			var dx := v.x * 256 + 128 - e.x
			var dy := v.y * 256 + 128 - e.y
			if dx * dx + dy * dy < bd:
				bd = dx * dx + dy * dy
				best = vi
		if best >= 0:
			var v: Sim.Dev = sim.devs[best]
			var k := v.t
			if v.t == "door":
				k = "door_close" if v.on else "door_open"
			if (v.t == "card" or v.t == "loot") and e.item != "":
				k = ""
			if k != "":
				text = I18n.t("act.%s" % k)
		elif e.item != "":
			text = I18n.t("act.drop")
	if text == "":
		return
	_prompt.text = "[%s] %s" % [Keys.key_name("use"), text]
	_prompt.reset_size()
	var sp := view.project(View.pos(e.x, e.y, 1.9))
	_prompt.position = sp - Vector2(_prompt.size.x / 2, _prompt.size.y)
	_prompt.visible = true

## One echo of the budget: free, used, or being played.
class EchoDot extends Control:
	var color := Color.WHITE
	var state := 0
	func _init() -> void:
		custom_minimum_size = Vector2(26, 26)
		mouse_filter = Control.MOUSE_FILTER_IGNORE
	func _draw() -> void:
		var c := Vector2(13, 13)
		match state:
			0: draw_arc(c, 9, 0, TAU, 24, Color(color.r, color.g, color.b, 0.4), 1.5)
			1: draw_circle(c, 9, Color(color.r, color.g, color.b, 0.75))
			2:
				draw_circle(c, 10, color)
				draw_arc(c, 13, 0, TAU, 24, Color(1, 1, 1, 0.9), 2.0)

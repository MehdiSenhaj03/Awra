class_name DuelScreen
extends Screen
## A duel: pens (mouse, pads), recognition, the simulation at 60 Hz, the computer rival or the other player
## (same screen or online), the illuminated HUD and the effects of every event.
##
## cfg: {mode: "ai"|"local"|"net", rival, skill, known: [glyphs], arena, best_of, chapter, host_left: bool,
##       names: [left, right], portraits: [left, right]}

signal finished(r: Dictionary)

const RIVAL_FIG := {"cendres": 0, "marees": 1, "horloger": 2, "aldric": 3}

var cfg: Dictionary
var duel: Duel
var arena: Arena
var ai: DuelAI
var ai0: DuelAI                  # automation: the left side played by the computer too
var mode := "ai"
var me := 0                      # the local side (net)
var pens: Array[Pen] = []
var pen_side: Array[int] = []
var known: Array = []
var online: Node = null
var _queue: Array = [[], []]     # casts waiting for the next frame, per side
var _pause := 0
var _over_t := -1.0
var _sent := false
var _result: Dictionary = {}
var _remote_strokes: Array = []
var _hud: Control
var _round_lbl: Label
var _banner: Label
var _banner_sub: Label
var _hint: RichTextLabel
var _timer: Label
var _strip: Control

func _init(c: Dictionary) -> void:
	super()
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	cfg = c
	mode = c.get("mode", "ai")
	duel = Duel.new()
	duel.best_of = int(c.get("best_of", 3))
	known = c.get("known", range(Glyphs.COUNT))
	var rival: String = c.get("rival", "cendres")
	arena = Arena.new(duel, c.get("arena", "ar_scriptorium"), Kit.tex("fig_ysolde"), _rival_tex(rival))
	add_child(arena)
	if mode == "ai":
		ai = DuelAI.new(duel, 1, float(c.get("skill", 1.0)), int(c.get("seed", 7)), rival, c.get("ai_known", known))
		if c.get("autoplay", false):
			ai0 = DuelAI.new(duel, 0, 1.0, int(c.get("seed", 7)) * 3 + 1, "", known)
		_add_pen("mouse", 0)
		for dv in Input.get_connected_joypads():
			_add_pen("pad%d" % dv, 0)
			break
	elif mode == "local":
		_add_pen("mouse", 0)
		var pads := Input.get_connected_joypads()
		_add_pen("pad%d" % (pads[0] if pads.size() > 0 else 0), 1)
	else:
		me = 0 if c.get("host", true) else 1
		_add_pen("mouse", me)
	_build_hud()

func _rival_tex(rival: String) -> Texture2D:
	if rival == "reine":
		return Kit.tex("fig_reine")
	var t := Kit.tex("fig_rival_%s" % rival)
	return t

func _add_pen(src: String, side: int) -> void:
	var p := Pen.new(src)
	var s := side
	p.glyph.connect(func(strokes: Array, center: Vector2, size: float) -> void: _on_glyph(s, strokes, center, size))
	p.step.connect(func(d: int) -> void: _queue[s].append({"step": d}))
	pens.append(p)
	pen_side.append(side)

# ---- HUD -------------------------------------------------------------------------------------------------------------

func _build_hud() -> void:
	_hud = Control.new()
	Kit.full(_hud)
	_hud.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_hud)
	_hud.draw.connect(_draw_hud)
	var names: Array = cfg.get("names", [I18n.t("names.ysolde"), I18n.t("names.%s" % cfg.get("rival", "cendres"))])
	var pts: Array = cfg.get("portraits", ["pt_ysolde", "pt_%s" % cfg.get("rival", "cendres")])
	for i in 2:
		var pt := Kit.image(self, pts[i], false)
		pt.size = Vector2(150, 150)
		pt.position = Vector2(26, 8) if i == 0 else Vector2(Kit.W - 176, 8)
		pt.flip_h = i == 1
		var nm := Kit.label(self, names[i], 24, Kit.IVORY, "display", 700, 0.08, Color(0, 0, 0, 0.9), 8)
		nm.position = Vector2(190, 18) if i == 0 else Vector2(Kit.W - 190 - 560, 18)
		nm.size = Vector2(560, 40)
		nm.horizontal_alignment = HORIZONTAL_ALIGNMENT_LEFT if i == 0 else HORIZONTAL_ALIGNMENT_RIGHT
	_timer = Kit.label(self, "", 34, Kit.IVORY, "deco", 400, 0.06, Color(0, 0, 0, 0.9), 10)
	_timer.position = Vector2(0, 26)
	_timer.size = Vector2(Kit.W, 50)
	_timer.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_round_lbl = Kit.label(self, "", 15, Kit.BRASS, "display", 700, 0.3, Color(0, 0, 0, 0.9), 6)
	_round_lbl.position = Vector2(0, 78)
	_round_lbl.size = Vector2(Kit.W, 26)
	_round_lbl.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_banner = Kit.label(self, "", 84, Kit.GOLD, "deco", 400, 0.04, Color(0.4, 0.1, 0.02, 0.8), 26)
	_banner.position = Vector2(0, Kit.H * 0.3)
	_banner.size = Vector2(Kit.W, 120)
	_banner.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_banner_sub = Kit.label(self, "", 26, Kit.IVORY, "body", 700, 0.03, Color(0, 0, 0, 0.9), 10)
	_banner_sub.position = Vector2(0, Kit.H * 0.3 + 120)
	_banner_sub.size = Vector2(Kit.W, 40)
	_banner_sub.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_hint = Kit.rich(self, Kit.html(I18n.t("duel.hint", {"up": Keys.key_name("step_up"), "down": Keys.key_name("step_down"), "book": Keys.key_name("book")})), 16, Kit.SOFT)
	_hint.position = Vector2(80, Kit.H - 40)
	_hint.size = Vector2(Kit.W - 160, 30)
	_hint.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_show_banner(I18n.t("duel.round", {"n": 1}), I18n.t("duel.draw_to_cast"), 2.2)

func _show_banner(a: String, b: String, hold: float) -> void:
	_banner.text = a
	_banner_sub.text = b
	_banner.modulate.a = 0.0
	_banner_sub.modulate.a = 0.0
	var tw := create_tween()
	tw.tween_property(_banner, "modulate:a", 1.0, 0.35)
	tw.parallel().tween_property(_banner_sub, "modulate:a", 1.0, 0.5)
	if hold > 0.0:
		tw.tween_interval(hold)
		tw.tween_property(_banner, "modulate:a", 0.0, 0.5)
		tw.parallel().tween_property(_banner_sub, "modulate:a", 0.0, 0.5)

func _draw_hud() -> void:
	var ci := _hud
	for i in 2:
		var m := duel.mages[i]
		var x := 190.0 if i == 0 else Kit.W - 190.0 - 560.0
		_bar(ci, Rect2(x, 64, 560, 22), float(m.hp) / Duel.HP, Color("#c8322a"), Color("#ff8a5a"), i == 1)
		var ink_r := Rect2(x, 94, 448, 12) if i == 0 else Rect2(x + 112, 94, 448, 12)
		_bar(ci, ink_r, float(m.ink) / Duel.INK, Color("#1f4fb0"), Color("#79b6ff"), i == 1)
		# Rounds won.
		for k in (duel.best_of + 1) / 2:
			var cx := x + (14.0 + k * 30.0 if i == 0 else 560.0 - 14.0 - k * 30.0)
			var on := k < m.rounds
			ci.draw_circle(Vector2(cx, 128), 10, Color(0.1, 0.06, 0.03, 0.8))
			ci.draw_arc(Vector2(cx, 128), 10, 0, TAU, 24, Kit.GOLD, 2.0, true)
			if on:
				ci.draw_circle(Vector2(cx, 128), 7, Kit.GOLD)
	# The live ink of every pen.
	for k in pens.size():
		var p := pens[k]
		var col: Color = Arena.SIDE_INK[pen_side[k]]
		GlyphArt.ink(ci, p.strokes, col, 8.0)
		if p.source != "mouse":
			ci.draw_circle(p.cursor, 9, Color(col.r, col.g, col.b, 0.9))
			ci.draw_arc(p.cursor, 16, 0, TAU, 24, col, 2.0, true)
	# The remote player's ink (online).
	if not _remote_strokes.is_empty():
		GlyphArt.ink(ci, _remote_strokes, Arena.SIDE_INK[1 - me], 8.0)
	# The glyphs this mage knows, along the bottom.
	var n := known.size()
	var w := 74.0
	var x0 := (Kit.W - n * w) / 2.0
	for k in n:
		var g: int = known[k]
		var c := Vector2(x0 + k * w + w / 2.0, Kit.H - 92)
		var affordable: bool = duel.mages[pen_side[0] if pen_side.size() > 0 else 0].ink >= Duel.COST[g]
		ci.draw_circle(c, 30, Color(0.06, 0.03, 0.02, 0.7))
		ci.draw_arc(c, 30, 0, TAU, 32, Color(0.85, 0.65, 0.3, 0.6), 1.5, true)
		var col: Color = GlyphArt.COLORS[g]
		GlyphArt.draw(ci, g, c, 34, Color(col.r, col.g, col.b, 1.0 if affordable else 0.3), 2.5, 1.0, false)

func _bar(ci: Control, r: Rect2, k: float, a: Color, b: Color, rtl: bool) -> void:
	ci.draw_rect(r.grow(3), Color(0.08, 0.04, 0.02, 0.85))
	var f := r
	f.size.x = r.size.x * clampf(k, 0.0, 1.0)
	if rtl:
		f.position.x = r.end.x - f.size.x
	ci.draw_rect(f, a)
	ci.draw_rect(Rect2(f.position, Vector2(f.size.x, f.size.y * 0.4)), Color(b.r, b.g, b.b, 0.55))
	ci.draw_rect(r.grow(3), Color(0.9, 0.7, 0.35, 0.85), false, 2.0)

# ---- Input ------------------------------------------------------------------------------------------------------------

func _on_glyph(side: int, strokes: Array, center: Vector2, size: float) -> void:
	var r := Glyphs.recognize(strokes)
	var g: int = r.g
	var lane := arena.lane_of(center.y)
	if g >= 0 and not known.has(g):
		arena.fx.text(I18n.t("duel.unknown"), center, Color(1, 0.6, 0.5), 26)
		g = -1
	if g < 0:
		arena.fx.burst(center, Color(0.3, 0.2, 0.15, 0.8), 18, 300, 6)
		arena.fx.text(I18n.t("duel.fizzle"), center, Color(1, 0.55, 0.45), 30)
		Sound.sfx("fizzle")
		_queue[side].append({"g": -1, "lane": lane, "pow": 0})
		return
	var sf := clampf(lerpf(0.8, 1.25, (size - 150.0) / 300.0), 0.8, 1.25)
	var qf := lerpf(0.7, 1.15, clampf((float(r.score) - Glyphs.REJECT) / (1.0 - Glyphs.REJECT), 0.0, 1.0))
	var pw := clampi(int(100.0 * sf * qf), 50, 140)
	var col: Color = GlyphArt.COLORS[g]
	arena.fx.glyph(g, center, clampf(size * 0.7, 80, 260), col, 0.7)
	arena.fx.text("%s  %d%%" % [I18n.t("glyph.%s" % Glyphs.NAMES[g]), pw], center + Vector2(0, -90), col.lightened(0.3), 28)
	_queue[side].append({"g": g, "lane": lane, "pow": pw})

## Lockstep: this machine's input for a future frame.
func local_net_input() -> Array:
	var c: Dictionary = _queue[me].pop_front() if not _queue[me].is_empty() else {}
	return [CastCode.encode(c), 0]

func step_net(host_in: Array, guest_in: Array) -> void:
	_tick([CastCode.decode(int(host_in[0])), CastCode.decode(int(guest_in[0]))])

func step() -> void:
	var c0: Dictionary = _queue[0].pop_front() if not _queue[0].is_empty() else {}
	if ai0:
		c0 = ai0.update()
	var c1: Dictionary = {}
	if mode == "ai":
		c1 = ai.update()
	else:
		c1 = _queue[1].pop_front() if not _queue[1].is_empty() else {}
	_tick([c0, c1])

func _tick(casts: Array) -> void:
	if duel.over >= 0:
		return
	if duel.round_over >= 0:
		_pause += 1
		if _pause > 150:
			_pause = 0
			duel.next_round()
			_show_banner(I18n.t("duel.round", {"n": duel.round_no}), "", 1.4)
		return
	duel.step(casts)
	_events()

func _events() -> void:
	var fx := arena.fx
	for e in duel.events:
		match e.t:
			"cast":
				arena.cast_pose(int(e.side))
				var g: int = e.g
				var mp := arena.mage_pos(int(e.side))
				fx.ring(mp + Vector2(0, 40), GlyphArt.COLORS[g], 20, 160, 0.5, 5)
				Sound.sfx("g_%s" % Glyphs.NAMES[g])
			"combo":
				var lane: int = e.lane
				var p := Vector2(arena.mage_pos(int(e.side)).x, Arena.LANE_Y[lane])
				fx.ring(p, Kit.GOLD, 20, 260, 0.8, 8)
				fx.text(I18n.t("combo.%d" % int(e.kind)), Vector2(Kit.W / 2.0, Arena.LANE_Y[lane] - 120.0), Kit.GOLD, 46, 1.6)
				fx.shake = 0.5
				Sound.sfx("combo")
			"clash":
				var p := Vector2(Arena.sx(int(e.x)), Arena.LANE_Y[int(e.lane)] + Arena.FLY)
				fx.burst(p, GlyphArt.COLORS[mini(int(e.a), 11)], 16, 380, 6)
				fx.burst(p, GlyphArt.COLORS[mini(int(e.b), 11)], 16, 380, 6)
				fx.ring(p, Color(1, 0.95, 0.8), 10, 90, 0.35, 4)
				Sound.sfx("clash")
			"hit":
				var side: int = e.side
				arena.flash(side)
				var mp := arena.mage_pos(side)
				fx.burst(mp + Vector2(0, 30), Color(1.0, 0.4, 0.25), 26, 460, 7)
				if int(e.absorbed) > 0:
					fx.ring(mp + Vector2(0, 40), Color(1, 0.85, 0.45), 100, 140, 0.4, 6)
				if int(e.dmg) > 0:
					fx.text("-%d" % int(roundf(int(e.dmg) / 100.0)), mp + Vector2(0, -170), Color(1.0, 0.45, 0.35), 40)
				fx.shake = maxf(fx.shake, 0.35 + int(e.dmg) / 6000.0)
				Sound.sfx("hit" if int(e.dmg) > 0 else "block")
			"bolt":
				var lane: int = e.lane
				var a := Vector2(arena.mage_pos(int(e.side)).x, Arena.LANE_Y[lane] + Arena.FLY)
				var b := Vector2(arena.mage_pos(1 - int(e.side)).x, Arena.LANE_Y[lane] + Arena.FLY)
				fx.lightning(a, b, Color(1.0, 0.95, 0.5), 0.4, 6)
				fx.lightning(a, b, Color(0.8, 0.9, 1.0), 0.3, 3)
				Sound.sfx("thunder")
			"wall_hit":
				fx.burst(Vector2(Arena.sx(int(e.x)), Arena.LANE_Y[int(e.lane)] + Arena.FLY), Color(0.85, 0.75, 0.55), 10, 260, 5)
			"wall_break":
				fx.burst(Vector2(Arena.sx(int(e.x)), Arena.LANE_Y[int(e.lane)] + Arena.FLY), Color(0.85, 0.75, 0.55), 40, 520, 8)
				Sound.sfx("break")
			"reflect":
				fx.ring(Vector2(Arena.sx(int(e.x)), Arena.LANE_Y[int(e.lane)] + Arena.FLY), Color(0.7, 1.0, 0.85), 10, 120, 0.4, 5)
				Sound.sfx("g_wind", 1.3)
			"freeze":
				fx.burst(Vector2(Arena.sx(int(e.x)), Arena.LANE_Y[int(e.lane)] + Arena.FLY), Color(0.7, 0.95, 1.0), 30, 300, 6)
				Sound.sfx("g_ice")
			"sealed":
				var lane: int = e.lane
				var p := Vector2(arena.mage_pos(int(e.side)).x, Arena.LANE_Y[lane])
				fx.glyph(Glyphs.SEAL, p + Vector2(0, -60), 140, GlyphArt.COLORS[Glyphs.SEAL], 1.0)
				fx.text(I18n.t("duel.sealed"), p + Vector2(0, -200), GlyphArt.COLORS[Glyphs.SEAL], 34)
				Sound.sfx("seal")
			"mirror":
				fx.ring(arena.mage_pos(int(e.side)) + Vector2(0, 40), Color(0.85, 0.9, 1.0), 60, 200, 0.5, 6)
			"noink":
				if int(e.side) == me or mode != "net":
					fx.text(I18n.t("duel.noink"), arena.mage_pos(int(e.side)) + Vector2(0, -200), Color(0.5, 0.75, 1.0), 30)
			"fizzle":
				pass
			"step":
				Sound.sfx("step")
			"round":
				var w: int = e.winner
				var nm: String = (cfg.get("names", [I18n.t("names.ysolde"), I18n.t("names.%s" % cfg.get("rival", "cendres"))]) as Array)[w]
				if duel.over < 0:
					_show_banner(I18n.t("duel.round_to", {"name": nm}), "", 1.6)
				Sound.sfx("round")
			"over":
				_on_over(int(e.winner))

func _on_over(w: int) -> void:
	_over_t = 0.0
	var won := w == (me if mode == "net" else 0)
	var names: Array = cfg.get("names", [I18n.t("names.ysolde"), I18n.t("names.%s" % cfg.get("rival", "cendres"))])
	_show_banner(I18n.t("duel.victory") if won or mode == "local" else I18n.t("duel.defeat"), I18n.t("duel.winner", {"name": names[w]}), 0.0)
	Sound.sfx("win" if won else "lose")
	_result = {"mode": "duel", "won": won, "winner": w, "frame": duel.frame, "rounds": [duel.mages[0].rounds, duel.mages[1].rounds], "chapter": cfg.get("chapter", 0), "casts": duel.mages[0].casts}

# ---- Per frame -------------------------------------------------------------------------------------------------------------

func frame(dt: float) -> void:
	var mp := get_viewport().get_mouse_position()
	var drawing_ok := duel.round_over < 0 and duel.over < 0
	for p in pens:
		p.enabled = drawing_ok
		p.update(dt, mp)
	# Lane under the local nib.
	var hp := -1
	for k in pens.size():
		if pen_side[k] == (me if mode == "net" else 0) and not pens[k].strokes.is_empty():
			hp = arena.lane_of(pens[k].cursor.y)
	arena.hover_lane = hp
	if mode == "ai" and ai.drawing >= 0:
		arena.ghost = {"side": 1, "g": ai.drawing, "lane": ai.draw_lane, "progress": ai.progress()}
	elif ai0 and ai0.drawing >= 0:
		arena.ghost = {"side": 0, "g": ai0.drawing, "lane": ai0.draw_lane, "progress": ai0.progress()}
	else:
		arena.ghost = {}
	if online and mode == "net":
		_stream_strokes()
	var left := maxi(0, Duel.ROUND_FRAMES - duel.round_frame)
	_timer.text = "%d" % int(ceil(left / 60.0))
	var frenzy := duel.round_frame >= Duel.FRENZY and duel.round_over < 0
	_round_lbl.text = I18n.t("duel.frenzy") if frenzy else I18n.t("duel.round_small", {"n": duel.round_no})
	_round_lbl.label_settings.font_color = Color("#ff7a4a") if frenzy else Kit.BRASS
	Sound.tension(0.8 if frenzy else 0.0)
	_hud.queue_redraw()
	if _over_t >= 0.0:
		_over_t += dt
		if _over_t > 3.4 and not _sent:
			_sent = true
			finished.emit(_result)

var _stream_t := 0.0
func _stream_strokes() -> void:
	_stream_t += get_process_delta_time()
	if _stream_t < 0.05:
		return
	_stream_t = 0.0
	var pts: Array = []
	for k in pens.size():
		if pen_side[k] == me:
			for s: PackedVector2Array in pens[k].strokes:
				pts.append(s)
	online.call("send_strokes", pts)

func remote_strokes(pts: Array) -> void:
	_remote_strokes = pts

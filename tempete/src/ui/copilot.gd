class_name Copilot
extends RefCounted
## Eli on the radio: reads the simulation like a meteorologist and says what matters — a new cell, a rotation
## tightening, a tornado on the ground and where it heads, hail ahead, mud, a probe window, too close. Also keeps
## an intercept route (along the roads) for the radar.

signal say(text: String, urgent: bool)

var chase: Chase
var team := 0
var ai: ChaserAI
var route: Array[Vector2i] = []
var _said: Dictionary = {}
var _cd := 0.0
var _route_t := 0.0
var _mud_t := 0.0

func _init(c: Chase, t: int) -> void:
	chase = c
	team = t
	ai = ChaserAI.new(c, t, 1.0, 77)

static func compass(dx: float, dy: float) -> String:
	var a := fposmod(atan2(dy, dx), TAU)
	var names := ["E", "NE", "N", "NO", "O", "SO", "S", "SE"]
	var k := int(roundf(a / (TAU / 8.0))) % 8
	return I18n.t("dir.%s" % names[k])

func _once(key: String, text: String, urgent: bool = false) -> void:
	if _said.has(key):
		return
	_said[key] = true
	say.emit(text, urgent)

func update(dt: float) -> void:
	var v: Chase.Vehicle = chase.vehicles[team]
	if not v.alive:
		return
	_cd -= dt
	_route_t -= dt
	for e in chase.events:
		match e.t:
			"storm":
				var st := chase._storm(int(e.id))
				if st:
					var d := Vector2(st.x - v.x, st.y - v.y)
					say.emit(I18n.t("eli.new_cell", {"n": st.id, "dir": compass(d.x, d.y), "km": int(d.length() / 1e6)}), false)
			"tornado":
				var d := Vector2(int(e.x) - v.x, int(e.y) - v.y)
				var st := chase._storm(int(e.storm))
				var mdir := compass(st.vx, st.vy) if st else "?"
				say.emit(I18n.t("eli.tornado", {"km": "%.1f" % (d.length() / 1e6), "dir": compass(d.x, d.y), "to": mdir}), true)
			"tornado_end":
				say.emit(I18n.t("eli.tornado_end", {"n": e.storm}), false)
			"probe_hit":
				if int(e.team) == team:
					say.emit(I18n.t("eli.probe_hit", {"ef": e.ef, "pts": e.score}), true)
			"wreck":
				if int(e.team) == team:
					say.emit(I18n.t("eli.wreck"), true)
	# Rotation tightening, a tornado coming.
	for st in chase.storms:
		if st.dead:
			continue
		if st.rot() > 750:
			_once("rot%d" % st.id, I18n.t("eli.rotation", {"n": st.id}))
		for w: Array in st.windows:
			var dt_f := int(w[0]) - st.t
			if dt_f > 0 and dt_f < 60 * 25:
				_once("soon%d_%d" % [st.id, int(w[0])], I18n.t("eli.soon", {"n": st.id}), true)
	if _cd > 0.0:
		return
	# Dangers first.
	for tn in chase.tornadoes:
		if tn.dead:
			continue
		var d := Vector2(tn.x - v.x, tn.y - v.y).length()
		if d < maxf(1_400_000.0, tn.width * 2.5):
			say.emit(I18n.t("eli.too_close"), true)
			_cd = 5.0
			return
	if chase.hail_at(v.x, v.y) > 30:
		say.emit(I18n.t("eli.hail"), true)
		_cd = 7.0
		return
	if v.surface == Plains.S.DIRT and chase.plains.wet_at(v.x, v.y) > 300:
		_mud_t -= dt
		if _mud_t <= 0.0:
			_mud_t = 40.0
			say.emit(I18n.t("eli.mud"), false)
			_cd = 4.0
			return
	# A probe window.
	if v.probes > 0 and absi(v.v) < 6 * Chase.MS:
		for tn in chase.tornadoes:
			if tn.dead:
				continue
			var st := chase._storm(tn.storm)
			if st == null:
				continue
			var rel := Trig.rot(v.x - tn.x, v.y - tn.y, -st.motion())
			if rel.x > 600_000 and rel.x < 4_000_000 and absi(rel.y) < 350_000 + tn.width / 2:
				say.emit(I18n.t("eli.probe_here", {"key": Keys.key_name("probe")}), true)
				_cd = 8.0
				return
	# The route to the next intercept.
	if _route_t <= 0.0:
		_route_t = 12.0
		var t := ai.target()
		if not t.is_empty():
			var lead := 120 if t.kind == "tornado" else 180
			var goal := ai.intercept(t, lead, 1800 if t.kind == "tornado" else 3500)
			route = ai.plan(Vector2i(v.x, v.y), goal)
			_once("route_hint", I18n.t("eli.route"))

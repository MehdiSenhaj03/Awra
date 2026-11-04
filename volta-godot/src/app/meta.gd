class_name Meta
## Progression: Overtones (perks bought with Notes), weapon unlocks and upgrades, grades, the save and the settings.

const OVERTONES := [
	{"id": "sillage", "cost": 400, "icon": "a8"},
	{"id": "tenuto", "cost": 500, "icon": "a9"},
	{"id": "forte", "cost": 900, "icon": "a10"},
	{"id": "aimant", "cost": 300, "icon": "a11"},
	{"id": "reprise", "cost": 1200, "icon": "a12"},
	{"id": "frolement", "cost": 600, "icon": "a13"},
	{"id": "trille", "cost": 1400, "icon": "a14"},
	{"id": "ampleur", "cost": 700, "icon": "a15"},
	{"id": "orgue", "cost": 800, "icon": "b0"},
	{"id": "harmonie", "cost": 1500, "icon": "b1"},
	{"id": "allegro", "cost": 600, "icon": "b2"},
	{"id": "contrepoint", "cost": 1100, "icon": "b3"},
	{"id": "tempsfort", "cost": 1000, "icon": "b4"},
	{"id": "sourdine", "cost": 900, "icon": "b5"},
	{"id": "virtuose", "cost": 2000, "icon": "b6"},
	{"id": "metronome", "cost": 0, "icon": "b7"},
]
const WEAPON_UNLOCK := {"prism": "", "chord": "s1_4", "lance": "s2_4", "arc": "s3_4", "swarm": "s4_4"}
const WEAPON_UPGRADE := [0, 800, 2200]
const GRADES := ["D", "C", "B", "A", "S", "V"]
const SAVE_PATH := "user://save.json"
const SETTINGS_PATH := "user://settings.json"

static func overtone(id: String) -> Dictionary:
	for o in OVERTONES:
		if o.id == id:
			return o
	return {}

static func apply_overtone(id: String, m: Mods) -> void:
	match id:
		"tenuto": m.style_keep = 2.0
		"forte": m.damage *= 1.2
		"aimant": m.magnet = 2.2
		"ampleur": m.pulse_radius = 1.35
		"orgue": m.overdrive = 1.5
		"allegro": m.speed = 1.12
		_: m.has[id] = true

static func new_save() -> Dictionary:
	return {"version": 1, "stages": {}, "notes": 0, "weaponLevels": {"prism": 1}, "weapon": "prism", "owned": ["metronome"], "equipped": [], "fragments": [], "seenIntro": false, "ending": "", "endlessBest": 0, "endlessWave": 0, "hard": false, "deaths": 0, "playFrames": 0}

## Letter ranks of a stage result: D C B A S, and V (VOLTA) for an S with no hit and the top style rank.
static func grade(stage_id: String, score: float, hits: int, max_rank: int) -> int:
	var par: Array = Stages.get_def(stage_id).par
	var g := 0
	for i in 4:
		if score >= float(par[i]):
			g = i + 1
	if g == 4 and hits == 0 and max_rank >= 6:
		g = 5
	return g

static func cleared(s: Dictionary, id: String) -> bool:
	var r: Variant = s.stages.get(id)
	return r != null and bool(r.cleared)

static func stage_open(s: Dictionary, id: String) -> bool:
	var d := Stages.get_def(id)
	if int(d.index) > 0:
		return cleared(s, "s%d_%d" % [d.sector, d.index])
	if int(d.sector) == 1:
		return true
	return cleared(s, "s%d_4" % (int(d.sector) - 1))

static func weapon_unlocked(s: Dictionary, w: String) -> bool:
	return WEAPON_UNLOCK[w] == "" or cleared(s, WEAPON_UNLOCK[w])

static func pulse_unlocked(s: Dictionary) -> bool:
	return cleared(s, "s1_1")

static func overdrive_unlocked(s: Dictionary) -> bool:
	return cleared(s, "s1_4")

static func slots(s: Dictionary) -> int:
	return 2 + (1 if cleared(s, "s2_4") else 0) + (1 if cleared(s, "s4_4") else 0)

static func endless_unlocked(s: Dictionary) -> bool:
	return cleared(s, "s3_4")

static func build_mods(s: Dictionary) -> Mods:
	var m := Mods.new()
	for id: String in s.equipped:
		apply_overtone(id, m)
	return m

static func _read(path: String) -> Variant:
	if not FileAccess.file_exists(path):
		return null
	var f := FileAccess.open(path, FileAccess.READ)
	if f == null:
		return null
	return JSON.parse_string(f.get_as_text())

static func _write(path: String, v: Variant) -> void:
	var tmp := path + ".tmp"
	var f := FileAccess.open(tmp, FileAccess.WRITE)
	if f == null:
		return
	f.store_string(JSON.stringify(v))
	f.close()
	DirAccess.rename_absolute(ProjectSettings.globalize_path(tmp), ProjectSettings.globalize_path(path))

static func load_save() -> Dictionary:
	var s := new_save()
	var raw: Variant = _read(SAVE_PATH)
	if raw is Dictionary:
		s.merge(raw, true)
	return s

static func write_save(s: Dictionary) -> void:
	_write(SAVE_PATH, s)

static func load_settings() -> Dictionary:
	var lang := "fr" if OS.get_locale_language() == "fr" else "en"
	var d := {"music": 0.7, "sfx": 0.8, "shake": 1.0, "flashes": 1.0, "latency": 0.0, "lang": lang, "bindings": null, "fullscreen": true, "quality": 2}
	var raw: Variant = _read(SETTINGS_PATH)
	if raw is Dictionary:
		d.merge(raw, true)
	return d

static func write_settings(s: Dictionary) -> void:
	_write(SETTINGS_PATH, s)

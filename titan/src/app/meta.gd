class_name Meta
## The Long March: nine legs over three generations (at each crossroads, a choice of land), a wild titan at the end
## of each generation; the four lineages of the crew (portraits, names, gifts passed on), Ama's health and scars,
## supplies carried from leg to leg; the save and the settings.

const LEGS := [
	{"id": "l1", "gen": 0, "choices": ["ash", "ruins"], "level": 1, "length_m": 2200},
	{"id": "l2", "gen": 0, "choices": ["forest", "oasis"], "level": 1, "length_m": 2400},
	{"id": "l3", "gen": 0, "choices": ["ash"], "level": 1, "length_m": 1400, "boss": "belier"},
	{"id": "l4", "gen": 1, "choices": ["ruins", "storm"], "level": 2, "length_m": 2600},
	{"id": "l5", "gen": 1, "choices": ["oasis", "forest"], "level": 2, "length_m": 2600},
	{"id": "l6", "gen": 1, "choices": ["storm"], "level": 2, "length_m": 1600, "boss": "fournaise"},
	{"id": "l7", "gen": 2, "choices": ["storm", "ruins"], "level": 3, "length_m": 2800},
	{"id": "l8", "gen": 2, "choices": ["forest", "oasis"], "level": 3, "length_m": 2800},
	{"id": "l9", "gen": 2, "choices": ["ash"], "level": 3, "length_m": 1800, "boss": "roi"},
]
const BOSSES := {
	"belier": {"hp": 14000, "power": 92, "level": 1},
	"fournaise": {"hp": 19000, "power": 105, "level": 2},
	"roi": {"hp": 25000, "power": 118, "level": 4},
}
## The lineages: who holds each post in each generation (portraits from the art), and their elders.
const LINES := [
	{"role": March.P.PILOT, "names": ["Ysa", "Tamo", "Lysa"], "pt": ["portrait_0", "heir_0", "heir_4"], "elder": "portrait_1", "gift": "steady"},
	{"role": March.P.GUN_L, "names": ["Kader", "Nour", "Ilan"], "pt": ["portrait_2", "heir_1", "heir_5"], "elder": "portrait_3", "gift": "quick"},
	{"role": March.P.HEART, "names": ["Mira", "Sefa", "Oren"], "pt": ["portrait_4", "heir_2", "heir_6"], "elder": "portrait_5", "gift": "frugal"},
	{"role": March.P.LOOKOUT, "names": ["Pip", "Pip", "Aïa"], "pt": ["portrait_6", "portrait_7", "heir_7"], "elder": "portrait_7", "gift": "keen"},
]
const NARRATOR := "heir_3"
const GIFTS := ["steady", "quick", "frugal", "keen", "stout", "brave"]
const SAVE_PATH := "user://save.json"
const SETTINGS_PATH := "user://settings.json"
const REST := 1200                 # health regained between two legs
const MAX_SCARS := 3000

static func leg(i: int) -> Dictionary:
	return LEGS[clampi(i, 0, LEGS.size() - 1)]

static func new_run() -> Dictionary:
	var gifts: Array = []
	for l: Dictionary in LINES:
		gifts.append(l.gift)
	return {"leg": 0, "kinds": [], "hp": March.HP, "supplies": 800, "scars": 0, "gifts": gifts, "start": {}}

static func new_save() -> Dictionary:
	return {"version": 1, "run": null, "best": 0, "done": false, "seen": {}, "crew_seen": 0}

static func max_hp(run: Dictionary) -> int:
	return March.HP - int(run.get("scars", 0))

static func gen(run: Dictionary) -> int:
	return int(leg(int(run.leg)).gen)

## The leg configuration for the simulation (with the chosen land).
static func leg_cfg(run: Dictionary, kind: String) -> Dictionary:
	var i := int(run.leg)
	var l := leg(i)
	var c := {"kind": kind, "length_m": l.length_m, "seed": 1000 + i * 37 + kind.hash() % 97, "level": l.level,
		"hp": int(run.hp), "supplies": int(run.supplies)}
	if l.has("boss"):
		var b: Dictionary = BOSSES[l.boss]
		c.boss = true
		c.foe_hp = b.hp
		c.foe_power = b.power
		c.level = b.level
		c.boss_id = l.boss
	return c

## The crew for the simulation: roles and the gifts of this generation.
static func crew_info(run: Dictionary) -> Array:
	var out: Array = []
	for k in LINES.size():
		var l: Dictionary = LINES[k]
		out.append({"role": l.role, "gift": run.gifts[k], "age": 20 + gen(run) * 20})
	return out

static func crew_name(run: Dictionary, k: int) -> String:
	return LINES[k].names[gen(run)]

static func crew_pt(g: int, k: int) -> String:
	return LINES[k].pt[clampi(g, 0, 2)]

## A leg is over: carries Ama's state on. Returns what changed.
static func leg_done(run: Dictionary, hp: int, supplies: int) -> Dictionary:
	var start_hp := int(run.hp)
	var lost := maxi(0, start_hp - hp)
	run.scars = mini(MAX_SCARS, int(run.scars) + lost / 8)
	var was_gen := gen(run)
	run.leg = int(run.leg) + 1
	run.supplies = clampi(supplies, 100, 1000)
	var heal := REST
	if int(run.leg) < LEGS.size() and gen(run) != was_gen:
		heal = March.HP
	run.hp = mini(max_hp(run), hp + heal)
	run.start = {}
	return {"lost": lost, "scars": lost / 8, "new_gen": int(run.leg) < LEGS.size() and gen(run) != was_gen}

static func _read(path: String) -> Variant:
	if not FileAccess.file_exists(path):
		return null
	var f := FileAccess.open(path, FileAccess.READ)
	return JSON.parse_string(f.get_as_text()) if f else null

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
	var d := {"music": 0.7, "sfx": 0.8, "lang": lang, "bindings": null, "fullscreen": true, "quality": 2, "shake": 1.0, "mouse": 1.0}
	var raw: Variant = _read(SETTINGS_PATH)
	if raw is Dictionary:
		d.merge(raw, true)
	return d

static func write_settings(s: Dictionary) -> void:
	_write(SETTINGS_PATH, s)

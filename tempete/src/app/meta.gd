class_name Meta
## The Season (eight chase days, April to June), the garage (upgrades bought with the dossiers' earnings), settings.

## A day: the forecast risk, the storms (position in mm, severity 1..5, direction in 1/4096 turn, speed m/s,
## tornadoes), a rival team or not, and the dossier needed to call the day a success.
const DAYS := [
	{"id": "d1", "risk": 1, "seed": 11, "minutes": 8, "rival": false, "goal": 350, "storms": [
		{"at": 0, "x": 2_800_000, "y": 3_200_000, "sev": 2, "dir": 560, "speed": 12, "tornadoes": 1}]},
	{"id": "d2", "risk": 2, "seed": 23, "minutes": 9, "rival": false, "goal": 500, "storms": [
		{"at": 0, "x": 2_200_000, "y": 2_400_000, "sev": 2, "dir": 600, "speed": 13, "tornadoes": 1},
		{"at": 5_400, "x": 1_200_000, "y": 6_800_000, "sev": 3, "dir": 520, "speed": 14, "tornadoes": 1}]},
	{"id": "d3", "risk": 2, "seed": 37, "minutes": 9, "rival": true, "goal": 650, "storms": [
		{"at": 0, "x": 2_600_000, "y": 3_600_000, "sev": 3, "dir": 540, "speed": 14, "tornadoes": 1}]},
	{"id": "d4", "risk": 3, "seed": 41, "minutes": 9, "rival": false, "goal": 850, "storms": [
		{"at": 0, "x": 2_000_000, "y": 2_800_000, "sev": 3, "dir": 560, "speed": 14, "tornadoes": 2},
		{"at": 7_200, "x": 1_500_000, "y": 7_000_000, "sev": 3, "dir": 500, "speed": 15, "tornadoes": 1}]},
	{"id": "d5", "risk": 3, "seed": 53, "minutes": 9, "rival": true, "goal": 1000, "dusk": true, "storms": [
		{"at": 0, "x": 2_400_000, "y": 3_000_000, "sev": 4, "dir": 580, "speed": 13, "tornadoes": 1}]},
	{"id": "d6", "risk": 4, "seed": 67, "minutes": 10, "rival": false, "goal": 1200, "storms": [
		{"at": 0, "x": 2_200_000, "y": 2_600_000, "sev": 4, "dir": 560, "speed": 15, "tornadoes": 2},
		{"at": 6_000, "x": 1_000_000, "y": 6_200_000, "sev": 3, "dir": 520, "speed": 15, "tornadoes": 1}]},
	{"id": "d7", "risk": 4, "seed": 79, "minutes": 10, "rival": true, "goal": 1350, "storms": [
		{"at": 0, "x": 1_800_000, "y": 3_400_000, "sev": 4, "dir": 540, "speed": 16, "tornadoes": 2},
		{"at": 4_800, "x": 2_600_000, "y": 1_200_000, "sev": 3, "dir": 620, "speed": 14, "tornadoes": 1}]},
	{"id": "d8", "risk": 5, "seed": 97, "minutes": 10, "rival": true, "goal": 1600, "storms": [
		{"at": 0, "x": 2_000_000, "y": 3_000_000, "sev": 5, "dir": 560, "speed": 15, "tornadoes": 3}]},
]
## The garage: levels and their effects, costs in dollars.
const UPGRADES := {
	"armor": {"values": [100, 80, 62, 45], "costs": [0, 1200, 2500, 4200]},
	"engine": {"values": [100, 110, 120, 130], "costs": [0, 1000, 2200, 3800]},
	"probes": {"values": [3, 4, 5], "costs": [0, 1500, 3000]},
	"zoom": {"values": [4, 6, 8], "costs": [0, 900, 2000]},
}
const SAVE_PATH := "user://save.json"
const SETTINGS_PATH := "user://settings.json"

static func day(i: int) -> Dictionary:
	return DAYS[clampi(i, 0, DAYS.size() - 1)]

static func day_index(id: String) -> int:
	for i in DAYS.size():
		if DAYS[i].id == id:
			return i
	return 0

static func new_save() -> Dictionary:
	return {"version": 1, "days": {}, "money": 600, "levels": {"armor": 0, "engine": 0, "probes": 0, "zoom": 0}, "seen": {}, "album": []}

static func done(s: Dictionary, i: int) -> bool:
	var id: String = DAYS[i].id
	return s.days.has(id) and bool(s.days[id].get("ok", false))

static func is_open(s: Dictionary, i: int) -> bool:
	return i <= 0 or done(s, i - 1)

static func upgrades(s: Dictionary) -> Dictionary:
	var out := {}
	for k: String in UPGRADES:
		var lv: int = int(s.levels.get(k, 0))
		out[k] = UPGRADES[k].values[clampi(lv, 0, UPGRADES[k].values.size() - 1)]
	return out

## Records a day's dossier; returns the money earned.
static func record(s: Dictionary, i: int, total: int, alive: bool, hp: int) -> Dictionary:
	var d := day(i)
	var id: String = d.id
	var ok := total >= int(d.goal) and alive
	var prev: Dictionary = s.days.get(id, {})
	var earned := total * 3 / 2
	var repair := (10000 - hp) / 8 if alive else 1600
	var net := maxi(0, earned - repair)
	s.money = int(s.money) + net
	s.days[id] = {"ok": ok or bool(prev.get("ok", false)), "best": maxi(int(prev.get("best", 0)), total), "tries": int(prev.get("tries", 0)) + 1}
	return {"ok": ok, "earned": earned, "repair": repair, "net": net}

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
	var d := {"music": 0.7, "sfx": 0.8, "lang": lang, "bindings": null, "fullscreen": true, "quality": 2}
	var raw: Variant = _read(SETTINGS_PATH)
	if raw is Dictionary:
		d.merge(raw, true)
	return d

static func write_settings(s: Dictionary) -> void:
	_write(SETTINGS_PATH, s)

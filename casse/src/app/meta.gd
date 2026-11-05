class_name Meta
## Progression and persistence: the campaign (districts, vaults, medals), the workshop's vaults, settings.

const DISTRICTS := [
	{"id": 1, "vaults": ["1-1", "1-2", "1-3", "1-4", "1-5", "1-6"]},
	{"id": 2, "vaults": ["2-1", "2-2", "2-3", "2-4", "2-5"]},
	{"id": 3, "vaults": ["3-1", "3-2", "3-3", "3-4", "3-5"]},
	{"id": 4, "vaults": ["4-1", "4-2", "4-3", "4-4"]},
	{"id": 5, "vaults": ["5-1", "5-2"]},
]
const SAVE_PATH := "user://save.json"
const SETTINGS_PATH := "user://settings.json"

static func order() -> Array:
	var out: Array = []
	for d: Dictionary in DISTRICTS:
		out.append_array(d.vaults)
	return out

static func district_of(id: String) -> int:
	return int(id.split("-")[0])

static func level(id: String) -> Level:
	var d: Variant = JSON.parse_string(FileAccess.get_file_as_string("res://levels/campaign/%s.json" % id))
	return Level.new(d)

static func new_save() -> Dictionary:
	return {"version": 1, "vaults": {}, "seen": {}, "workshop": [], "deaths": 0, "frames": 0}

static func done(s: Dictionary, id: String) -> bool:
	return s.vaults.has(id) and int(s.vaults[id].medal) > 0

static func is_open(s: Dictionary, id: String) -> bool:
	var o := order()
	var i := o.find(id)
	return i <= 0 or done(s, o[i - 1])

static func medal(s: Dictionary, id: String) -> int:
	return int(s.vaults[id].medal) if s.vaults.has(id) else 0

## Records a result; keeps the best of each.
static func record(s: Dictionary, id: String, r: Dictionary) -> bool:
	var prev: Dictionary = s.vaults.get(id, {})
	var better := prev.is_empty() or int(r.echoes) < int(prev.get("echoes", 99)) or (int(r.echoes) == int(prev.get("echoes", 99)) and int(r.frame) < int(prev.get("frame", 999999)))
	s.vaults[id] = {
		"medal": maxi(int(prev.get("medal", 0)), int(r.medal)),
		"echoes": mini(int(prev.get("echoes", 99)), int(r.echoes)),
		"frame": int(r.frame) if better else int(prev.get("frame", r.frame)),
		"gems": maxi(int(prev.get("gems", 0)), int(r.gems)),
		"replay": r.replay if better else prev.get("replay", r.replay),
	}
	return better

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
	var d := {"music": 0.7, "sfx": 0.8, "shake": 1.0, "lang": lang, "bindings": null, "fullscreen": true, "quality": 2, "name": "Odile"}
	var raw: Variant = _read(SETTINGS_PATH)
	if raw is Dictionary:
		d.merge(raw, true)
	return d

static func write_settings(s: Dictionary) -> void:
	_write(SETTINGS_PATH, s)

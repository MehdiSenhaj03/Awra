class_name Meta
## Progression and persistence: the Season (nine nights), settings.

## The nine nights of the Season: where, who you play, how sharp the other side is, which missions.
const NIGHTS := [
	{"id": 1, "venue": "salon", "role": "spy", "skill": 0.55, "missions": ["letter", "poison", "contact"], "need": 2},
	{"id": 2, "venue": "salon", "role": "hunter", "skill": 0.6, "missions": ["letter", "poison", "bug", "contact"], "need": 3},
	{"id": 3, "venue": "salon", "role": "spy", "skill": 0.85, "missions": ["letter", "poison", "bug", "contact"], "need": 3},
	{"id": 4, "venue": "serre", "role": "hunter", "skill": 0.9, "missions": ["letter", "poison", "bug", "contact"], "need": 3},
	{"id": 5, "venue": "serre", "role": "spy", "skill": 1.05, "missions": ["letter", "poison", "bug", "countess"], "need": 3},
	{"id": 6, "venue": "serre", "role": "hunter", "skill": 1.1, "missions": ["letter", "poison", "pocket", "contact"], "need": 3},
	{"id": 7, "venue": "opera", "role": "spy", "skill": 1.25, "missions": ["letter", "poison", "bug", "pocket", "countess"], "need": 3},
	{"id": 8, "venue": "opera", "role": "hunter", "skill": 1.3, "missions": ["letter", "poison", "bug", "countess", "contact"], "need": 3},
	{"id": 9, "venue": "opera", "role": "spy", "skill": 1.7, "missions": ["letter", "poison", "bug", "pocket", "countess"], "need": 4},
]
const SAVE_PATH := "user://save.json"
const SETTINGS_PATH := "user://settings.json"

static func night(n: int) -> Dictionary:
	return NIGHTS[clampi(n - 1, 0, NIGHTS.size() - 1)]

static func new_save() -> Dictionary:
	return {"version": 1, "nights": {}, "seen": {}, "stats": {"shots": 0, "hits": 0, "missions": 0}}

static func done(s: Dictionary, n: int) -> bool:
	return s.nights.has(str(n)) and bool(s.nights[str(n)].get("won", false))

static func is_open(s: Dictionary, n: int) -> bool:
	return n <= 1 or done(s, n - 1)

## Records a result; keeps the best (fastest win).
static func record(s: Dictionary, n: int, r: Dictionary) -> bool:
	var key := str(n)
	var prev: Dictionary = s.nights.get(key, {})
	var won: bool = r.won
	var better := won and (not bool(prev.get("won", false)) or int(r.frame) < int(prev.get("frame", 1 << 30)))
	s.nights[key] = {
		"won": won or bool(prev.get("won", false)),
		"frame": int(r.frame) if better else int(prev.get("frame", r.frame if won else 1 << 30)),
		"tries": int(prev.get("tries", 0)) + 1,
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
	var d := {"music": 0.7, "sfx": 0.8, "shake": 1.0, "lang": lang, "bindings": null, "fullscreen": true, "quality": 2, "sens": 1.0}
	var raw: Variant = _read(SETTINGS_PATH)
	if raw is Dictionary:
		d.merge(raw, true)
	return d

static func write_settings(s: Dictionary) -> void:
	_write(SETTINGS_PATH, s)

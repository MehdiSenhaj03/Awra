class_name Meta
## Progression and persistence: the Apprentice's ten chapters, the glyphs learned, settings.

## Each chapter: a duel or an expedition on the Two Banks, the glyphs it teaches, the opponent.
const CHAPTERS := [
	{"id": 1, "kind": "duel", "rival": "aldric", "skill": 0.25, "arena": "ar_scriptorium", "teach": [Glyphs.FIRE, Glyphs.WATER, Glyphs.SHIELD], "best_of": 1},
	{"id": 2, "kind": "rives", "level": 0, "teach": [Glyphs.WALL, Glyphs.WIND]},
	{"id": 3, "kind": "duel", "rival": "cendres", "skill": 0.45, "arena": "ar_cendres", "teach": [Glyphs.EARTH], "best_of": 3},
	{"id": 4, "kind": "rives", "level": 1, "teach": [Glyphs.ICE]},
	{"id": 5, "kind": "duel", "rival": "marees", "skill": 0.6, "arena": "ar_marees", "teach": [Glyphs.BOLT], "best_of": 3},
	{"id": 6, "kind": "rives", "level": 2, "teach": [Glyphs.STAR, Glyphs.HEAL]},
	{"id": 7, "kind": "duel", "rival": "horloger", "skill": 0.75, "arena": "ar_horloge", "teach": [Glyphs.TIME], "best_of": 3},
	{"id": 8, "kind": "rives", "level": 3, "boss": true, "teach": [Glyphs.SEAL]},
	{"id": 9, "kind": "duel", "rival": "aldric", "skill": 0.9, "arena": "ar_scriptorium", "teach": [], "best_of": 3},
	{"id": 10, "kind": "duel", "rival": "reine", "skill": 1.05, "arena": "ar_reine", "teach": [], "best_of": 3},
]
const SAVE_PATH := "user://save.json"
const SETTINGS_PATH := "user://settings.json"

static func chapter(n: int) -> Dictionary:
	return CHAPTERS[clampi(n - 1, 0, CHAPTERS.size() - 1)]

## Glyphs known when playing chapter n (everything taught up to and including it).
static func known_at(n: int) -> Array:
	var out: Array = []
	for c: Dictionary in CHAPTERS:
		if int(c.id) <= n:
			for g: int in c.teach:
				if not out.has(g):
					out.append(g)
	return out

static func new_save() -> Dictionary:
	return {"version": 1, "chapters": {}, "seen": {}, "lessons": {}}

static func done(s: Dictionary, n: int) -> bool:
	return s.chapters.has(str(n)) and bool(s.chapters[str(n)].get("won", false))

static func is_open(s: Dictionary, n: int) -> bool:
	return n <= 1 or done(s, n - 1)

static func all_known(s: Dictionary) -> Array:
	var n := 1
	for c: Dictionary in CHAPTERS:
		if done(s, int(c.id)):
			n = int(c.id) + 1
	return known_at(mini(n, CHAPTERS.size()))

static func record(s: Dictionary, n: int, r: Dictionary) -> void:
	var key := str(n)
	var prev: Dictionary = s.chapters.get(key, {})
	var won: bool = r.won
	s.chapters[key] = {"won": won or bool(prev.get("won", false)), "tries": int(prev.get("tries", 0)) + 1,
		"best": mini(int(prev.get("best", 1 << 30)), int(r.frame)) if won else int(prev.get("best", 1 << 30))}

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

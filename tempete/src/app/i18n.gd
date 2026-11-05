class_name I18n
## Translations (French and English), with {name} placeholders. Falls back to French.

static var lang := "fr"
static var _d: Dictionary = {}

static func _load(l: String) -> Dictionary:
	if not _d.has(l):
		var f := FileAccess.open("res://assets/i18n/%s.json" % l, FileAccess.READ)
		_d[l] = JSON.parse_string(f.get_as_text()) if f else {}
	return _d[l]

static func set_lang(l: String) -> void:
	lang = l if l == "en" or l == "fr" else "fr"

static func _lookup(d: Dictionary, key: String) -> Variant:
	var cur: Variant = d
	for p in key.split("."):
		if not (cur is Dictionary):
			return null
		cur = (cur as Dictionary).get(p)
		if cur == null:
			return null
	return cur

static func _get_v(key: String) -> Variant:
	var v: Variant = _lookup(_load(lang), key)
	if v == null:
		v = _lookup(_load("fr"), key)
	return v

static func t(key: String, vars: Dictionary = {}) -> String:
	var v: Variant = _get_v(key)
	if not (v is String):
		return key
	var s: String = v
	for k in vars:
		s = s.replace("{%s}" % k, str(vars[k]))
	return s

static func tl(key: String) -> Array:
	var v: Variant = _get_v(key)
	return v if v is Array else []

static func has(key: String) -> bool:
	return _lookup(_load(lang), key) != null

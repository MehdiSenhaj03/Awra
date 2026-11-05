class_name CodeScreen
extends Screen
## Paste a vault code or a replay code from a friend.

var _on_vault: Callable
var _on_replay: Callable
var _on_back: Callable
var _edit: TextEdit
var _info: Label
var _menu: UIMenu
var _parsed: Dictionary = {}

func _init(on_vault: Callable, on_replay: Callable, on_back: Callable) -> void:
	super()
	_on_vault = on_vault
	_on_replay = on_replay
	_on_back = on_back
	Kit.background(self, "bg_title", 4.5, 0.3)
	var p := Kit.panel(self)
	var m := Kit.margin(p, 40, 34, 40, 34)
	var v := Kit.vbox(m, 16)
	Kit.label(v, I18n.t("code.title"), 40, Kit.CREAM, "display", 400, 0.1)
	var h := Kit.label(v, I18n.t("code.help"), 16, Kit.SOFT)
	h.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	h.custom_minimum_size.x = 900
	_edit = TextEdit.new()
	_edit.custom_minimum_size = Vector2(900, 160)
	_edit.wrap_mode = TextEdit.LINE_WRAPPING_BOUNDARY
	_edit.add_theme_font_override("font", Kit.font("mono", 400, 0.0, 14))
	_edit.add_theme_font_size_override("font_size", 14)
	_edit.text_changed.connect(_parse)
	v.add_child(_edit)
	_info = Kit.label(v, "", 18, Kit.GOLD, "body", 600)
	_menu = UIMenu.new(v)
	_menu.add(I18n.t("code.paste"), func() -> void:
		_edit.text = DisplayServer.clipboard_get()
		_parse(), {"size": 17, "width": 900})
	_menu.add(I18n.t("code.try"), func() -> void:
		if _parsed.has("level"):
			_on_vault.call(_parsed.level), {"size": 17, "width": 900})
	_menu.add(I18n.t("code.watch"), func() -> void:
		if _parsed.has("rec"):
			_on_replay.call(_parsed), {"size": 17, "width": 900})
	_menu.add(I18n.t("common.back"), _on_back, {"size": 17, "width": 900})
	p.ready.connect(func() -> void: p.position = ((Vector2(Kit.W, Kit.H) - p.size) / 2).round())
	if DisplayServer.clipboard_get().strip_edges().begins_with("CASSE1:") or DisplayServer.clipboard_get().strip_edges().begins_with("REPLAY1:"):
		_edit.text = DisplayServer.clipboard_get().strip_edges()
		_parse.call_deferred()

func _parse() -> void:
	_parsed = {}
	var t := _edit.text.strip_edges()
	if t.begins_with("REPLAY1:"):
		var r := Run.parse_replay(t)
		if not r.is_empty():
			_parsed = {"level": r.level, "rec": r.rec}
			_info.text = I18n.t("code.vault", {"name": r.level.get("name", "?"), "author": r.level.get("author", "?")}) + "   ·   " + I18n.t("code.replay", {"n": (r.rec as Array).size()})
			return
	elif t.begins_with("CASSE1:"):
		var d := Level.decode(t)
		if not d.is_empty():
			_parsed = {"level": d}
			_info.text = I18n.t("code.vault", {"name": d.get("name", "?"), "author": d.get("author", "?")})
			return
	_info.text = I18n.t("code.invalid") if t != "" else ""

func update(_dt: float) -> void:
	if _edit.has_focus():
		if Keys.pressed("pause"):
			_edit.release_focus()
		return
	if Keys.pressed("back"):
		_on_back.call()
		return
	_menu.handle()

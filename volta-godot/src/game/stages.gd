class_name Stages
extends RefCounted
## Stage data exported from the original game (stages.json): order, sectors, and every stage with its waves.

static var _data: Dictionary = {}

static func data() -> Dictionary:
	if _data.is_empty():
		var f := FileAccess.open("res://assets/stages.json", FileAccess.READ)
		_data = JSON.parse_string(f.get_as_text())
	return _data

static func get_def(id: String) -> Dictionary:
	return data().stages[id]

static func order() -> Array:
	return data().order

static func sectors() -> Array:
	return data().sectors

extends SceneTree
## Loads every script so parse/type errors surface in headless runs.
func _init() -> void:
	var bad := 0
	for dir in ["res://src/", "res://src/sim/", "res://src/ai/", "res://src/render/", "res://src/ui/", "res://src/app/", "res://src/net/", "res://src/audio/"]:
		var d := DirAccess.open(dir)
		for f in d.get_files():
			if f.ends_with(".gd"):
				var s: Script = load(dir + f)
				if s == null or not s.can_instantiate():
					print("BAD ", dir + f)
					bad += 1
	print("compile: ", bad, " bad")
	quit()

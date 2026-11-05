extends SceneTree
## Searches timings: plans JSON may contain the string "AT" (replaced by values from..to step).
## godot --headless -s res://tests/tune.gd -- <id> '<plans with "AT">' from to step
func _init() -> void:
	var a := OS.get_cmdline_user_args()
	var lv := Level.new(JSON.parse_string(FileAccess.get_file_as_string("res://levels/campaign/%s.json" % a[0])))
	var src: String = a[1]
	if src == "-":
		for it: Dictionary in JSON.parse_string(FileAccess.get_file_as_string("res://levels/campaign/index.json")):
			if it.id == a[0]:
				src = JSON.stringify(it.plans)
	var ok: Array = []
	for t in range(int(a[2]), int(a[3]), int(a[4])):
		var plans: Array = JSON.parse_string(src.replace("\"AT\"", str(t)))
		var r := Planner.solve(lv, plans)
		if r.win:
			ok.append(t)
	print("wins at: ", ok)
	quit()

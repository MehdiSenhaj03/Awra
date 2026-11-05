extends SceneTree
## 2D timing search: plans with "AT" and "AT2". -- <id> '<plans>' a0 a1 astep b0 b1 bstep
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
		for u in range(int(a[5]), int(a[6]), int(a[7])):
			var plans: Array = JSON.parse_string(src.replace("\"AT2\"", str(u)).replace("\"AT\"", str(t)))
			var r := Planner.solve(lv, plans)
			if r.win:
				ok.append([t, u])
	print("wins at: ", ok.slice(0, 40), " (", ok.size(), ")")
	quit()

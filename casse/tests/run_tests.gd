extends SceneTree
## Headless checks: determinism, codes, and every campaign vault solved by its written plan.
## godot --headless -s res://tests/run_tests.gd [-- ids]

func _init() -> void:
	var fails := 0
	var index: Array = JSON.parse_string(FileAccess.get_file_as_string("res://levels/campaign/index.json"))
	var only := OS.get_cmdline_user_args()
	for it: Dictionary in index:
		if only.size() > 0 and not only.has(it.id):
			continue
		var lv := Level.new(JSON.parse_string(FileAccess.get_file_as_string("res://levels/campaign/%s.json" % it.id)))
		var t0 := Time.get_ticks_msec()
		var r := Planner.solve(lv, it.plans)
		var ok: bool = r.win
		var again := Run.simulate(Level.new(Level.decode(Level.encode(lv.data))), r.rec)
		if again.win_frame != r.frame:
			ok = false
			print("  non-deterministic replay")
		print("%-5s %-22s %s  echoes=%d frame=%d over=%s by=%d (%d ms)" % [it.id, it.name, "OK " if ok else "FAIL", it.plans.size(), r.frame, r.over, r.alarm_by, Time.get_ticks_msec() - t0])
		if not ok:
			fails += 1
	print("FAILS=", fails)
	quit(1 if fails > 0 else 0)

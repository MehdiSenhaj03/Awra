extends SceneTree
## Prints a run step by step: godot --headless -s res://tests/trace.gd -- <id> '<plans>' [every]
func _init() -> void:
	var a := OS.get_cmdline_user_args()
	var lv := Level.new(JSON.parse_string(FileAccess.get_file_as_string("res://levels/campaign/%s.json" % a[0])))
	var plans: Array = JSON.parse_string(a[1])
	var every := int(a[2]) if a.size() > 2 else 30
	var r := Planner.solve(lv, plans)
	var s := Sim.new(lv)
	s.reset_echoes(plans.size())
	while s.frame < s.loop_frames and s.over == "":
		var inp := PackedByteArray()
		inp.resize(plans.size())
		for k in plans.size():
			inp[k] = (r.rec[k] as PackedByteArray)[s.frame]
		s.step(inp)
		for e in s.events:
			if e.t != "noise":
				print(s.frame, " ", e)
		if s.frame % every == 0:
			var line := "%d " % s.frame
			for i in s.echoes.size():
				line += "e%d(%.1f,%.1f) " % [i, s.echoes[i].x / 256.0, s.echoes[i].y / 256.0]
			for g in s.guards:
				line += "g(%.1f,%.1f f%d m%d s%d) " % [g.x / 256.0, g.y / 256.0, g.face, g.mode, g.sus]
			print(line)
	print("over=", s.over, " frame=", s.frame)
	quit()

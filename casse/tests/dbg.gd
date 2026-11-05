extends SceneTree
func _init() -> void:
	var lv := Level.new(JSON.parse_string(FileAccess.get_file_as_string("res://levels/test1.json")))
	var plans := [
		[["go", 5, 5], ["wait", 2000]],
		[["at", 60], ["go", 7, 4], ["go", 7, 2], ["go", 11, 1], ["hold", 95], ["go", 7, 2], ["go", 7, 4], ["go", 1, 7]],
	]
	var s := Sim.new(lv)
	s.reset_echoes(2)
	var p0 := Planner.new(plans[0], 0)
	var p1 := Planner.new(plans[1], 1)
	while s.frame < s.loop_frames and s.over == "":
		var inp := PackedByteArray([p0.next(s), p1.next(s)])
		s.step(inp)
		for e in s.events:
			print(s.frame, " ", e)
		if s.frame % 60 == 0:
			print(s.frame, " e0 ", s.echoes[0].x / 256.0, ",", s.echoes[0].y / 256.0, " e1 ", s.echoes[1].x / 256.0, ",", s.echoes[1].y / 256.0, " step1=", p1.i, " item=", s.echoes[1].item, " ch1=", s.chan[1])
	print("over ", s.over)
	quit()

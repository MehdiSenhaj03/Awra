extends SceneTree
func _init() -> void:
	var lv := Level.new(JSON.parse_string(FileAccess.get_file_as_string("res://levels/campaign/4-2.json")))
	var idx: Array = JSON.parse_string(FileAccess.get_file_as_string("res://levels/campaign/index.json"))
	var plans: Array = []
	for it: Dictionary in idx:
		if it.id == "4-2":
			plans = it.plans
	var r0 := Planner.solve(lv, [plans[0]])
	var s := Sim.new(lv)
	s.reset_echoes(2)
	var p := Planner.new(plans[1], 1)
	p.recs = r0.rec
	while s.frame < 600:
		var b := p.next(s)
		var inp := PackedByteArray([(r0.rec[0] as PackedByteArray)[s.frame], b])
		if s.frame > 470 and s.frame < 480:
			print(s.frame, " step=", p.i, " b=", b, " safe_mv=", p._safe(s, 1), " safe0=", p._safe(s, 0), " route=", p._route)
		s.step(inp)
	quit()

extends SceneTree
## AI spy against AI sniper over several seeds: balance and speed. -- <venue> <n> <spy skill> <hunter skill>
func _init() -> void:
	var a := OS.get_cmdline_user_args()
	var vid: String = a[0] if a.size() > 0 else "salon"
	var n := int(a[1]) if a.size() > 1 else 6
	var ss := float(a[2]) if a.size() > 2 else 1.0
	var hs := float(a[3]) if a.size() > 3 else 1.0
	var vd: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://venues/%s.json" % vid))
	var v := Venue.new(vd)
	var wins := {"spy": 0, "hunter": 0}
	for k in n:
		var t0 := Time.get_ticks_msec()
		var s := Sim.new(v, "contract", 1000 + k, int(vd.guests))
		var spy := SpyAI.new(s, ss, 7 + k)
		var hunter := HunterAI.new(s, hs, 99 + k)
		var inp := PackedByteArray([0])
		while s.over == "":
			inp[0] = spy.input()
			var h: Array = hunter.update()
			s.step(inp, h[0], h[1])
		var why := ""
		for e in s.events:
			if e.t == "over":
				why = e.why
		wins[s.over] += 1
		print("seed %d: %s (%s) at %.1fs, missions %d, %.2f ms/frame" % [k, s.over, why, s.frame / 60.0, s.done.size(), float(Time.get_ticks_msec() - t0) / s.frame])
	print(wins)
	quit()

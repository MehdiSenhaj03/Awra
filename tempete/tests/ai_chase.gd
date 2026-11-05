extends SceneTree
## A computer team plays a whole chase day: does it find the storms, photograph them, drop probes, survive?
## -- <sev> <tornadoes> <seed>
func _init() -> void:
	var a := OS.get_cmdline_user_args()
	var sev := int(a[0]) if a.size() > 0 else 3
	var nt := int(a[1]) if a.size() > 1 else 2
	var sd := int(a[2]) if a.size() > 2 else 4
	var day := {"seed": sd, "minutes": 9, "storms": [
		{"at": 0, "x": 2_500_000, "y": 3_000_000, "sev": sev, "dir": 560, "speed": 14, "tornadoes": nt},
		{"at": 60 * 90, "x": 1_000_000, "y": 6_500_000, "sev": maxi(1, sev - 1), "dir": 520, "speed": 15, "tornadoes": 1},
	]}
	var t0 := Time.get_ticks_msec()
	var c := Chase.new(day, 2)
	var ai0 := ChaserAI.new(c, 0, 1.0, 5)
	var ai1 := ChaserAI.new(c, 1, 0.8, 9)
	var counts := {}
	var log := ""
	while not c.over:
		c.step([ai0.update(), ai1.update()])
		for e in c.events:
			counts[e.t] = int(counts.get(e.t, 0)) + 1
			if e.t in ["tornado", "probe_hit", "wreck", "storm"]:
				log += "%ds %s %s\n" % [c.frame / 60, e.t, e]
	print(log)
	print("events ", counts)
	for team in 2:
		var v: Chase.Vehicle = c.vehicles[team]
		print("team %d dossier %s  pos (%.1f, %.1f) km  hp %d" % [team, c.dossier(team), v.x / 1e6, v.y / 1e6, v.hp])
	print("%.2f ms per frame" % (float(Time.get_ticks_msec() - t0) / c.frame))
	quit()

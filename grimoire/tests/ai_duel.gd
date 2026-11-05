extends SceneTree
## AI against AI: balance and sanity. -- <n> <skill0> <skill1> <rival1>
func _init() -> void:
	var a := OS.get_cmdline_user_args()
	var n := int(a[0]) if a.size() > 0 else 6
	var s0 := float(a[1]) if a.size() > 1 else 1.0
	var s1 := float(a[2]) if a.size() > 2 else 1.0
	var rv: String = a[3] if a.size() > 3 else ""
	var wins := [0, 0]
	var counts := {}
	for k in n:
		var d := Duel.new()
		var ai0 := DuelAI.new(d, 0, s0, 11 + k * 2)
		var ai1 := DuelAI.new(d, 1, s1, 99 + k * 2, rv)
		var pause := 0
		while d.over < 0 and d.frame < 60 * 600:
			if d.round_over >= 0:
				pause += 1
				if pause > 90:
					pause = 0
					d.next_round()
				d.step([{}, {}])
				continue
			d.step([ai0.update(), ai1.update()])
			for e in d.events:
				counts[e.t] = int(counts.get(e.t, 0)) + 1
		if d.over >= 0:
			wins[d.over] += 1
		print("duel %d: winner %d after %.1fs, rounds %d-%d, casts %d/%d" % [k, d.over, d.frame / 60.0, d.mages[0].rounds, d.mages[1].rounds, d.mages[0].casts, d.mages[1].casts])
	print("wins ", wins, "  events ", counts)
	quit()

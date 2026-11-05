extends SceneTree
## A whole leg played by the AI crew alone: does Ama arrive, how hurt, what happened? -- <kind> <level> <boss 0/1> <seed>
func _init() -> void:
	var a := OS.get_cmdline_user_args()
	var kind: String = a[0] if a.size() > 0 else "ash"
	var lv := int(a[1]) if a.size() > 1 else 1
	var boss := a.size() > 2 and a[2] == "1"
	var sd := int(a[3]) if a.size() > 3 else 3
	var m := March.new({"kind": kind, "length_m": 3000, "seed": sd, "level": lv, "boss": boss, "foe_hp": 7000 + lv * 1500, "foe_power": 80 + lv * 15})
	var counts := {}
	var t0 := Time.get_ticks_msec()
	while m.over == "" and m.frame < 60 * 900:
		m.step([])
		for e in m.events:
			counts[e.t] = int(counts.get(e.t, 0)) + 1
	print("%s lv%d boss=%s: %s at %ds  hp %d  heat %d  pressure %d  supplies %d  foe %s" % [kind, lv, boss, m.over, m.frame / 60, m.titan.hp, m.titan.heat, m.titan.pressure, m.supplies, str(m.foe.hp) if m.foe else "-"])
	print("events ", counts)
	print("%.2f ms/frame" % (float(Time.get_ticks_msec() - t0) / m.frame))
	quit()

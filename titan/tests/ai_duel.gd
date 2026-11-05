extends SceneTree
## The fight of colossi, both titans played by the AI (the crew's pilot and gunners against the wild titan). -- <level> <n>
func _init() -> void:
	var a := OS.get_cmdline_user_args()
	var lv := int(a[0]) if a.size() > 0 else 1
	var n := int(a[1]) if a.size() > 1 else 5
	var wins := 0
	for k in n:
		var m := March.new({"kind": "ash", "seed": 10 + k, "level": lv, "boss": true, "length_m": 1, "foe_power": 80 + lv * 15})
		var counts := {}
		while m.over == "" and m.frame < 60 * 400:
			m.step([])
			for e in m.events:
				counts[e.t] = int(counts.get(e.t, 0)) + 1
		if m.over == "won":
			wins += 1
		print("fight %d: %s at %ds  hp %d  foe %d  %s" % [k, m.over, m.frame / 60, m.titan.hp, m.foe.hp, counts])
	print("wins %d/%d" % [wins, n])
	quit()

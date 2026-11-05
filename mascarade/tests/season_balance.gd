extends SceneTree
## For each night: the AI opponent at its skill against an AI "player" of average skill (1.0). -- <n per night>
func _init() -> void:
	var a := OS.get_cmdline_user_args()
	var n := int(a[0]) if a.size() > 0 else 4
	var only := int(a[1]) if a.size() > 1 else 0
	for nt: Dictionary in Meta.NIGHTS:
		if only > 0 and int(nt.id) != only:
			continue
		var vd: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://venues/%s.json" % nt.venue))
		var v := Venue.new(vd)
		var player_wins := 0
		var info := ""
		for k in n:
			var s := Sim.new(v, "contract", 500 + k * 13 + int(nt.id) * 101, int(vd.guests), 1, nt.missions)
			s.need = mini(int(nt.need), s.missions.size())
			var sp := SpyAI.new(s, nt.skill if nt.role == "hunter" else 1.0, 3 + k)
			var hu := HunterAI.new(s, nt.skill if nt.role == "spy" else 1.0, 9 + k)
			var inp := PackedByteArray([0])
			while s.over == "":
				inp[0] = sp.input()
				var h: Array = hu.update()
				s.step(inp, h[0], h[1])
			var why := ""
			for e in s.events:
				if e.t == "over":
					why = e.why
			if s.over == nt.role:
				player_wins += 1
			info += "%s/%s@%ds " % [s.over, why, s.frame / 60]
		print("night %d (%s, player=%s, ai %.2f): player wins %d/%d  %s" % [nt.id, nt.venue, nt.role, nt.skill, player_wins, n, info])
	quit()

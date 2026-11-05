extends SceneTree
## Party rules: walking a player onto another and stabbing counts a kill; stabbing an innocent exposes.
func _init() -> void:
	var vd: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://venues/salon.json"))
	var s := Sim.new(Venue.new(vd), "party", 77, int(vd.guests), 3)
	var a: Sim.Guest = s.guests[int(s.players[0].guest)]
	var b: Sim.Guest = s.guests[int(s.players[1].guest)]
	var kills := 0
	var inn := 0
	# Teleport A next to B and stab.
	a.x = b.x + 120
	a.y = b.y
	s.step(PackedByteArray([Sim.IN_STAB, 0, 0]))
	for e in s.events:
		if e.t == "kill": kills += 1
	print("kill events: ", kills, " score p0=", s.players[0].score, " p1 respawn=", s.players[1].respawn)
	# Find an NPC and stab it.
	for i in s.guests.size():
		var g := s.guests[i]
		if g.ctrl < 0 and g.st != Sim.St.DEAD:
			a.x = g.x + 100
			a.y = g.y
			break
	s.step(PackedByteArray([Sim.IN_STAB, 0, 0]))
	for e in s.events:
		if e.t == "innocent": inn += 1
	print("innocent events: ", inn, " exposed p0=", s.players[0].exposed, " score p0=", s.players[0].score)
	for k in 200:
		s.step(PackedByteArray([0, 0, 0]))
	print("p1 back: ", s.guests[int(s.players[1].guest)].st != Sim.St.OUT)
	quit()

extends SceneTree
func _init() -> void:
	var vd: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://venues/salon.json"))
	var t0 := Time.get_ticks_usec()
	var v := Venue.new(vd)
	print("venue ", (Time.get_ticks_usec() - t0) / 1000.0, " ms, pois ", v.pois.size())
	t0 = Time.get_ticks_usec()
	var s := Sim.new(v, "contract", 1, 90)
	print("sim init ", (Time.get_ticks_usec() - t0) / 1000.0, " ms")
	var inp := PackedByteArray([0])
	t0 = Time.get_ticks_usec()
	for f in 300:
		s.step(inp)
	print("step ", (Time.get_ticks_usec() - t0) / 300000.0, " ms/frame")
	var spy := SpyAI.new(s, 1.0, 7)
	var hunter := HunterAI.new(s, 1.0, 9)
	t0 = Time.get_ticks_usec()
	for f in 300:
		inp[0] = spy.input()
	print("spy ai ", (Time.get_ticks_usec() - t0) / 300000.0, " ms/frame")
	t0 = Time.get_ticks_usec()
	for f in 300:
		hunter.update()
	print("hunter ai ", (Time.get_ticks_usec() - t0) / 300000.0, " ms/frame")
	quit()

extends SceneTree
## Traces the AI spy alone. -- <venue> <k>
func _init() -> void:
	var a := OS.get_cmdline_user_args()
	var vd: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://venues/%s.json" % a[0]))
	var k := int(a[1])
	var v := Venue.new(vd)
	var s := Sim.new(v, "contract", 1000 + k, int(vd.guests))
	var spy := SpyAI.new(s, 1.0, 7 + k)
	var inp := PackedByteArray([0])
	var last := ""
	while s.frame < 60 * 210 and s.over == "":
		inp[0] = spy.input()
		s.step(inp, Vector2i(-9999, -9999), false)
		var g: Sim.Guest = s.guests[s.spy]
		var st := "%s goal=%d vis=%d lin=%s here=%s done=%s" % [spy._mission, spy._goal, spy._visits, spy._linger > 0, s.mission_here(g), s.done.keys()]
		if st != last or s.frame % 600 == 0:
			var c: Sim.Guest = s.guests[s.contact]
			print("%.1f %s pos=%d,%d st=%d in=%d  contact=%d,%d (%d,%d) st=%d poi=%d" % [s.frame / 60.0, st, g.x >> 8, g.y >> 8, g.st, inp[0], c.x >> 8, c.y >> 8, c.x, c.y, c.st, c.poi])
			last = st
	quit()

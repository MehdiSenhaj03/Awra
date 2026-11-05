extends SceneTree
## Recogniser accuracy on noisy synthetic drawings of every template (random affine, jitter, stroke order and
## direction), plus random scribbles that should be rejected. -- <samples per glyph>
func _init() -> void:
	var a := OS.get_cmdline_user_args()
	var n := int(a[0]) if a.size() > 0 else 40
	var rng := RandomNumberGenerator.new()
	rng.seed = 7
	var ok := 0
	var total := 0
	var conf := {}
	var scores := []
	var t0 := Time.get_ticks_msec()
	for g in Glyphs.COUNT:
		var variants: Array = Glyphs.shapes(g)
		for k in n:
			var v: Array = variants[k % variants.size()]
			var strokes := _distort(v, rng)
			var r := Glyphs.recognize(strokes)
			total += 1
			scores.append(r.d)
			if r.g != g and k < 3:
				print("  miss ", Glyphs.NAMES[g], " corners=", Glyphs.corners(strokes), " got ", r)
			if r.g == g:
				ok += 1
			else:
				var key := "%s->%s" % [Glyphs.NAMES[g], Glyphs.NAMES[r.g] if r.g >= 0 else "none"]
				conf[key] = int(conf.get(key, 0)) + 1
	scores.sort()
	print("glyph d p50=%.2f p95=%.2f p99=%.2f" % [scores[scores.size() / 2], scores[int(scores.size() * 0.95)], scores[int(scores.size() * 0.99)]])
	var cs := ""
	for g in Glyphs.COUNT:
		cs += "%s=%d " % [Glyphs.NAMES[g], Glyphs.corners(Glyphs.shapes(g)[0])]
	print("template corners: ", cs)
	print("accuracy %d / %d = %.1f%%  (%.1f ms per glyph)" % [ok, total, 100.0 * ok / total, float(Time.get_ticks_msec() - t0) / total])
	print("confusions: ", conf)
	# Random scribbles.
	var rej := 0
	var sc2 := []
	for k in 100:
		var st: Array = []
		var p := Vector2(rng.randf(), rng.randf())
		for i in rng.randi_range(4, 9):
			st.append(p)
			p = Vector2(rng.randf(), rng.randf())
		var r := Glyphs.recognize([st])
		sc2.append(r.d)
		if r.g < 0:
			rej += 1
		elif k < 8:
			print("  scribble -> ", r)
	sc2.sort()
	print("scribble d p5=%.2f p50=%.2f" % [sc2[5], sc2[50]])
	print("scribbles rejected: %d / 100" % rej)
	quit()

func _distort(v: Array, rng: RandomNumberGenerator) -> Array:
	var rot := rng.randf_range(-0.22, 0.22)
	var sx := rng.randf_range(0.8, 1.2)
	var sy := rng.randf_range(0.8, 1.2)
	var sh := rng.randf_range(-0.12, 0.12)
	var size := rng.randf_range(80.0, 400.0)
	var out: Array = []
	for st: Array in v:
		var s2: Array = []
		# Densify like a mouse would (points every few pixels) with jitter.
		for i in range(st.size() - 1):
			var p0: Vector2 = st[i]
			var p1: Vector2 = st[i + 1]
			var steps := maxi(2, int(p0.distance_to(p1) * size / 6.0))
			for j in steps:
				var p := p0.lerp(p1, float(j) / steps)
				p = Vector2(p.x * sx + p.y * sh, p.y * sy).rotated(rot) * size
				p += Vector2(rng.randf_range(-1, 1), rng.randf_range(-1, 1)) * size * 0.025
				s2.append(p)
		var last: Vector2 = st[st.size() - 1]
		s2.append(Vector2(last.x * sx + last.y * sh, last.y * sy).rotated(rot) * size)
		if rng.randf() < 0.5:
			s2.reverse()
		out.append(s2)
	out.shuffle()
	return out

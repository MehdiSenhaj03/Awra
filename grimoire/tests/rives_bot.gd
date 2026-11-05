extends SceneTree
## A simple bot plays the Two Banks alone (both banks), drawing one glyph every `delay` frames.
## -- <level> <boss 0/1> <delay> <n>
func _init() -> void:
	var a := OS.get_cmdline_user_args()
	var level := int(a[0]) if a.size() > 0 else 0
	var boss := a.size() > 1 and a[1] == "1"
	var delay := int(a[2]) if a.size() > 2 else 60
	var n := int(a[3]) if a.size() > 3 else 4
	var wins := 0
	for k in n:
		var r := Rives.new(100 + k * 2, level, boss, 1)
		var cd := 0
		while r.over == "" and r.frame < 60 * 600:
			var c := {}
			cd -= 1
			if cd <= 0:
				c = _choose(r)
				if not c.is_empty():
					cd = delay
			r.step([c])
		if r.over == "won":
			wins += 1
		print("run %d: %s at %.0fs, hp %d/%d, walked %d/%d/%d, beast %d" % [k, r.over, r.frame / 60.0, r.banks[0].hp, r.banks[1].hp, r.banks[0].walked, r.banks[1].walked, r.length, r.beast_hp])
	print("wins %d / %d" % [wins, n])
	quit()

func _choose(r: Rives) -> Dictionary:
	for bi in 2:
		var b: Rives.Bank = r.banks[bi]
		if b.hp < 4000 and b.heal <= 0:
			return {"g": Glyphs.HEAL, "bank": bi, "x": 800, "pow": 100}
	for s: Dictionary in r.strikes:
		if r.banks[int(s.bank)].shield <= 0:
			return {"g": Glyphs.SHIELD, "bank": s.bank, "x": 900, "pow": 100}
	# The nearest threat over both banks.
	var best: Rives.Foe = null
	for f in r.foes:
		if not f.dead and (best == null or f.x < best.x):
			best = f
	if best and best.x < 6000:
		var g := Glyphs.FIRE
		if Rives.ESPEC[best.kind][3]:
			g = Glyphs.WIND if best.kind == Rives.E.CROW else Glyphs.BOLT
		elif best.kind == Rives.E.GOLEM:
			g = Glyphs.EARTH
		elif best.kind == Rives.E.SNAKE:
			g = Glyphs.BOLT
		return {"g": g, "bank": best.bank, "x": best.x, "pow": 100}
	for o in r.obstacles:
		var ox := r.obstacle_x(o)
		if not o.cleared and ox >= 0 and ox < 5000:
			return {"g": Rives.CLEARS[o.kind], "bank": o.bank, "x": ox, "pow": 100}
	if r.boss and r.beast_t > 300:
		return {"g": Glyphs.BOLT, "bank": r.beast_eye, "x": 8000, "pow": 100}
	return {}

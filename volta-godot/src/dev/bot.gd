class_name Bot
extends RefCounted
## A simple player (same as the original test bot): aims at the nearest foe, keeps its distance, sidesteps
## close bullets, dashes through danger, pulses on the beat when bullets are about to land.

static func controls(w: World, rhythm: bool = false) -> Controls:
	var c := Controls.new()
	var p := w.player
	var tg := w.nearest_enemy(p.x, p.y, 80.0)
	var ax := 1.0
	var ay := 0.0
	var mx := 0.0
	var my := 0.0
	if tg:
		ax = tg.x - p.x
		ay = tg.y - p.y
		var d := U.hypot(ax, ay)
		if d == 0.0:
			d = 1.0
		ax /= d
		ay /= d
		var want := 8.0
		mx = (d - want) * 0.25 * ax - ay * 0.6
		my = (d - want) * 0.25 * ay + ax * 0.6
	else:
		mx = -p.x * 0.1
		my = -p.y * 0.1
	var danger := 0
	var b := w.bullets
	for i in b.n:
		if b.flags[i] == 2:
			continue
		var dx := p.x - b.x[i]
		var dy := p.y - b.y[i]
		var d := U.hypot(dx, dy)
		if d < 3.5:
			var kk := (3.5 - d) / 3.5
			var dd := d if d != 0.0 else 1.0
			mx += (dx / dd) * kk * 3.0
			my += (dy / dd) * kk * 3.0
			if d < 1.4:
				danger += 1
	for h in w.hazards:
		if h.kind == "zone" and U.hypot(p.x - h.x, p.y - h.y) < h.r + 1.0:
			mx += (p.x - h.x) * 2.0
			my += (p.y - h.y) * 2.0
	mx -= p.x * 0.02
	my -= p.y * 0.02
	var l := U.hypot(mx, my)
	if l > 1.0:
		mx /= l
		my /= l
	var on_beat := w.beat.judge() == "perfect"
	c.mx = mx
	c.my = my
	c.ax = ax
	c.ay = ay
	c.fire = (on_beat or w.frame % 3 != 0) if rhythm else true
	c.fire_pressed = rhythm and on_beat and absf(w.beat.offset_sec()) < 0.017
	c.dash = danger >= 2 and p.dash_charges > 0
	c.pulse = danger >= 1 and p.pulse_cd == 0 and on_beat
	c.overdrive = w.gauge >= 100.0
	return c

static func play(id: String, god: bool = true, max_frames: int = 60 * 60 * 8) -> Dictionary:
	var d := Stages.get_def(id)
	var w := World.new(d.arena, d.bpm, 3)
	w.god = god
	w.player.y = 4.0
	var run := StageRunner.new(w, d)
	var max_phase := 0
	var f := 0
	var t0 := Time.get_ticks_usec()
	while f < max_frames and w.over == "":
		w.update(controls(w))
		run.update()
		w.events.clear()
		for e in w.enemies:
			if e.boss:
				max_phase = maxi(max_phase, e.phase)
		f += 1
	var us := Time.get_ticks_usec() - t0
	w.dispose()
	return {"over": w.over, "s": int(f / 60.0), "score": int(w.score), "phase": max_phase, "kills": w.stats.kills, "hits": w.stats.hits, "wave": run.wave_number, "ms_per_frame": snappedf(us / 1000.0 / maxf(1.0, f), 0.001)}

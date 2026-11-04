class_name Lancer
extends Enemy
## Lance: aims a warning line for one beat, then charges straight through it.

var tx := 0.0
var ty := 0.0
var warn: Hazard = null

func _init(px_: float, py_: float) -> void:
	super(px_, py_)
	kind = "lancer"; look = "lancer"
	r = 0.65; hp = 8.0; max_hp = 8.0; value = 150.0; notes = 2
	state = "roam"

func update(w: World, _k: float) -> void:
	var spb := w.beat.spb * 60.0
	if state == "roam":
		steer(w.player.x, w.player.y, 3.0, 0.03)
		rot = angle_to(w)
		if st > spb * 2.0 and w.beat.crossed(1):
			var a := angle_to(w)
			tx = x + cos(a) * 22.0
			ty = y + sin(a) * 22.0
			rot = a
			warn = Hazard.new("line", x, y, tx, ty, 0.5, spb, P.RED)
			w.add_hazard(warn)
			to("aim")
			w.sfx("telegraph", 1.2, 0.5)
	elif state == "aim":
		vx *= 0.8
		vy *= 0.8
		if warn:
			warn.x = x
			warn.y = y
		if st >= spb:
			to("charge")
			w.sfx("lunge", 1.0, 0.6)
	elif state == "charge":
		var a := rot
		vx = cos(a) * 34.0
		vy = sin(a) * 34.0
		if st > 26.0 or not w.arena.inside(x + cos(a) * 1.2, y + sin(a) * 1.2, r):
			to("recover")
			if elite:
				P.fan(w, x, y, a + PI, 7, 1.6, 8.0, {"color": P.RED})
	elif state == "recover":
		vx *= 0.85
		vy *= 0.85
		if st > spb:
			to("roam")

func release() -> void:
	warn = null

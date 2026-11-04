class_name Decoy
extends Enemy
## A hollow copy of Staccato: shoots with it, breaks in a few hits, gives nothing to the real one.

var master: Boss

func _init(px_: float, py_: float, m: Boss) -> void:
	super(px_, py_)
	master = m
	kind = "decoy"; look = "staccato"
	r = 1.6; hp = 30.0; max_hp = 30.0
	color = P.MAGENTA
	value = 300.0
	notes = 4
	heavy = true

func update(w: World, _k: float) -> void:
	vx *= 0.8
	vy *= 0.8
	if master == null or master.dead or master.dying > 0:
		dead = true
	if w.beat.crossed(4) and w.beat.index(4) % 16 in [0, 1, 2]:
		w.bullets.add(x, y, angle_to(w), 15.0, {"color": P.MAGENTA, "kind": EnemyBullets.SHARD, "r": 0.28})

func release() -> void:
	master = null

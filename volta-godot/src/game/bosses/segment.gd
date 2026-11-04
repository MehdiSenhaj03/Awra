class_name Segment
extends Enemy
## One ring of Rubato's body. Hits are passed to the head at a discount.

var head: Rubato
var index: int

func _init(h: Rubato, i: int) -> void:
	super(h.x, h.y)
	head = h
	index = i
	kind = "segment"; look = "segment"
	r = 1.15 - i * 0.04
	hp = 1e9; max_hp = 1e9
	color = P.VIOLET
	heavy = true
	spawn_t = 0
	value = 0.0
	notes = 0
	contact = true

func update(_w: World, _k: float) -> void:
	if head == null:
		dead = true
		return
	var tr := head.trail
	if not tr.is_empty():
		var q: Vector2 = tr[mini(tr.size() - 1, (index + 1) * 7)]
		vx = (q.x - x) * 60.0
		vy = (q.y - y) * 60.0
		x = q.x - vx / 60.0
		y = q.y - vy / 60.0
	vis = head.vis
	hurtable = head.vis > 0.5 and head.hurtable
	targetable = hurtable
	contact = head.vis > 0.5
	dead = head.dead

func hit(w: World, d: float, fx: float, fy: float, _k: int = -1) -> bool:
	if not hurtable or head == null:
		return false
	flash = 1.0
	w.emit({"t": "hit", "x": x, "y": y, "color": color, "big": false})
	return head.hit(w, d * 0.45, fx, fy)

func release() -> void:
	head = null

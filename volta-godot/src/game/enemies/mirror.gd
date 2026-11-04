class_name Mirror
extends Enemy
## Miroir: reflects shots, except on the beat when its face opens.

var last_beat := 0.0

func _init(px_: float, py_: float) -> void:
	super(px_, py_)
	kind = "mirror"; look = "mirror"
	r = 0.85; hp = 10.0; max_hp = 10.0; value = 260.0; notes = 3
	color = P.WHITE

func is_open() -> bool:
	var f := last_beat - floorf(last_beat)
	return f < 0.2 or f > 0.9 or stun > 0.0

func update(w: World, _k: float) -> void:
	last_beat = w.beat.beat
	var a := angle_to(w)
	steer(w.player.x - cos(a) * 9.0, w.player.y - sin(a) * 9.0, 3.5, 0.04)
	rot = a
	spin = 1.0 if is_open() else 0.0

func blocks(w: World, _fx: float, _fy: float, _k: int) -> bool:
	last_beat = w.beat.beat
	if is_open():
		return false
	var a := atan2(w.player.y - y, w.player.x - x)
	w.bullets.add(x + cos(a) * r, y + sin(a) * r, a + (randf() - 0.5) * 0.2, 12.0, {"color": P.WHITE, "kind": EnemyBullets.SHARD, "r": 0.26})
	return true

class_name Screen
extends Control
## A full screen or an overlay. update() is called every displayed frame while it is on top.

var app: Node

func _init() -> void:
	Kit.full(self)
	mouse_filter = Control.MOUSE_FILTER_STOP
	modulate.a = 0.0
	ready.connect(func() -> void:
		var tw := create_tween()
		tw.tween_property(self, "modulate:a", 1.0, 0.5).set_ease(Tween.EASE_OUT))

func update(_dt: float) -> void:
	pass

## Fades out and frees itself.
func leave() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	pivot_offset = size / 2
	var tw := create_tween().set_parallel()
	tw.tween_property(self, "modulate:a", 0.0, 0.35).set_ease(Tween.EASE_IN)
	tw.tween_property(self, "scale", Vector2(1.03, 1.03), 0.35)
	tw.chain().tween_callback(queue_free)

func dim(a: float) -> ColorRect:
	return Kit.full(Kit.rect(self, Color(0, 0, 0, a))) as ColorRect

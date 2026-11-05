class_name GlassPanel
extends PanelContainer
## The dark glass panel, with its short gold mark on the top-left corner.

func _draw() -> void:
	draw_rect(Rect2(-1, -1, 60, 2), Color(0.95, 0.72, 0.36))
	for k in range(1, 4):
		draw_rect(Rect2(-1, -1 - k, 60, 2 + k * 2), Color(1.0, 0.7, 0.3, 0.08))

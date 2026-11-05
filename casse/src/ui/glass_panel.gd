class_name GlassPanel
extends PanelContainer
## The dark glass panel, with its short cyan mark on the top-left corner.

func _draw() -> void:
	draw_rect(Rect2(-1, -1, 60, 2), Color(0.25, 1.0, 1.0))
	for k in range(1, 4):
		draw_rect(Rect2(-1, -1 - k, 60, 2 + k * 2), Color(0.18, 0.95, 1.0, 0.08))

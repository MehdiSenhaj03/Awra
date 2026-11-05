class_name UIMenu
extends RefCounted
## A focusable list driven by keyboard, gamepad and mouse (vertical by default), as in the web version.

var items: Array[MenuItem] = []
var index := 0
var horizontal := false
var root: BoxContainer

func _init(parent: Node, horiz: bool = false, gap: int = 4) -> void:
	horizontal = horiz
	root = HBoxContainer.new() if horiz else VBoxContainer.new()
	root.add_theme_constant_override("separation", gap)
	root.mouse_filter = Control.MOUSE_FILTER_IGNORE
	parent.add_child(root)

func add(text: String, action: Callable = Callable(), opts: Dictionary = {}) -> MenuItem:
	var it := MenuItem.new(text, opts.get("sub", ""), opts.get("value", ""), opts.get("size", 18), opts.get("width", 0))
	it.action = action
	it.adjust = opts.get("adjust", Callable())
	it.disabled = opts.get("disabled", false)
	var i := items.size()
	it.mouse_entered.connect(func() -> void: focus(i))
	it.clicked.connect(func() -> void:
		focus(i)
		activate())
	items.append(it)
	root.add_child(it)
	if items.size() == 1:
		focus(0, true)
	return it

func focus(i: int, quiet: bool = false) -> void:
	if i == index and not quiet and items[i].on:
		return
	if index < items.size():
		items[index].set_on(false)
	index = clampi(i, 0, items.size() - 1)
	if items.size() > 0:
		items[index].set_on(true)
	if not quiet:
		Sound.sfx("ui", 1.0 + index * 0.04)

func activate() -> void:
	if index >= items.size():
		return
	var it := items[index]
	if it.disabled:
		return
	Sound.sfx("uiOk")
	if it.action.is_valid():
		it.action.call()

## Reads this frame's menu actions. Returns true when something happened.
func handle() -> bool:
	var prev := "left" if horizontal else "up"
	var nxt := "right" if horizontal else "down"
	if Keys.pressed(prev):
		_step(-1)
		return true
	if Keys.pressed(nxt):
		_step(1)
		return true
	if index < items.size():
		var it := items[index]
		if it.adjust.is_valid() and not horizontal:
			if Keys.pressed("left"):
				it.adjust.call(-1)
				Sound.sfx("ui", 0.9)
				return true
			if Keys.pressed("right"):
				it.adjust.call(1)
				Sound.sfx("ui", 1.1)
				return true
	if Keys.pressed("confirm"):
		activate()
		return true
	return false

func _step(d: int) -> void:
	var n := items.size()
	var i := index
	for k in n:
		i = (i + d + n) % n
		if not items[i].disabled:
			break
	focus(i)

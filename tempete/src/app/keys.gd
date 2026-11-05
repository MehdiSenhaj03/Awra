class_name Keys
## Actions and their bindings (physical keys, so AZERTY works like QWERTY), mouse buttons and gamepads.
## Bindings are stored as "k<physical keycode>" or "m<mouse button>". Driving reads analog strength (pads).

const ACTIONS := ["up", "down", "left", "right", "act", "throttle", "brake", "steer_left", "steer_right", "horn", "probe", "camera", "shoot", "radar", "look", "pause", "confirm", "back"]
const BINDABLE := ["throttle", "brake", "steer_left", "steer_right", "probe", "camera", "shoot", "radar", "look", "horn", "pause"]

static func defaults() -> Dictionary:
	return {
		"up": ["k%d" % KEY_W, "k%d" % KEY_UP],
		"down": ["k%d" % KEY_S, "k%d" % KEY_DOWN],
		"left": ["k%d" % KEY_A, "k%d" % KEY_LEFT],
		"right": ["k%d" % KEY_D, "k%d" % KEY_RIGHT],
		"act": ["k%d" % KEY_E, "k%d" % KEY_SPACE],
		"throttle": ["k%d" % KEY_W, "k%d" % KEY_UP],
		"brake": ["k%d" % KEY_S, "k%d" % KEY_DOWN],
		"steer_left": ["k%d" % KEY_A, "k%d" % KEY_LEFT],
		"steer_right": ["k%d" % KEY_D, "k%d" % KEY_RIGHT],
		"horn": ["k%d" % KEY_H, ""],
		"probe": ["k%d" % KEY_P, "k%d" % KEY_SPACE],
		"camera": ["m%d" % MOUSE_BUTTON_RIGHT, "k%d" % KEY_C],
		"shoot": ["m%d" % MOUSE_BUTTON_LEFT, "k%d" % KEY_ENTER],
		"radar": ["k%d" % KEY_R, "k%d" % KEY_TAB],
		"look": ["k%d" % KEY_L, "m%d" % MOUSE_BUTTON_MIDDLE],
		"pause": ["k%d" % KEY_ESCAPE, "k%d" % KEY_BACKSPACE],
		"confirm": ["k%d" % KEY_ENTER, "k%d" % KEY_SPACE],
		"back": ["k%d" % KEY_ESCAPE, "k%d" % KEY_BACKSPACE],
	}

const PAD := {
	"act": [JOY_BUTTON_A],
	"probe": [JOY_BUTTON_Y],
	"camera": [JOY_BUTTON_LEFT_SHOULDER],
	"shoot": [JOY_BUTTON_RIGHT_SHOULDER],
	"radar": [JOY_BUTTON_BACK],
	"look": [JOY_BUTTON_RIGHT_STICK],
	"horn": [JOY_BUTTON_LEFT_STICK],
	"pause": [JOY_BUTTON_START],
	"confirm": [JOY_BUTTON_A],
	"back": [JOY_BUTTON_B],
	"up": [JOY_BUTTON_DPAD_UP],
	"down": [JOY_BUTTON_DPAD_DOWN],
	"left": [JOY_BUTTON_DPAD_LEFT],
	"right": [JOY_BUTTON_DPAD_RIGHT],
}

static var bindings: Dictionary = defaults()

static func _event(code: String) -> InputEvent:
	if code == "":
		return null
	var n := int(code.substr(1))
	if code.begins_with("m"):
		var m := InputEventMouseButton.new()
		m.button_index = n as MouseButton
		return m
	var k := InputEventKey.new()
	k.physical_keycode = n as Key
	return k

## Rebuilds the engine's input map from the bindings.
static func apply(custom: Variant = null) -> void:
	bindings = defaults()
	if custom is Dictionary:
		for a in custom:
			if bindings.has(a) and custom[a] is Array:
				bindings[a] = custom[a]
	for a: String in ACTIONS:
		if InputMap.has_action(a):
			InputMap.erase_action(a)
		InputMap.add_action(a, 0.3)
		for c: String in bindings[a]:
			var ev := _event(c)
			if ev:
				InputMap.action_add_event(a, ev)
		for b: int in PAD.get(a, []):
			var j := InputEventJoypadButton.new()
			j.button_index = b as JoyButton
			InputMap.action_add_event(a, j)
	var axes := [["left", JOY_AXIS_LEFT_X, -1.0], ["right", JOY_AXIS_LEFT_X, 1.0], ["up", JOY_AXIS_LEFT_Y, -1.0], ["down", JOY_AXIS_LEFT_Y, 1.0],
		["steer_left", JOY_AXIS_LEFT_X, -1.0], ["steer_right", JOY_AXIS_LEFT_X, 1.0], ["throttle", JOY_AXIS_TRIGGER_RIGHT, 1.0], ["brake", JOY_AXIS_TRIGGER_LEFT, 1.0]]
	for ax in axes:
		var m := InputEventJoypadMotion.new()
		m.axis = ax[1]
		m.axis_value = ax[2]
		InputMap.action_add_event(ax[0], m)

static func code_name(code: String) -> String:
	if code == "":
		return "—"
	var n := int(code.substr(1))
	if code.begins_with("m"):
		match n:
			MOUSE_BUTTON_LEFT: return "Clic G"
			MOUSE_BUTTON_RIGHT: return "Clic D"
			MOUSE_BUTTON_MIDDLE: return "Clic M"
		return "Souris %d" % n
	match n:
		KEY_SPACE: return "Espace"
		KEY_SHIFT: return "Maj"
		KEY_CTRL: return "Ctrl"
		KEY_ESCAPE: return "Échap"
		KEY_ENTER: return "Entrée"
		KEY_BACKSPACE: return "Retour"
		KEY_TAB: return "Tab"
		KEY_UP: return "↑"
		KEY_DOWN: return "↓"
		KEY_LEFT: return "←"
		KEY_RIGHT: return "→"
	var label := DisplayServer.keyboard_get_label_from_physical(n as Key) if DisplayServer.get_name() != "headless" else (n as Key)
	if label == KEY_NONE:
		label = n as Key
	return OS.get_keycode_string(label).to_upper()

static func key_name(a: String) -> String:
	var list: Array = bindings.get(a, [])
	return code_name(list[0]) if list.size() > 0 else "?"

static func code_of(e: InputEvent) -> String:
	if e is InputEventKey:
		var k := e as InputEventKey
		return "k%d" % (k.physical_keycode if k.physical_keycode != KEY_NONE else k.keycode)
	if e is InputEventMouseButton:
		return "m%d" % (e as InputEventMouseButton).button_index
	return ""

static func pressed(a: String) -> bool:
	return Input.is_action_just_pressed(a)

static func down(a: String) -> bool:
	return Input.is_action_pressed(a)

static func move() -> Vector2:
	var v := Vector2(Input.get_action_strength("right") - Input.get_action_strength("left"), Input.get_action_strength("down") - Input.get_action_strength("up"))
	return v.limit_length(1.0)

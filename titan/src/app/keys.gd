class_name Keys
## Actions and their bindings (physical keys, so AZERTY works like QWERTY), mouse buttons and gamepads.
## Two players can share the keyboard: player 1 on the left (WASD, Space, Shift, F, 1-5, the mouse), player 2 on
## the right (arrows, Enter, right Shift, right Ctrl, 6-0). Gamepads are read directly by the game (a pad each).
## Bindings are stored as "k<physical keycode>" or "m<mouse button>".

const ACTIONS := ["up", "down", "left", "right", "confirm", "back", "pause",
	"mv_up", "mv_down", "mv_left", "mv_right", "act", "alt", "grab", "post1", "post2", "post3", "post4", "post5", "view",
	"p2_up", "p2_down", "p2_left", "p2_right", "p2_act", "p2_alt", "p2_grab", "p2_post1", "p2_post2", "p2_post3", "p2_post4", "p2_post5"]
const BINDABLE := ["act", "alt", "grab", "post1", "post2", "post3", "post4", "post5", "view", "p2_act", "p2_alt", "p2_grab", "pause"]

static func defaults() -> Dictionary:
	return {
		"up": ["k%d" % KEY_W, "k%d" % KEY_UP],
		"down": ["k%d" % KEY_S, "k%d" % KEY_DOWN],
		"left": ["k%d" % KEY_A, "k%d" % KEY_LEFT],
		"right": ["k%d" % KEY_D, "k%d" % KEY_RIGHT],
		"confirm": ["k%d" % KEY_ENTER, "k%d" % KEY_SPACE],
		"back": ["k%d" % KEY_ESCAPE, "k%d" % KEY_BACKSPACE],
		"pause": ["k%d" % KEY_ESCAPE, "k%d" % KEY_P],
		"mv_up": ["k%d" % KEY_W, ""],
		"mv_down": ["k%d" % KEY_S, ""],
		"mv_left": ["k%d" % KEY_A, ""],
		"mv_right": ["k%d" % KEY_D, ""],
		"act": ["k%d" % KEY_SPACE, "m%d" % MOUSE_BUTTON_LEFT],
		"alt": ["k%d" % KEY_SHIFT, "m%d" % MOUSE_BUTTON_RIGHT],
		"grab": ["k%d" % KEY_F, "k%d" % KEY_E],
		"post1": ["k%d" % KEY_1, ""],
		"post2": ["k%d" % KEY_2, ""],
		"post3": ["k%d" % KEY_3, ""],
		"post4": ["k%d" % KEY_4, ""],
		"post5": ["k%d" % KEY_5, ""],
		"view": ["k%d" % KEY_TAB, "k%d" % KEY_V],
		"p2_up": ["k%d" % KEY_UP, ""],
		"p2_down": ["k%d" % KEY_DOWN, ""],
		"p2_left": ["k%d" % KEY_LEFT, ""],
		"p2_right": ["k%d" % KEY_RIGHT, ""],
		"p2_act": ["k%d" % KEY_ENTER, "k%d" % KEY_KP_0],
		"p2_alt": ["k%d" % KEY_SLASH, "k%d" % KEY_KP_PERIOD],
		"p2_grab": ["k%d" % KEY_PERIOD, "k%d" % KEY_KP_ENTER],
		"p2_post1": ["k%d" % KEY_6, "k%d" % KEY_KP_1],
		"p2_post2": ["k%d" % KEY_7, "k%d" % KEY_KP_2],
		"p2_post3": ["k%d" % KEY_8, "k%d" % KEY_KP_3],
		"p2_post4": ["k%d" % KEY_9, "k%d" % KEY_KP_4],
		"p2_post5": ["k%d" % KEY_0, "k%d" % KEY_KP_5],
	}

## Gamepad buttons for the menus only (in game, the pads are read per device).
const PAD := {
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
	for ax in [["left", JOY_AXIS_LEFT_X, -1.0], ["right", JOY_AXIS_LEFT_X, 1.0], ["up", JOY_AXIS_LEFT_Y, -1.0], ["down", JOY_AXIS_LEFT_Y, 1.0]]:
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
			MOUSE_BUTTON_LEFT: return I18n.t("key.lmb")
			MOUSE_BUTTON_RIGHT: return I18n.t("key.rmb")
			MOUSE_BUTTON_MIDDLE: return I18n.t("key.mmb")
		return "M%d" % n
	match n:
		KEY_SPACE: return I18n.t("key.space")
		KEY_SHIFT: return I18n.t("key.shift")
		KEY_CTRL: return "Ctrl"
		KEY_ESCAPE: return I18n.t("key.esc")
		KEY_ENTER: return I18n.t("key.enter")
		KEY_KP_ENTER: return I18n.t("key.kp_enter")
		KEY_BACKSPACE: return "⌫"
		KEY_TAB: return "Tab"
		KEY_UP: return "↑"
		KEY_DOWN: return "↓"
		KEY_LEFT: return "←"
		KEY_RIGHT: return "→"
	if n >= KEY_KP_0 and n <= KEY_KP_9:
		return "Num %d" % (n - KEY_KP_0)
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

class_name Keys
## Actions and their bindings (physical keys, so AZERTY works like QWERTY), mouse buttons and gamepads.
## Bindings are stored as "k<physical keycode>" or "m<mouse button>". Also reads raw devices for the party mode.

const ACTIONS := ["up", "down", "left", "right", "act", "hurry", "fire", "scope", "mark", "pause", "confirm", "back"]
const BINDABLE := ["up", "down", "left", "right", "act", "hurry", "fire", "scope", "mark", "pause"]

static func defaults() -> Dictionary:
	return {
		"up": ["k%d" % KEY_W, "k%d" % KEY_UP],
		"down": ["k%d" % KEY_S, "k%d" % KEY_DOWN],
		"left": ["k%d" % KEY_A, "k%d" % KEY_LEFT],
		"right": ["k%d" % KEY_D, "k%d" % KEY_RIGHT],
		"act": ["k%d" % KEY_E, "k%d" % KEY_SPACE],
		"hurry": ["k%d" % KEY_SHIFT, ""],
		"fire": ["m%d" % MOUSE_BUTTON_LEFT, "k%d" % KEY_F],
		"scope": ["m%d" % MOUSE_BUTTON_RIGHT, "k%d" % KEY_Q],
		"mark": ["m%d" % MOUSE_BUTTON_MIDDLE, "k%d" % KEY_R],
		"pause": ["k%d" % KEY_ESCAPE, "k%d" % KEY_P],
		"confirm": ["k%d" % KEY_ENTER, "k%d" % KEY_SPACE],
		"back": ["k%d" % KEY_ESCAPE, "k%d" % KEY_BACKSPACE],
	}

const PAD := {
	"act": [JOY_BUTTON_A],
	"hurry": [JOY_BUTTON_RIGHT_SHOULDER],
	"fire": [JOY_BUTTON_A],
	"scope": [JOY_BUTTON_LEFT_SHOULDER],
	"mark": [JOY_BUTTON_Y],
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
	var axes := [["left", JOY_AXIS_LEFT_X, -1.0], ["right", JOY_AXIS_LEFT_X, 1.0], ["up", JOY_AXIS_LEFT_Y, -1.0], ["down", JOY_AXIS_LEFT_Y, 1.0], ["hurry", JOY_AXIS_TRIGGER_RIGHT, 1.0], ["scope", JOY_AXIS_TRIGGER_LEFT, 1.0]]
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

## Input byte for the simulation from a move vector and buttons.
static func byte(mv: Vector2, act: bool, hurry: bool, stab: bool = false) -> int:
	var d := 0
	if mv.length() > 0.35:
		var a := fposmod(atan2(mv.y, mv.x), TAU)
		var sector := int(roundf(a / (TAU / 8.0))) % 8
		d = sector + 1
	return d | (Sim.IN_HURRY if hurry else 0) | (Sim.IN_ACT if act else 0) | (Sim.IN_STAB if stab else 0)

## Solo: the bound actions (keyboard and every pad).
static func solo_byte() -> int:
	return byte(move(), down("act"), down("hurry"))

## Raw device reading for the party mode. src: "kb" (WASD side), "kb2" (arrows side), or "pad<N>".
## Each device: move, act (blend in, hold), hurry, stab (press).
static func device_byte(src: String, prev: Dictionary) -> int:
	var mv := Vector2.ZERO
	var act := false
	var hurry := false
	var stab := false
	if src == "kb":
		mv = Vector2(float(Input.is_physical_key_pressed(KEY_D)) - float(Input.is_physical_key_pressed(KEY_A)), float(Input.is_physical_key_pressed(KEY_S)) - float(Input.is_physical_key_pressed(KEY_W)))
		act = Input.is_physical_key_pressed(KEY_E)
		hurry = Input.is_physical_key_pressed(KEY_SHIFT)
		stab = Input.is_physical_key_pressed(KEY_Q)
	elif src == "kb2":
		mv = Vector2(float(Input.is_physical_key_pressed(KEY_RIGHT)) - float(Input.is_physical_key_pressed(KEY_LEFT)), float(Input.is_physical_key_pressed(KEY_DOWN)) - float(Input.is_physical_key_pressed(KEY_UP)))
		act = Input.is_physical_key_pressed(KEY_ENTER) or Input.is_physical_key_pressed(KEY_KP_0)
		hurry = Input.is_physical_key_pressed(KEY_CTRL)
		stab = Input.is_physical_key_pressed(KEY_KP_1) or Input.is_physical_key_pressed(KEY_SLASH) or Input.is_physical_key_pressed(KEY_PERIOD)
	elif src.begins_with("pad"):
		var dv := int(src.substr(3))
		mv = Vector2(Input.get_joy_axis(dv, JOY_AXIS_LEFT_X), Input.get_joy_axis(dv, JOY_AXIS_LEFT_Y))
		if Input.is_joy_button_pressed(dv, JOY_BUTTON_DPAD_LEFT): mv.x = -1
		if Input.is_joy_button_pressed(dv, JOY_BUTTON_DPAD_RIGHT): mv.x = 1
		if Input.is_joy_button_pressed(dv, JOY_BUTTON_DPAD_UP): mv.y = -1
		if Input.is_joy_button_pressed(dv, JOY_BUTTON_DPAD_DOWN): mv.y = 1
		act = Input.is_joy_button_pressed(dv, JOY_BUTTON_A)
		hurry = Input.is_joy_button_pressed(dv, JOY_BUTTON_RIGHT_SHOULDER) or Input.get_joy_axis(dv, JOY_AXIS_TRIGGER_RIGHT) > 0.4
		stab = Input.is_joy_button_pressed(dv, JOY_BUTTON_X)
	var was: bool = prev.get(src, false)
	prev[src] = stab
	return byte(mv, act, hurry, stab and not was)

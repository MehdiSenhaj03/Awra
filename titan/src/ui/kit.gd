class_name Kit
## The interface kit: palette, fonts (the web version's Unbounded, Space Grotesk, JetBrains Mono), labels,
## panels, images, and a few shaders. Every screen is built in code at 1920×1080 and scaled to the window.

const CYAN := Color("#e9b872")      # the kit's accent: dusty gold
const BRASS := Color("#d9a35e")
const RED := Color("#d8343c")
const MAGENTA := Color("#ff2fb9")
const GOLD := Color("#efc98a")
const CRIMSON := Color("#b0182c")
const IVORY := Color("#f2ebe0")
const CREAM := Color("#f6ecd9")
const INK := Color("#04050b")
const PALE := Color("#f2ebe0")
const DIM := Color("#7d7062")
const SOFT := Color("#d6cbbb")
const MUTED := Color("#a39582")
const W := 1920.0
const H := 1080.0

const FILES := {
	"display": "cinzel",
	"deco": "marcellus",
	"body": "ibm-plex-sans",
	"mono": "ibm-plex-mono",
}
const PORTRAIT := {}

static var _fonts: Dictionary = {}
static var _vars: Dictionary = {}
static var _tex: Dictionary = {}

static func _weight(fam: String, w: int) -> int:
	var have := {"display": [400, 700], "deco": [400], "body": [400, 600], "mono": [400, 600]}
	var best: int = have[fam][0]
	for x: int in have[fam]:
		if absi(x - w) < absi(best - w):
			best = x
	return best

static func _file(fam: String, w: int) -> FontFile:
	var key := "%s%d" % [fam, w]
	var f: FontFile = _fonts.get(key)
	if f == null:
		var base: String = FILES[fam]
		f = load("res://assets/fonts/%s-latin-%d-normal.woff2" % [base, w])
		var ext: FontFile = load("res://assets/fonts/%s-latin-ext-%d-normal.woff2" % [base, w])
		var sym := SystemFont.new()
		sym.font_names = PackedStringArray(["Segoe UI Symbol", "Noto Sans Symbols 2", "Noto Sans Symbols", "DejaVu Sans", "Symbola", "Arial Unicode MS"])
		f.fallbacks = [ext, sym] if ext else [sym]
		_fonts[key] = f
	return f

## A font of a family and weight, with extra letter spacing in em (as in the CSS) for a given size.
static func font(fam: String, w: int = 400, spacing_em: float = 0.0, size: int = 16) -> Font:
	var wf := _weight(fam, w)
	var px := int(roundf(spacing_em * size))
	var key := "%s|%d|%d" % [fam, wf, px]
	var v: FontVariation = _vars.get(key)
	if v == null:
		v = FontVariation.new()
		v.base_font = _file(fam, wf)
		v.spacing_glyph = px
		_vars[key] = v
	return v

static func label(parent: Node, text: String, size: int, color: Color = PALE, fam: String = "body", w: int = 400, spacing_em: float = 0.0, glow: Color = Color(0, 0, 0, 0), glow_size: int = 0) -> Label:
	var l := Label.new()
	l.text = text
	var ls := LabelSettings.new()
	ls.font = font(fam, w, spacing_em, size)
	ls.font_size = size
	ls.font_color = color
	if glow.a > 0.0:
		ls.shadow_color = glow
		ls.shadow_size = glow_size
		ls.shadow_offset = Vector2.ZERO
	l.label_settings = ls
	l.mouse_filter = Control.MOUSE_FILTER_IGNORE
	if parent:
		parent.add_child(l)
	return l

static func rich(parent: Node, bb: String, size: int, color: Color = PALE, fam: String = "body", w: int = 400) -> RichTextLabel:
	var r := RichTextLabel.new()
	r.bbcode_enabled = true
	r.fit_content = true
	r.scroll_active = false
	r.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	r.add_theme_font_override("normal_font", font(fam, w, 0.0, size))
	r.add_theme_font_override("bold_font", font("display", 700, 0.0, size))
	r.add_theme_font_override("mono_font", font("mono", 400, 0.0, size))
	r.add_theme_font_size_override("normal_font_size", size)
	r.add_theme_font_size_override("bold_font_size", size)
	r.add_theme_font_size_override("mono_font_size", int(size * 0.8))
	r.add_theme_color_override("default_color", color)
	r.mouse_filter = Control.MOUSE_FILTER_IGNORE
	r.text = bb
	if parent:
		parent.add_child(r)
	return r

## The few HTML tags of the translations, as BBCode.
static func html(s: String) -> String:
	s = s.replace("<kbd>", "[bgcolor=#ffffff1c][color=#ffffff][code] ").replace("</kbd>", " [/code][/color][/bgcolor] ")
	s = s.replace("<b>", "[b]").replace("</b>", "[/b]").replace("<br>", "\n")
	return s

static func tex(name: String) -> Texture2D:
	var t: Texture2D = _tex.get(name)
	if t == null:
		for ext in ["jpg", "png"]:
			var p := "res://assets/art/%s.%s" % [name, ext]
			if ResourceLoader.exists(p):
				t = load(p)
				_tex[name] = t
				break
	return t

static func image(parent: Node, name: String, cover: bool = true) -> TextureRect:
	var r := TextureRect.new()
	r.texture = tex(name)
	r.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	r.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_COVERED if cover else TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	r.mouse_filter = Control.MOUSE_FILTER_IGNORE
	if parent:
		parent.add_child(r)
	return r

static func full(c: Control) -> Control:
	c.set_anchors_preset(Control.PRESET_FULL_RECT)
	c.offset_left = 0
	c.offset_top = 0
	c.offset_right = 0
	c.offset_bottom = 0
	return c

static func rect(parent: Node, color: Color, pos: Vector2 = Vector2.ZERO, size: Vector2 = Vector2.ZERO) -> ColorRect:
	var r := ColorRect.new()
	r.color = color
	r.position = pos
	r.size = size
	r.mouse_filter = Control.MOUSE_FILTER_IGNORE
	if parent:
		parent.add_child(r)
	return r

## The dark glass panel with its cyan corner mark.
static func panel(parent: Node) -> PanelContainer:
	var p := GlassPanel.new()
	var sb := StyleBoxFlat.new()
	sb.bg_color = Color(0.09, 0.07, 0.06, 0.93)
	sb.border_color = Color(0.9, 0.7, 0.45, 0.3)
	sb.set_border_width_all(1)
	sb.shadow_color = Color(0, 0, 0, 0.8)
	sb.shadow_size = 40
	sb.shadow_offset = Vector2(0, 30)
	p.add_theme_stylebox_override("panel", sb)
	p.mouse_filter = Control.MOUSE_FILTER_IGNORE
	if parent:
		parent.add_child(p)
	return p

static func margin(parent: Node, l: int, t: int, r: int, b: int) -> MarginContainer:
	var m := MarginContainer.new()
	m.add_theme_constant_override("margin_left", l)
	m.add_theme_constant_override("margin_top", t)
	m.add_theme_constant_override("margin_right", r)
	m.add_theme_constant_override("margin_bottom", b)
	m.mouse_filter = Control.MOUSE_FILTER_IGNORE
	if parent:
		parent.add_child(m)
	return m

static func vbox(parent: Node, gap: int = 0) -> VBoxContainer:
	var b := VBoxContainer.new()
	b.add_theme_constant_override("separation", gap)
	b.mouse_filter = Control.MOUSE_FILTER_IGNORE
	if parent:
		parent.add_child(b)
	return b

static func hbox(parent: Node, gap: int = 0) -> HBoxContainer:
	var b := HBoxContainer.new()
	b.add_theme_constant_override("separation", gap)
	b.mouse_filter = Control.MOUSE_FILTER_IGNORE
	if parent:
		parent.add_child(b)
	return b

static func h_title(parent: Node, text: String, size: int = 34) -> Label:
	return label(parent, text, size, GOLD, "display", 700, 0.12, Color(1.0, 0.55, 0.2, 0.3), 12)

static func h_sub(parent: Node, text: String) -> Label:
	return label(parent, text, 13, BRASS, "display", 400, 0.35)

static func fmt_int(v: float) -> String:
	var s := str(int(roundf(v)))
	var out := ""
	var n := s.length()
	for i in n:
		out += s[i]
		var left := n - 1 - i
		if left > 0 and left % 3 == 0 and s[i] != "-":
			out += " "
	return out

## A painted background with a slow Ken Burns drift, optional blur and dimming.
static func background(parent: Node, name: String, blur: float = 0.0, bright: float = 1.0) -> TextureRect:
	var r := image(parent, name)
	full(r)
	r.set_anchors_preset(Control.PRESET_FULL_RECT)
	r.offset_left = -W * 0.04
	r.offset_top = -H * 0.04
	r.offset_right = W * 0.04
	r.offset_bottom = H * 0.04
	var m := ShaderMaterial.new()
	m.shader = preload("res://src/ui/shaders/bg.gdshader")
	m.set_shader_parameter("blur", blur)
	m.set_shader_parameter("bright", bright)
	r.material = m
	r.texture_filter = CanvasItem.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS
	r.pivot_offset = Vector2(W * 0.54, H * 0.54)
	var tw := r.create_tween().set_loops()
	tw.tween_property(r, "scale", Vector2(1.1, 1.1), 40.0).from(Vector2(1.0, 1.0)).set_trans(Tween.TRANS_SINE)
	tw.tween_property(r, "scale", Vector2(1.0, 1.0), 40.0).set_trans(Tween.TRANS_SINE)
	return r

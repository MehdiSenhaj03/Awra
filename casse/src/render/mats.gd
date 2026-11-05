class_name Mats
## Shared materials. Per-object variation (rim colour, hit flash, shield tint, fade) goes through instance
## uniforms, so a whole army of foes shares a handful of shaders and materials.

const GLOW := preload("res://src/render/shaders/glow.gdshader")
const GLOW_FADE := preload("res://src/render/shaders/glow_fade.gdshader")
const OBSIDIAN := preload("res://src/render/shaders/obsidian.gdshader")
const OBSIDIAN_DS := preload("res://src/render/shaders/obsidian_ds.gdshader")
const OBSIDIAN_FADE := preload("res://src/render/shaders/obsidian_fade.gdshader")

static var _cache: Dictionary = {}
static var _lin: Dictionary = {}

## sRGB hex colour to linear RGB (what the shaders and lights expect).
static func lin(c: int) -> Color:
	var v: Variant = _lin.get(c)
	if v != null:
		return v
	var col := Color.hex((c << 8) | 0xff).srgb_to_linear()
	_lin[c] = col
	return col

static func lin3(c: int, k: float = 1.0) -> Vector3:
	var l := lin(c)
	return Vector3(l.r * k, l.g * k, l.b * k)

static func srgb(c: int) -> Color:
	return Color.hex((c << 8) | 0xff)

static func glow(color: int, k: float = 3.0) -> ShaderMaterial:
	var key := "g|%d|%s" % [color, k]
	var m: ShaderMaterial = _cache.get(key)
	if m == null:
		m = ShaderMaterial.new()
		m.shader = GLOW
		m.set_shader_parameter("color", lin3(color, k))
		_cache[key] = m
	return m

static func glow_fade(color: int, k: float, opacity: float = 1.0) -> ShaderMaterial:
	var key := "gf|%d|%s|%s" % [color, k, opacity]
	var m: ShaderMaterial = _cache.get(key)
	if m == null:
		m = ShaderMaterial.new()
		m.shader = GLOW_FADE
		m.set_shader_parameter("color", lin3(color, k))
		m.set_shader_parameter("opacity", opacity)
		_cache[key] = m
	return m

static func obsidian(tint: int = 0x0b0c14, rough: float = 0.16, metal: float = 0.92, double_sided: bool = false) -> ShaderMaterial:
	var key := "o|%d|%s|%s|%s" % [tint, rough, metal, double_sided]
	var m: ShaderMaterial = _cache.get(key)
	if m == null:
		m = ShaderMaterial.new()
		m.shader = OBSIDIAN_DS if double_sided else OBSIDIAN
		m.set_shader_parameter("tint", srgb(tint))
		m.set_shader_parameter("rough", rough)
		m.set_shader_parameter("metal", metal)
		_cache[key] = m
	return m

static func obsidian_fade(tint: int, rough: float, metal: float = 0.8) -> ShaderMaterial:
	var key := "of|%d|%s|%s" % [tint, rough, metal]
	var m: ShaderMaterial = _cache.get(key)
	if m == null:
		m = ShaderMaterial.new()
		m.shader = OBSIDIAN_FADE
		m.set_shader_parameter("tint", srgb(tint))
		m.set_shader_parameter("rough", rough)
		m.set_shader_parameter("metal", metal)
		_cache[key] = m
	return m

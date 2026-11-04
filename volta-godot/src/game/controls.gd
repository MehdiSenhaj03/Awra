class_name Controls
extends RefCounted
## What the player asks for this frame (from devices, or from tests and the bot).

var mx: float = 0.0
var my: float = 0.0
## Aim direction (unit vector, world space).
var ax: float = 1.0
var ay: float = 0.0
var fire: bool = false
var fire_pressed: bool = false
var dash: bool = false
var pulse: bool = false
var overdrive: bool = false

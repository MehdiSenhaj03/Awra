class_name Truck
extends Node3D
## "La Bailleuse": the armoured chase pick-up, built from boxes and cylinders — body, cab with grilles, light bar,
## instrument mast with a spinning anemometer, probes on the rack, turning wheels, headlights.

var wheels: Array[Node3D] = []
var anemo: Node3D
var lights: Array[SpotLight3D] = []
var body: Node3D
var probe_nodes: Array[Node3D] = []
var _spin := 0.0
var _roll := 0.0

func _init(paint: Color = Color(0.32, 0.33, 0.34), stripe: Color = Color(1.0, 0.45, 0.08)) -> void:
	body = Node3D.new()
	add_child(body)
	var pm := Shapes.mat(paint, 0.45, 0.35)
	var sm := Shapes.mat(stripe, 0.5, 0.1)
	var dark := Shapes.mat(Color(0.06, 0.06, 0.07), 0.6, 0.2)
	var glass := Shapes.mat(Color(0.08, 0.1, 0.12), 0.05, 0.6)
	var chrome := Shapes.mat(Color(0.7, 0.7, 0.72), 0.25, 0.9)
	# Chassis and bed (the truck faces +X).
	Shapes.put(body, Shapes.box(Vector3(5.6, 0.7, 2.2)), pm, Vector3(0, 0.95, 0))
	Shapes.put(body, Shapes.box(Vector3(5.62, 0.16, 2.22)), sm, Vector3(0, 1.12, 0))
	Shapes.put(body, Shapes.box(Vector3(2.4, 1.0, 2.1)), pm, Vector3(0.6, 1.75, 0))
	Shapes.put(body, Shapes.box(Vector3(2.0, 0.6, 2.12)), glass, Vector3(0.75, 1.85, 0))
	# Grilles over the windows.
	for k in 6:
		Shapes.put(body, Shapes.box(Vector3(0.04, 0.6, 2.16)), dark, Vector3(0.0 + k * 0.32, 1.85, 0))
	Shapes.put(body, Shapes.box(Vector3(0.25, 0.8, 2.2)), dark, Vector3(2.9, 1.0, 0))
	Shapes.put(body, Shapes.box(Vector3(2.4, 0.5, 2.0)), dark, Vector3(-1.6, 1.4, 0))
	# Light bar.
	Shapes.put(body, Shapes.box(Vector3(0.3, 0.15, 1.9)), dark, Vector3(1.45, 2.32, 0))
	var lamp := Shapes.mat(Color(1, 1, 0.9), 0.2, 0.0, Color(1.0, 0.95, 0.8), 3.0)
	for k in 5:
		Shapes.put(body, Shapes.box(Vector3(0.05, 0.1, 0.25)), lamp, Vector3(1.61, 2.32, -0.8 + k * 0.4))
	Shapes.put(body, Shapes.box(Vector3(0.05, 0.2, 0.4)), lamp, Vector3(2.81, 1.15, 0.75))
	Shapes.put(body, Shapes.box(Vector3(0.05, 0.2, 0.4)), lamp, Vector3(2.81, 1.15, -0.75))
	# Instrument mast with its anemometer.
	Shapes.put(body, Shapes.cyl(0.05, 0.05, 1.6, 6), chrome, Vector3(-0.4, 3.0, 0.5))
	anemo = Node3D.new()
	anemo.position = Vector3(-0.4, 3.85, 0.5)
	body.add_child(anemo)
	for k in 3:
		var arm := Node3D.new()
		arm.rotation.y = TAU * k / 3.0
		anemo.add_child(arm)
		Shapes.put(arm, Shapes.box(Vector3(0.35, 0.03, 0.03)), chrome, Vector3(0.17, 0, 0))
		Shapes.put(arm, Shapes.sphere(0.07, 8, 4), sm, Vector3(0.35, 0, 0))
	Shapes.put(body, Shapes.cyl(0.02, 0.02, 0.9, 4), dark, Vector3(-0.9, 2.6, -0.6))
	# Probes on the rack: flat metal domes.
	var probe_m := Shapes.mat(Color(0.75, 0.76, 0.78), 0.3, 0.8)
	var dome := Shapes.lathe(PackedVector2Array([Vector2(0.45, 0), Vector2(0.44, 0.08), Vector2(0.3, 0.2), Vector2(0.0, 0.25)]), 16)
	for k in 5:
		var p := Shapes.put(body, dome, probe_m, Vector3(-1.2 - (k % 2) * 0.9, 1.65, -0.5 + (k / 2) * 0.5))
		p.scale = Vector3.ONE * 0.8
		probe_nodes.append(p)
	# Wheels.
	var tyre := Shapes.mat(Color(0.05, 0.05, 0.05), 0.9)
	for wp in [Vector3(1.9, 0.55, 1.05), Vector3(1.9, 0.55, -1.05), Vector3(-1.8, 0.55, 1.05), Vector3(-1.8, 0.55, -1.05)]:
		var w := Node3D.new()
		w.position = wp
		add_child(w)
		var c := Shapes.put(w, Shapes.cyl(0.55, 0.55, 0.45, 14), tyre, Vector3.ZERO)
		c.rotation.x = PI / 2
		var hub := Shapes.put(w, Shapes.cyl(0.25, 0.25, 0.47, 8), chrome, Vector3.ZERO)
		hub.rotation.x = PI / 2
		wheels.append(w)
	for z in [0.75, -0.75]:
		var l := SpotLight3D.new()
		l.position = Vector3(2.85, 1.15, z)
		l.rotation.y = -PI / 2
		l.spot_range = 70.0
		l.spot_angle = 28.0
		l.light_energy = 3.0
		l.light_color = Color(1.0, 0.95, 0.85)
		l.shadow_enabled = false
		add_child(l)
		lights.append(l)

func set_probes(n: int) -> void:
	for i in probe_nodes.size():
		probe_nodes[i].visible = i < n

## Per frame: speed in m/s (wheels), wind in m/s (anemometer), steering (-1..1), roughness of the ground.
func animate(dt: float, speed: float, wind: float, steer: float, rough: float) -> void:
	_spin += dt * (2.0 + wind * 0.6)
	anemo.rotation.y = _spin
	for i in wheels.size():
		var w := wheels[i]
		w.rotation.z -= speed * dt / 0.55
		if i < 2:
			w.rotation.y = -steer * 0.45
	_roll += dt * 9.0
	body.position.y = sin(_roll * (1.0 + rough)) * 0.03 * rough * minf(1.0, absf(speed) / 6.0)
	body.rotation.x = sin(_roll * 0.7) * 0.012 * rough

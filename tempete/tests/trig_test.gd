extends SceneTree
func _init() -> void:
	var maxe := 0.0
	for a in range(0, 4096, 7):
		var f := sin(a * TAU / 4096.0) * 65536.0
		maxe = maxf(maxe, absf(f - Trig.sina(a)))
		maxe = maxf(maxe, absf(cos(a * TAU / 4096.0) * 65536.0 - Trig.cosa(a)))
	var ae := 0
	for k in 2000:
		var ang := (k * 37) % 4096
		var x := Trig.cosa(ang) * 1000
		var y := Trig.sina(ang) * 1000
		ae = maxi(ae, absi(Trig.adiff(ang, Trig.atan2i(y, x))))
	print("max sin/cos error (1/65536): ", maxe, "  max atan2 error (1/4096 turn): ", ae, "  isqrt(10^12)=", Trig.isqrt(1000000000000))
	quit()

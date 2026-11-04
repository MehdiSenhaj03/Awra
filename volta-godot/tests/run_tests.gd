extends SceneTree
## Headless test run: every stage completable by the invulnerable bot, every Conductor reaches phase 3,
## then a difficulty pass with a vulnerable bot. Usage: godot --headless -s res://tests/run_tests.gd [-- ids]

func _init() -> void:
	var ids: Array = Stages.order()
	var args := OS.get_cmdline_user_args()
	if args.size() > 0:
		ids = args[0].split(",")
	var mode := args[1] if args.size() > 1 else "god"
	var fails := 0
	for id in ids:
		var r := Bot.play(id, mode == "god", 60 * 60 * 8 if mode == "god" else 60 * 300)
		var ok: bool = r.over == "win" and (not str(id).ends_with("_4") or r.phase == 3)
		if mode == "god" and not ok:
			fails += 1
		print(id, " ", JSON.stringify(r), "" if ok or mode != "god" else "  <-- FAIL")
	print("FAILS=", fails)
	quit(1 if fails > 0 else 0)

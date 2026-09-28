-- Hyprland 0.55+ (tested with 0.56.2). Load after your theme / HyDE.
-- After installing HandMouse: local handmouse = "GestC"
-- For a source checkout, set an absolute path to build/GestC instead.
local handmouse = "GestC"

hl.bind("SUPER + SHIFT + F9", hl.dsp.exec_cmd(handmouse .. " --toggle-pause"))
hl.bind("SUPER + SHIFT + F10", hl.dsp.exec_cmd(handmouse .. " --stop"))
-- Keep only one SUPER + left-button binding if your config already has it.
hl.bind("SUPER + mouse:272", hl.dsp.window.drag(), { mouse = true })

hl.window_rule({
    name = "handmouse-glass",
    match = { class = "handmouse" },
    no_blur = false,
    opaque = false,
    force_rgbx = false,
    rounding = 20,
    opacity = "1.0 override 1.0 override",
})
-- Blur is a compositor-wide setting. Reuse your theme's settings if enabled.
-- hl.config({ decoration = { blur = { enabled = true, size = 8, passes = 3 } } })

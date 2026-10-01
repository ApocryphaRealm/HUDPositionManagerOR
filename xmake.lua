-- HUD Position Manager for The Elder Scrolls IV: Oblivion Remastered (OBSE64 plugin): move, resize and hide each part
-- of the game's own HUD from an Apocrypha Menu Framework page (plan: 4. plans/hud-position-manager-oblivion/PLAN.md).
-- rule 45: no build-machine paths in any compiled object - set BEFORE includes() so CommonLibOB64's own library
-- target gets it too. /d1trimfile is wrapped in a TABLE so xmake passes it as one argument (logic library 7598); no
-- trailing separator. /PDBALTPATH:%_PDB% makes the debug directory record only the PDB's file name.
add_cxflags({"/d1trimfile:$(projectdir)"}, {force = true, expand = false})
add_shflags("/PDBALTPATH:%_PDB%", {force = true})

includes("lib/commonlibob64")

set_project("HUDPositionManager")
set_version("1.0.1")
set_license("GPL-3.0-or-later")
set_languages("c++23")
set_warnings("allextra")

add_rules("mode.debug", "mode.releasedbg")
add_requires("nlohmann_json")
add_rules("plugin.vsxmake.autoupdate")

-- Dear ImGui 1.90.8 docking, the framework's own build (ApocryphaMenuFrameworkOR/xmake.lua): AMF::UseFrameworkImGui()
-- checks the version and struct sizes byte for byte, so this must stay the same source and the same defines. Only the
-- core is compiled - the page draws inside the framework's frame.
target("imgui")
    set_kind("static")
    set_warnings("none")
    add_files("extern/imgui/imgui.cpp", "extern/imgui/imgui_draw.cpp", "extern/imgui/imgui_tables.cpp",
              "extern/imgui/imgui_widgets.cpp")
    add_includedirs("extern/imgui", {public = true})

target("HUDPositionManager")
    add_rules("commonlibob64.plugin", {
        name = "HUDPositionManager",
        author = "ApocryphaRealm",
        description = "HUD Position Manager - move, resize and hide each part of the HUD, or keep it visible at all times (Oblivion Remastered)"
    })
    add_deps("imgui")
    add_packages("nlohmann_json")
    add_syslinks("user32", "shell32", "ole32")
    on_load(function (target)
        target:add("defines", "HPM_VERSION=\"" .. (target:version() or "0.0.0") .. "\"")
    end)
    add_files("src/**.cpp")
    add_headerfiles("src/**.h", "include/**.h")
    add_includedirs("include", "src")
    set_pcxxheader("src/pch.h")

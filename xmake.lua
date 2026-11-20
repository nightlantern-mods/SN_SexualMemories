-- set minimum xmake version
set_xmakever("3.0.0")

-- CommonLibSSE-NG 7.5.4: point COMMONLIB_SSE_NG at a checkout (the default is the author's).
includes(os.getenv("COMMONLIB_SSE_NG") or "C:/Modding/SkyrimNetAddon/src/skse/lib/commonlibsse-ng")

local PLUGIN_NAME = "SN_SexualHistory"
local PLUGIN_VERSION = "0.4.2"

set_project(PLUGIN_NAME)
set_version(PLUGIN_VERSION)
set_license("MIT")

set_languages("c++23")
set_warnings("allextra")
add_defines("UNICODE", "_UNICODE")

set_policy("package.requires_lock", true)

add_requires("nlohmann_json v3.12.0")

add_rules("mode.release")

target(PLUGIN_NAME)
    add_deps("commonlibsse-ng")
    add_packages("nlohmann_json")

    add_rules("commonlibsse-ng.plugin", {
       name = PLUGIN_NAME,
       author = "nightlantern-mods",
       description = "Turns each OStim or SexLab scene into SkyrimNet memories for everyone who took part"
    })

    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")
    add_cxflags("/utf-8")

    -- Lays out the DLL, the SkyrimNet prompt bundle and the map page in dist/, as they go into Data.
    after_build(function (target)
        local plugindir = path.join(os.projectdir(), "dist", "SKSE", "Plugins")
        os.mkdir(plugindir)
        os.cp(target:targetfile(), plugindir)

        local skyrimnetdir = path.join(plugindir, "SkyrimNet", "external")
        os.mkdir(skyrimnetdir)
        os.cp(path.join(os.projectdir(), "skyrimnet-plugin", "*"), skyrimnetdir)
        -- The relationship map's page, where each UI host looks for it. Its texts are in English
        -- unless SKSE/Plugins/SN_SexualHistory/strings.json (tools/make_strings.py) says otherwise.
        local view = path.join(os.projectdir(), "view", "index.html")
        local data = path.join(os.projectdir(), "dist")
        for _, dir in ipairs({ path.join(data, "PrismaUI", "views", "SN_SexualHistory"), path.join(data, "Magelight", "SN_SexualHistory", "views") }) do
            os.mkdir(dir)
            os.cp(view, path.join(dir, "index.html"))
        end
        cprint("${bright green}laid out in: ${clear}dist/")
    end)

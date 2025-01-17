add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", {outputdir = "$(projectdir)"})
add_requires("spdlog", "glfw", "vulkansdk")
target("learn-vulkan")
    set_kind("binary")
    set_languages("c99", "c++20")
    add_defines(
        "NDEBUG", 
    	"DEBUG",
        "NOMINMAX",
        "NUM_FRAMES_IN_FLIGHT=3",
        "CLIENT_WIDTH=1024",
        "CLIENT_HEIGHT=768",
        "PROJ_DIR=$(projectdir)/"
    )
    add_files("$(projectdir)/source/**.cpp")
    add_packages("spdlog", "glfw", "vulkansdk")
target_end()
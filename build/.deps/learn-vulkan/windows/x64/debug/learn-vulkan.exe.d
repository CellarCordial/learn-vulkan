{
    files = {
        [[build\.objs\learn-vulkan\windows\x64\debug\source\core\math\bounds.cpp.obj]],
        [[build\.objs\learn-vulkan\windows\x64\debug\source\core\math\matrix.cpp.obj]],
        [[build\.objs\learn-vulkan\windows\x64\debug\source\core\math\quad_tree.cpp.obj]],
        [[build\.objs\learn-vulkan\windows\x64\debug\source\core\math\quaternion.cpp.obj]],
        [[build\.objs\learn-vulkan\windows\x64\debug\source\core\math\rectangle.cpp.obj]],
        [[build\.objs\learn-vulkan\windows\x64\debug\source\core\math\surface.cpp.obj]],
        [[build\.objs\learn-vulkan\windows\x64\debug\source\core\parallel\parallel.cpp.obj]],
        [[build\.objs\learn-vulkan\windows\x64\debug\source\core\parallel\thread_pool.cpp.obj]],
        [[build\.objs\learn-vulkan\windows\x64\debug\source\core\tools\bit_allocator.cpp.obj]],
        [[build\.objs\learn-vulkan\windows\x64\debug\source\core\tools\ecs.cpp.obj]],
        [[build\.objs\learn-vulkan\windows\x64\debug\source\core\tools\hash_table.cpp.obj]],
        [[build\.objs\learn-vulkan\windows\x64\debug\source\glfw_window.cpp.obj]],
        [[build\.objs\learn-vulkan\windows\x64\debug\source\main.cpp.obj]],
        [[build\.objs\learn-vulkan\windows\x64\debug\source\vulkan_base.cpp.obj]]
    },
    values = {
        [[C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.42.34433\bin\HostX64\x64\link.exe]],
        {
            "-nologo",
            "-dynamicbase",
            "-nxcompat",
            "-machine:x64",
            [[-libpath:C:\Users\cir12\AppData\Local\.xmake\packages\g\glfw\3.4\fc47358da159466996f7d289b4cf1b4e\lib]],
            [[-libpath:D:\Program Files\VulkanSDK\lib]],
            "-debug",
            [[-pdb:build\windows\x64\debug\learn-vulkan.pdb]],
            "glfw3.lib",
            "opengl32.lib",
            "vulkan-1.lib",
            "user32.lib",
            "shell32.lib",
            "gdi32.lib"
        }
    }
}
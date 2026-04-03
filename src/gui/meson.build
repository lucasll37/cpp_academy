imgui_dep = subproject('imgui').get_variable('imgui_dep')

executable('imgui_demo',
    ['main.cpp'],
    dependencies: [fmt_dep, glfw_dep, opengl_dep, imgui_dep],
)
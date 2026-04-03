imgui_inc = include_directories('.')

imgui_src = files(
    'imgui.cpp',
    'imgui_draw.cpp',
    'imgui_tables.cpp',
    'imgui_widgets.cpp',
    'backends/imgui_impl_glfw.cpp',
    'backends/imgui_impl_opengl3.cpp',
)

imgui_dep = declare_dependency(
    include_directories: imgui_inc,
    sources: imgui_src,
)
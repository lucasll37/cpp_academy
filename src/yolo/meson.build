onnx_dep   = dependency('onnxruntime')
opencv_dep = dependency('opencv')


executable('yolo',
    ['main.cpp'],
    dependencies: [fmt_dep, onnx_dep, opencv_dep],
    install: true
)

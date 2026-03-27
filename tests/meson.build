test_exe = executable('test_hello_world',
    'test_hello_world.cpp',
    link_with: lib_hello_world,
    include_directories: inc,
    dependencies: [gtest_dep],
)

test('hello_world_tests', test_exe)
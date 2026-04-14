doxygen = find_program('doxygen', required: true)

doxyfile = configure_file(
  input: 'Doxyfile.in',
  output: 'Doxyfile',
  configuration: {
    'PROJECT_NAME': meson.project_name(),
    'OUTPUT_DIR': meson.current_build_dir() / 'docs'
  }
)

doc_target = custom_target(
  'docs',
  input: doxyfile,
  output: 'html',
  command: [doxygen, doxyfile],
  build_by_default: true
)
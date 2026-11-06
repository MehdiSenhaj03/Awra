# SPDX-License-Identifier: LGPL-2.1-or-later

blueprint_compiler = find_program(
  'blueprint-compiler',
  required: get_option('blueprint'),
)
if blueprint_compiler.found()
  if gir.length() == 0
    error('Blueprint validation requires introspection to be enabled')
  endif
  env_program = find_program('env')
  custom_target(
    'blueprint-example',
    input: 'awra-example.blp',
    output: '.',
    command: [
      env_program,
      'GI_TYPELIB_PATH=' + meson.project_build_root(),
      blueprint_compiler,
      'batch-compile',
      '--minify',
      '@OUTPUT@',
      '@CURRENT_SOURCE_DIR@',
      '@INPUT@',
    ],
    depends: gir,
  )
endif

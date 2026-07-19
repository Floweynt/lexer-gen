project('lexer-gen', 'cpp', 
  default_options : ['cpp_std=c++20'], 
  version: '1.0.0'
)

sources = [
  'src/main.cpp',
  'src/machine/dfa.cpp',
  'src/machine/nfa.cpp',
  'src/argparse.cpp',
  'src/diagnostics.cpp',
  'src/dump.cpp',
  'src/lexergen.cpp',
  'src/regex.cpp',
  'src/regex_parser.cpp'
]

include_dirs = [
  'include',
  '.'
]

conf_data = configuration_data()
conf_data.set_quoted('VERSION', meson.project_version())
conf_data.set_quoted('MESON_CXX_COMPILER', meson.get_compiler('cpp').get_id())
configure_file(input: 'build_config.h.in', output: 'build_config.h', configuration: conf_data)

lexer_gen = executable('lexer-gen', sources,
    dependencies: [],
    cpp_pch: 'pch/pch.h',
    include_directories: include_directories(include_dirs),
    install: true,
)

project('solar-shell', 'cpp', default_options: ['cpp_std=c++20', 'warning_level=2'])

fmt_dep = dependency('fmt', method: 'pkg-config')

shell_includes = [ '../../include_ext' ]
shell_deps = [ fmt_dep ]
shell_files = ['main.cc', 'shell.cc', 'command.cc', 'text_renderer.cc', 'guestbook_io.cc']
executable('solar-shell', shell_files, dependencies: shell_deps, include_directories: shell_includes)

shell_test_files = ['shell_test.cc', 'shell.cc', 'command.cc', 'text_renderer.cc', 'guestbook_io.cc']
shell_test = executable('shell_test', shell_test_files, dependencies: shell_deps, include_directories: shell_includes)
test('shell_tests', shell_test)

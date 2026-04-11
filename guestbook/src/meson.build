project('guestbook', 'cpp', default_options: ['cpp_std=c++20', 'warning_level=2'])

fmt_dep = dependency('fmt', method: 'pkg-config')

guestbook_deps = [ fmt_dep ]
guestbook_includes = [ '../../include_ext' ]
guestbook_files = [ 'main.cc' ]
guestbook_exe = executable('guestbook', guestbook_files, dependencies: guestbook_deps, include_directories: guestbook_includes)

guestbook_test_files = [ 'guestbook_test.cc' ]
guestbook_test = executable('guestbook_test', guestbook_test_files, dependencies: guestbook_deps, include_directories: guestbook_includes)
test('guestbook_tests', guestbook_test)

integration_test = find_program('integration_test.sh')
test('guestbook_integration', integration_test, args: [guestbook_exe])
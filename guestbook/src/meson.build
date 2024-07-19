project('guestbook', 'cpp', default_options: ['cpp_std=c++20'])

fmt_dep = dependency('fmt', method: 'pkg-config')

guestbook_deps = [ fmt_dep ]
guestbook_includes = [ '../../include_ext' ]
guestbook_files = [ 'main.cc' ]
executable('guestbook', guestbook_files, dependencies: guestbook_deps, include_directories: guestbook_includes)
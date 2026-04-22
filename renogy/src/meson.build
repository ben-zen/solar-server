project('renogy', 'cpp', default_options: ['cpp_std=c++20', 'warning_level=2'])

fmt_dep = dependency('fmt', method: 'pkg-config')

renogy_includes = [ '../../include_ext' ]
renogy_deps = [ fmt_dep ]
renogy_files = ['modbus.cc', 'renogy.cc', 'serial.cc']

renogy_test_files = ['renogy_test.cc', 'modbus.cc', 'renogy.cc', 'serial.cc']
renogy_test = executable('renogy_test', renogy_test_files, dependencies: renogy_deps, include_directories: renogy_includes)
test('renogy_tests', renogy_test)

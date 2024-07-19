project('solar-logger', 'cpp', default_options: ['cpp_std=c++20'])

curl_dep = dependency('libcurl', method: 'pkg-config')
fmt_dep = dependency('fmt', method: 'pkg-config')

logger_includes = [ '../../include_ext' ]
logger_deps = [ curl_dep, fmt_dep ]
logger_files = ['curl.cc', 'main.cc', 'report.cc', 'transform.cc', 'weather.cc']
executable('logger', logger_files, dependencies: logger_deps, include_directories: logger_includes)
project('solar-logger', 'cpp', default_options: ['cpp_std=c++20'])

curl_dep = dependency('libcurl', method: 'pkg-config')

logger_deps = [ curl_dep ]
logger_files = ['curl.cc', 'main.cc', 'weather.cc']
executable('logger', logger_files, dependencies: logger_deps)
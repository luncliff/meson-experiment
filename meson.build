#
# https://mesonbuild.com/Builtin-options.html
# https://mesonbuild.com/Reference-manual.html
# https://mesonbuild.com/Installing.html
# https://mesonbuild.com/Unit-tests.html
# https://mesonbuild.com/Machine-files.html
#
project('experiment', 'cpp',
  meson_version: '>=1.5.0',
  version: '0.1',
  default_options: ['cpp_std=c++20']
)

compiler = meson.get_compiler('cpp')

system_deps = []
if target_machine.system() == 'windows'
  add_global_arguments('/DNOMINMAX', language: 'cpp')
  system_deps = [
    compiler.find_library('kernel32', required: true),
    compiler.find_library('d3d11', required: true),
    compiler.find_library('d3d12', required: true),
    compiler.find_library('dxgi', required: true),
  ]
endif

external_deps = [
  dependency('spdlog', method: 'cmake', modules: ['spdlog::spdlog'], required: true),
]
if target_machine.system() == 'windows'
  # external_deps += [
  #   compiler.find_library('dispatch', required: true),
  # ]
endif
if target_machine.system() == 'darwin'
  external_deps += [
    compiler.find_library('c++', required: true),
    dependency('appleframeworks', modules:['Foundation'])
  ]
endif

if get_option('vulkan')
  external_deps += [
    dependency('Vulkan', method: 'cmake', modules: ['Vulkan::Vulkan'], required: true)
  ]
endif
if get_option('opencl')
  external_deps += [
    dependency('OpenCL', method: 'cmake', modules: ['OpenCL::OpenCL'], required: true)
  ]
endif

public_headers = [
  'src/experiment.hpp'
]

lib1 = shared_library('experiment',
  include_directories: join_paths('.', 'src'),
  sources:  [
    public_headers,
    'src/experiment.cpp'
  ],
  dependencies: [ system_deps, external_deps ],
  cpp_args: [ '/D_INTERFACE_=__declspec(dllexport)' ],
  install: true,
  install_dir: get_option('libdir')
)

install_headers(public_headers, subdir: 'experiment')
install_data(
  sources: [ 'LICENSE' ],
  install_dir: join_paths(get_option('infodir'), meson.project_name())
)

# https://mesonbuild.com/Pkgconfig-module.html
pkg = import('pkgconfig')
pkg.generate(lib1, description: '...')

# https://mesonbuild.com/CMake-module.html
cmake = import('cmake')
cmake.configure_package_config_file(
    name: 'experiment',
    input: 'experiment.cmake.in',
    configuration: configuration_data()
)
cmake.write_basic_package_version_file(name: 'experiment', version: '0.1')

if get_option('tests')
  gtest_dep = dependency('gtest', version: '>=1.14.0', method: 'pkg-config', required: false)
  if gtest_dep.found() == false
    gtest_dep = dependency('GTest', method: 'cmake', modules: ['GTest::gtest'], required: true)
  endif
  benchmark_dep = dependency('benchmark', version: '>=1.8', method: 'pkg-config', required: true)

  exe1 = executable('test-program',
    include_directories: join_paths('.', 'src'),
    sources: [
      public_headers,
      'test/test_vulkan.cpp',
      'test/test_main.cpp'
    ],
    dependencies: [ system_deps, external_deps, gtest_dep ],
    link_with: [ lib1 ],
    install: true,
    install_dir: get_option('bindir')
  )

  exe2 = executable('benchmark-program',
    include_directories: join_paths('.', 'src'),
    sources: [
      public_headers,
      'test/benchmark_main.cpp'
    ],
    dependencies: [ system_deps, external_deps, benchmark_dep ],
    link_with: [ lib1 ],
    install: true,
    install_dir: get_option('bindir')
  )

  if target_machine.system() == 'windows'
    test('test-1', exe1,
      args: [ '--gtest_output=test-1.xml' ],
      # env: env, # customized PATH # https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-search-order
      protocol: 'gtest'
    )
    benchmark('benchmark-1', exe2,
      args: [ '--benchmark_out=benchmark-results.json', '--benchmark_out_format=json' ],
      timeout: 40
    )
  endif
endif
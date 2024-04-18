#
# vcpkg install --triplet x64-windows --x-install-root externals
# $workspace = $(Get-Location)
#
# meson setup --backend vs2022 --vsenv `
#   --prefix "$workspace/install" `
#   --buildtype debug `
#   --pkg-config-path "$workspace/externals/x64-windows/debug/lib/pkgconfig" `
#   --build.pkg-config-path "$workspace/externals/x64-windows/debug/lib/pkgconfig" `
#   --cmake-prefix-path "$workspace/externals/x64-windows" `
#   --build.cmake-prefix-path "$workspace/externals/x64-windows" `
#   "build-x64-windows"
# meson compile -C "build-x64-windows"
# meson install -C "build-x64-windows"
# meson test    -C "build-x64-windows" --verbose
#
# https://mesonbuild.com/Builtin-options.html
# https://mesonbuild.com/Reference-manual.html
# https://mesonbuild.com/Installing.html
# https://mesonbuild.com/Unit-tests.html
# https://mesonbuild.com/Machine-files.html
#
project('experiment', 'cpp',
  meson_version: '>=1.4.0',
  version: '0.1',
  default_options: ['warning_level=3', 'cpp_std=c++20']
)
# message('cross build?: ', meson.is_cross_build())

# https://mesonbuild.com/Reference-manual_returned_env.html
env = environment()
if build_machine.system() == 'windows'
  vcpkg = find_program('vcpkg.exe', required: false)
  # env.prepend('VCPKG_ROOT', ...)
endif

root = meson.current_source_dir()
triplet = get_option('target_triplet')
message('Using target_triplet:', triplet)

externals = root / 'externals' / triplet
message('Using externals:', externals)

compiler = meson.get_compiler('cpp')
if target_machine.system() == 'windows'
  add_global_arguments('/DNOMINMAX', language: 'cpp')
endif

system_dependencies = [
  compiler.find_library('kernel32', required: true),
  dependency('vulkan', version: '>=1.3.0', method: 'auto', required: true)
]

external_dependencies = [
  compiler.find_library('dispatch', dirs: [ externals / 'lib'], required: true),
  dependency('fmt', method: 'cmake', modules: ['fmt::fmt'], required: true),
  dependency('spdlog', method: 'cmake', modules: ['spdlog::spdlog'], required: true),
  dependency('OpenCL', method: 'cmake', modules: ['OpenCL::OpenCL'], required: true)
]

public_headers = [
  'include/experiment.hpp'
]

lib1 = shared_library('experiment',
  include_directories: join_paths('.', 'include'),
  sources:  [
    public_headers,
    'src/experiment.cpp'
  ],
  dependencies: [ system_dependencies, external_dependencies ],
  cpp_args: [ '/D_INTERFACE_=__declspec(dllexport)' ],
  install: true,
  install_dir: get_option('libdir')
)

gtest_dep = dependency('gtest', version: '>=1.14.0', method: 'pkg-config', required: false)
if gtest_dep.found() == false
  gtest_dep = dependency('GTest', method: 'cmake', modules: ['GTest::gtest'], required: true)
endif
benchmark_dep = dependency('benchmark', version: '>=1.8', method: 'pkg-config', required: true)

exe1 = executable('test-program',
  include_directories: join_paths('.', 'include'),
  sources: [
    public_headers,
    'test/test_main.cpp'
  ],
  dependencies: [ system_dependencies, external_dependencies, gtest_dep ],
  link_with: [ lib1 ],
  install: true,
  install_dir: get_option('bindir')
)

exe2 = executable('benchmark-program',
  include_directories: join_paths('.', 'include'),
  sources: [
    public_headers,
    'test/benchmark_main.cpp'
  ],
  dependencies: [ system_dependencies, external_dependencies, benchmark_dep ],
  link_with: [ lib1 ],
  install: true,
  install_dir: get_option('bindir')
)

fs = import('fs')
if target_machine.system() == 'windows'
  test('test-1', exe1,
    args: [ '--gtest_output=test-1.xml' ],
    # env: env, # customized PATH # https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-search-order
    protocol: 'gtest',
  )
  benchmark('benchmark-1', exe2,
    args: [ '--benchmark_out=benchmark-results.json', '--benchmark_out_format=json' ],
    timeout: 40
  )
endif

install_headers(public_headers, subdir: 'experiment')
install_data(
  sources: [ 'LICENSE' ],
  install_dir: join_paths(get_option('infodir'), meson.project_name())
)

pkg = import('pkgconfig') # https://mesonbuild.com/Pkgconfig-module.html
pkg.generate(lib1, description: 'Test Interop between Windows DirectX Graphics Infrastructure (DXGI) and Vulkan SDK')

cmake = import('cmake') # https://mesonbuild.com/CMake-module.html
cmake.configure_package_config_file(
    name: 'experiment',
    input: 'experiment.cmake.in',
    configuration: configuration_data()
)
cmake.write_basic_package_version_file(name: 'experiment', version: '0.1')

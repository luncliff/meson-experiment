#
# vcpkg install --triplet x64-windows --x-install-root externals
# $env:PKG_CONFIG_PATH="$(pwd)/externals/x64-windows/lib/pkgconfig"
#
# meson setup --backend vs2022 --prefix "$(pwd)/install" `
#   --buildtype debug --build.pkg-config-path "$(pwd)/externals/x64-windows/debug/lib/pkgconfig" --build.cmake-prefix-path "$(pwd)/externals/x64-windows" `
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
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++20']
)

vcpkg = find_program('vcpkg.exe', dirs:['C:/vcpkg'], required : true)
cmake = import('cmake') # https://mesonbuild.com/CMake-module.html
pkg = import('pkgconfig') # https://mesonbuild.com/Pkgconfig-module.html
# pkg.generate(libraries : libs1, description: 'Test Interop between Windows DirectX Graphics Infrastructure (DXGI) and Vulkan SDK')
fs = import('fs')

root = meson.current_source_dir()
externals = root / 'externals/x64-windows'
if get_option('buildtype') == 'debug'
  externals = root / 'externals/x64-windows/debug'
endif
message('Using externals: ' + externals)

add_global_arguments('-DNOMINMAX', language : 'cpp')

public_headers = [
  'include/sample.hpp'
]
source_files = [
  'src/loader.cpp'
]

compiler = meson.get_compiler('cpp')

dep1 = compiler.find_library('kernel32', required: true)
dep2 = compiler.find_library('dispatch', dirs:[ externals / 'lib'],required: true)

gtest = dependency('gtest', version: '>=1.14.0', method: 'pkg-config', required: false)
if gtest.found() == false
  gtest = dependency('GTest', method : 'cmake', modules : ['GTest::gtest'], required: true)
endif

exe1 = executable('testsuite',
  include_directories: join_paths('.', 'include'),
  sources :[
    public_headers,
    source_files,
    'test/test_main.cpp'
  ],
  dependencies: [ dep1, dep2, gtest ],
  install : true,
  install_dir: get_option('bindir')
)

# https://mesonbuild.com/Reference-manual_returned_env.html
env = environment()
# if host_machine.system() == 'windows'
#   extra_args = '--use-links'
#   depbins = root / 'externals/x64-windows/debug/bin'
#   env.prepend('PATH', depbins)
# endif
# env.prepend('PATH', 'externals/x64-windows/bin')

copy1 = fs.copyfile(externals / 'bin/gtest.dll', install: true, install_dir: get_option('bindir'))

test('test 1', exe1,
  args: [ '--none' ],
  env: env,
  depends: [ copy1 ],
  protocol: 'gtest',
)

install_headers(public_headers, subdir : 'experiment')
install_data(
  sources: [ 'LICENSE' ],
  install_dir: join_paths(get_option('infodir'), meson.project_name())
)

cmake = import('cmake')
cmake.configure_package_config_file(
    name: 'experiment',
    input: 'experiment.cmake.in',
    configuration: configuration_data()
)

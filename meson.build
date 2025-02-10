project('muuk', ['cpp', 'c'], version: '0.1', default_options: ['cpp_std=c++20'])

json_dep = dependency('nlohmann_json')
log_dep = dependency('spdlog')
argparse_dep = dependency('argparse')

zip_src = files(
    # './modules/kuba---zip-v0.3.3/src/miniz.h',

    'modules/kuba---zip-v0.3.3/src/zip.c',
    # './modules/kuba---zip-v0.3.3/src/zip.h',

) # Add the library files here

zip_dep = declare_dependency(
    sources: zip_src, # Include the C source file

    include_directories: include_directories('modules/kuba---zip-v0.3.3/src'),

)

cracklib_dep = declare_dependency(
    sources: 'modules/evanwporter-cracklib-v0.0.3/src/cracklib.cpp',
    include_directories: include_directories('modules/evanwporter-cracklib-v0.0.3/include'),
)

glob_dep = declare_dependency(
    # sources: 'modules/p-ranav-glob-v0.0.1/source/glob.cpp',

    include_directories: 'modules/p-ranav-glob-v0.3/single_include',
)

executable(
    'muuk',
    [
        'src/main.cpp',
        'src/logger.cpp',
        'src/util.cpp',
        'src/muukfiler.cpp',
        'src/muuk.cpp',
        'src/muukbuilder.cpp',
        'src/muuklockgen.cpp',
        'src/ninjagen.cpp',
    ],
    install: true,
    dependencies: [json_dep, argparse_dep, zip_dep, log_dep, glob_dep, cracklib_dep],
)

# Add tests only if the build type is not 'release'

if get_option('buildtype') != 'release'
    gtest_dep = dependency('gtest', required: true)
    gmock_dep = dependency('gmock', required: true)
    gtest_main_dep = dependency('gtest_main', required: true)

    # Test executable

    test_executable = executable(
        'muuktest',
        [
            'tests/internal/muuk_test.cpp',
            # 'tests/util_test.hpp',

            # 'tests/muuk_test.hpp',

            'src/logger.cpp',
            'src/util.cpp',
            'src/muukfiler.cpp',
            'src/muuk.cpp',
            'src/muukbuilder.cpp',
            'src/muuklockgen.cpp',
            'src/ninjagen.cpp',
        ],
        dependencies: [
            gtest_dep,
            gmock_dep,
            gtest_main_dep,
            json_dep,
            argparse_dep,
            zip_dep,
            log_dep,
            glob_dep,
            cracklib_dep,
        ],
        win_subsystem: 'console',
    )

    # Add the test

    test('muuktest', test_executable)
endif
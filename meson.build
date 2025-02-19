project('muuk', ['cpp', 'c'], version: '0.1', default_options: ['cpp_std=c++20'])

json_dep = dependency('nlohmann_json')
log_dep = dependency('spdlog')
argparse_dep = dependency('argparse')

zip_src = files(
    'deps/zip/src/zip.c',
)

zip_dep = declare_dependency(
    sources: zip_src,
    include_directories: include_directories('deps/zip/src'),
)

cracklib_dep = declare_dependency(
    sources: ['deps/cracklib/src/cracklib.cpp'],
    include_directories: include_directories('deps/cracklib/include'),
)

tomlpp_dep = declare_dependency(
    sources: ['deps/tomlplusplus/src/toml.cpp'], # FIXED: Use square brackets for lists
    include_directories: include_directories('deps/tomlplusplus/include') # FIXED: Closed string properly,
)

glob_dep = declare_dependency(
    include_directories: include_directories('deps/glob/include'),
    sources: ['deps/glob/source/glob.cpp'],
)

executable(
    'muuk',
    [
        'src/main.cpp',
        'src/logger.cpp',
        'src/util.cpp',
        'src/muukfiler.cpp',
        'src/muuker.cpp',
        'src/muukbuilder.cpp',
        'src/muuklockgen.cpp',
        'src/ninjagen.cpp',
        'src/muukinitializer.cpp',
        'src/package_manager.cpp',
        'src/installer.cpp',
    ],
    install: true,
    include_directories: include_directories('include'),
    dependencies: [
        json_dep,
        argparse_dep,
        zip_dep,
        log_dep,
        glob_dep,
        cracklib_dep,
        tomlpp_dep,
    ],
)

# if get_option('buildtype') != 'release'
#     gtest_dep = dependency('gtest', required: true)
#     gmock_dep = dependency('gmock', required: true)
#     gtest_main_dep = dependency('gtest_main', required: true)

#     test_executable = executable(
#         'muuktest',
#         [
#             'tests/internal/muuk_test.cpp',
#             'src/logger.cpp',
#             'src/util.cpp',
#             'src/muukfiler.cpp',
#             'src/muuker.cpp',
#             'src/muukbuilder.cpp',
#             'src/muuklockgen.cpp',
#             'src/ninjagen.cpp',
#         ],
#         dependencies: [
#             gtest_dep,
#             gmock_dep,
#             gtest_main_dep,
#             json_dep,
#             argparse_dep,
#             zip_dep,
#             log_dep,
#             glob_dep,
#             cracklib_dep,
#             tomlpp_dep,
#         ],
#         win_subsystem: 'console',
#     )

#     test('muuktest', test_executable)
# endif
project(
    'tstr.h',
    'c',
    license: 'MIT',
    meson_version: '>=1.3.0',
    default_options: {
        'buildtype': 'release',
        'optimization': '3',
        'c_std': ['c23', 'c11'],
        'warning_level': '3',
        'werror': 'true',
        'b_ndebug': 'if-release',
    },
    version: '0.0.0',
)


inc_dirs = []
src_files = []

subdir('src')

tstr_lib = library(
    'tstr',
    src_files,
    include_directories: inc_dirs,
    install: true,
    version: meson.project_version(),
)

tstr_dep = declare_dependency(
     link_with: tstr_lib,
    include_directories: inc_dirs,
    version: meson.project_version(),
)

meson.override_dependency('tstr.h', tstr_dep)

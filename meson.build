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
    version: '1.0.1',
)


inc_dirs = []
src_files = []

subdir('src')

c_args = []

static_string_modification_behavior = get_option('static_string_modification_behavior')

if static_string_modification_behavior == 'allocate'
   c_args += '-DTSTR_STATIC_STRING_MODIFICATION_BEHAVIOR=0'
elif static_string_modification_behavior == 'error'
    c_args += '-DTSTR_STATIC_STRING_MODIFICATION_BEHAVIOR=1'
else
    error('invalid value for option: \'static_string_modification_behavior\': ' + static_string_modification_behavior)
endif

tstr_lib = library(
    'tstr',
    src_files,
    include_directories: inc_dirs,
    install: true,
    version: meson.project_version(),
    c_args: c_args
)

tstr_dep = declare_dependency(
     link_with: tstr_lib,
    include_directories: inc_dirs,
    version: meson.project_version(),
)

meson.override_dependency('tstr.h', tstr_dep)

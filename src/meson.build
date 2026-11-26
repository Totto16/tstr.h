src_files += files('tstr.c', 'tstr.h')

inc_dirs += include_directories('.')

include_dir = get_option('prefix') / get_option('includedir')

install_headers(
    files('tstr.h'),
    install_dir: include_dir / 'tstr',
    preserve_path: true,
)


cc = meson.get_compiler('c')

stdc_version = cc.get_define('__STDC_VERSION__ ')
stdc_version_int = stdc_version.replace('L', '').to_int()
if stdc_version_int >= 202000
    compile_flags += '-D_TSTR_COMPILE_WITH_NARROWED_ENUMS'
endif

inc_dirs += include_directories('.')

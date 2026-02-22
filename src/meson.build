inc_dirs += include_directories('.')

include_dir = get_option('prefix') / get_option('includedir')

install_headers(
    files('tstr.h'),
    install_dir: include_dir / 'tstr',
    preserve_path: true,
)


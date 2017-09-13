##### project
project('ephoto', 'c',
        version        : '1.5',
        license        : 'BSD 2 clause',
        default_options: [ 'c_std=gnu99' ],
        meson_version  : '>= 0.40.0')
base_url = 'https://www.enlightenment.org/about-'

##### convenience variables for later
proj     = meson.project_name()
ver      = meson.project_version()
cfg      = configuration_data()

##### dependencies
efl_version = '>= 1.19.0'
elm  = dependency('elementary', required: true, version: efl_version)
edje = dependency('edje',       required: true, version: efl_version)
eipc = dependency('ecore-ipc',  required: true, version: efl_version)
##### check for windows dependency
if build_machine.system() == 'windows'
   win = dependency('evil', version: efl_version)
endif

##### dir locations
dir_prefix = get_option('prefix')
dir_bin    = join_paths(dir_prefix, get_option('bindir'))
dir_lib    = join_paths(dir_prefix, get_option('libdir'))
dir_data   = join_paths(dir_prefix, get_option('datadir'))
dir_locale = join_paths(dir_prefix, get_option('localedir'))

##### get C compiler
cc = meson.get_compiler('c')

##### get edje command
edje_cmd = join_paths(edje.get_pkgconfig_variable('prefix'),
                      'bin', 'edje_cc')
##### translations
if get_option('nls') == true
  subdir('po')
endif
##### config.h
cfg.set_quoted('PACKAGE'                 , proj)
cfg.set_quoted('PACKAGE_NAME'            , proj)
cfg.set_quoted('PACKAGE_VERSION'         , ver)
cfg.set_quoted('PACKAGE_STRING'          , proj + ' ' + ver)
cfg.set_quoted('PACKAGE_URL'             , base_url + proj)
cfg.set_quoted('PACKAGE_BIN_DIR'         , dir_bin)
cfg.set_quoted('PACKAGE_LIB_DIR'         , dir_lib)
cfg.set_quoted('PACKAGE_DATA_DIR'        , join_paths(dir_data, proj))
cfg.set_quoted('LOCALEDIR'               , dir_locale)
cfg.set       ('_GNU_SOURCE'             , 1)
cfg.set       ('__EXTENSIONS__'          , 1)
cfg.set       ('_POSIX_PTHREAD_SEMANTICS', 1)
cfg.set       ('_ALL_SOURCE'             , 1)
cfg.set       ('_POSIX_SOURCE'           , 1)
cfg.set       ('_POSIX_1_SOURCE'         , 1)
##### Check for windows
if build_machine.system() == 'windows'
    cfg.set       ('_WIN32'              , 1)
endif
##### Check for arpa/inet and netinet/in.h
if cc.has_header('arpa/inet.h') == true
    cfg.set       ('HAVE_ARPA_INET_H'    , 1)
endif
if cc.has_header('netinet/in.h') == true
    cfg.set       ('HAVE_NETINET_IN_H'   , 1)
endif
configure_file(output: 'config.h', configuration: cfg)

subdir('src')
subdir('data')

inc = include_directories('.', '../..')
deps = [
    elm,
    edje,
    eipc,
    exif,
    depe
]
executable('ephoto', [
    'ephoto.c',
    'ephoto.h',
    'ephoto_bcg.c',
    'ephoto_color.c',
    'ephoto_config.c',
    'ephoto_cropper.c',
    'ephoto_directory_browser.c',
    'ephoto_editor.c',
    'ephoto_file.c',
    'ephoto_filters.c',
    'ephoto_hsv.c',
    'ephoto_ipc.c',
    'ephoto_main.c',
    'ephoto_red_eye.c',
    'ephoto_scale.c',
    'ephoto_single_browser.c',
    'ephoto_slideshow.c',
    'ephoto_thumb_browser.c',
    'ephoto_thumb.c'
  ],
  include_directories: inc,
  dependencies       : deps,
  gui_app            : true,
  install            : true)

executable('ephoto_thumbnail', [
    'ephoto_ipc.c',
    'ephoto_thumbnailer.c',
    'ephoto_thumb.c'
  ],
  include_directories: inc,
  dependencies       : deps,
  install_dir        : join_paths(dir_lib, proj),
  install            : true)

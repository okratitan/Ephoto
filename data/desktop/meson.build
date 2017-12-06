install_data('ephoto.desktop',
             install_dir: join_paths(dir_data, 'applications'))
install_data('ephoto.png',
             install_dir: join_paths(dir_data, 'icons'))
host_os = host_machine.system()
if host_os == 'linux'
        if cc.has_header_symbol('features.h', '__UCLIBC__')
                host_os = 'linux-uclibc'
        elif cc.has_header_symbol('features.h', '__dietlibc__')
                host_os = 'linux-dietlibc'
        else
                host_os = 'linux-gnu'
        endif
endif
if (build_gadget == true)
        release = depe.get_pkgconfig_variable('release')
        module_arch = '@0@-@1@-@2@'.format(host_os, host_machine.cpu_family(), release)
        dir_gadgets = join_paths([dir_lib, 'enlightenment/gadgets', module_arch])
        install_data('slideshow.desktop',
                install_dir: join_paths([dir_gadgets, 'slideshow']))
endif

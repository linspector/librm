simple_pwd_sources = ['simple_pwd.c']
simple_pwd_dep = [rm_dep]

libsimple_pwd = shared_module('simple_pwd',
                        simple_pwd_sources,
                        include_directories : rm_inc,
                        dependencies : simple_pwd_dep,
                        install : true,
                        install_dir : rm_plugins_path + '/simple_pwd/')

custom_target('simple_pwd.plugin',
    output : 'simple_pwd.plugin',
    input : 'simple_pwd.desktop.in',
    command : [msgfmt, '--desktop', '--template', '@INPUT@', '-d', podir, '-o', '@OUTPUT@'],
    install : true,
    install_dir : rm_plugins_path + '/simple_pwd/')

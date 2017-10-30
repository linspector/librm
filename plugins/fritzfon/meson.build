fritzfon_sources = []
fritzfon_sources += 'fritzfon.c'

fritzfon_dep = rm_dep

fritzfon_inc = [rm_inc]

libfritzfon = shared_library('fritzfon',
                        fritzfon_sources,
                        include_directories : fritzfon_inc,
                        dependencies : fritzfon_dep,
                        install : true,
                        install_dir : get_option('prefix') + '/' + get_option('libdir') + '/rm/fritzfon/')

custom_target('fritzfon.plugin',
    output : 'fritzfon.plugin',
    input : 'fritzfon.desktop.in',
    command : [msgfmt, '--desktop', '--template', '@INPUT@','-d', podir, '-o', '@OUTPUT@'],
    install : true,
    install_dir : get_option('prefix') + '/' + get_option('libdir') + '/rm/fritzfon/')

gnome.compile_schemas()
install_data('org.tabos.rm.plugins.fritzfon.gschema.xml',
install_dir : 'share/glib-2.0/schemas')

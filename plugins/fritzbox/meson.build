fritzbox_sources = [
	'callmonitor.c',
	'csv.c',
	'firmware-04-00.c',
	'firmware-04-74.c',
	'firmware-05-50.c',
	'firmware-06-35.c',
	'firmware-common.c',
	'firmware-query.c',
	'firmware-tr64.c',
	'fritzbox.c'
]

libfritzbox = shared_module('fritzbox',
                        fritzbox_sources,
                        include_directories : rm_inc,
                        dependencies : rm_dep,
                        install : true,
                        install_dir : rm_plugins_path + '/fritzbox/')

custom_target('fritzbox.plugin',
    output : 'fritzbox.plugin',
    input : 'fritzbox.desktop.in',
    command : [msgfmt, '--desktop', '--template', '@INPUT@', '-d', podir, '-o', '@OUTPUT@'],
    install : true,
    install_dir : rm_plugins_path + '/fritzbox/')

install_data('org.tabos.rm.plugins.fritzbox.gschema.xml', install_dir : schema_dir)

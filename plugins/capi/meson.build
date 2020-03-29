capi_sources = [
	'capi.c',
	'fax.c',
	'isdn-convert.c',
	'phone.c'
]

capi_dep = []
capi_dep += rm_dep
capi_dep += dependency('sndfile', fallback : ['libsndfile', 'sndfile_dep'])
capi_dep += dependency('capi20', fallback : ['libcapi20', 'capi_shared_dep'])

libcapi = shared_module('capi',
                        capi_sources,
                        include_directories : rm_inc,
                        dependencies : capi_dep,
                        install : true,
                        install_dir : rm_plugins_path + '/capi/')

custom_target('capi.plugin',
    output : 'capi.plugin',
    input : 'capi.desktop.in',
    command : [msgfmt, '--desktop', '--template', '@INPUT@', '-d', podir, '-o', '@OUTPUT@'],
    install : true,
    install_dir : rm_plugins_path + '/capi/')

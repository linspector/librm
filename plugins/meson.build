schema_dir = join_paths(get_option('datadir'), 'glib-2.0', 'schemas')

subdir('areacodes_global')
subdir('capi')
subdir('fritzbox')
subdir('fritzfon')

if host_machine.system().contains('linux')
subdir('gnotification')
endif

subdir('gstreamer')

subdir('reverselookup')

if get_option('enable-secret')
    subdir('secret')
endif

subdir('simple_pwd')

if host_machine.system().contains('windows')
subdir('wincred')
endif

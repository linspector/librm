schema_dir = join_paths(get_option('datadir'), 'glib-2.0', 'schemas')

subdir('areacodes_global')
subdir('capi')
subdir('fritzbox')
subdir('fritzfon')
subdir('gnotification')
subdir('gstreamer')

if get_option('enable-pulseaudio')
	subdir('pulseaudio')
endif

subdir('reverselookup')

if get_option('enable-secret')
    subdir('secret')
endif

subdir('simple_pwd')

# Disabled - needs more love
#if host_machine.system() == 'windows'
#	subdir('wincred')
#endif

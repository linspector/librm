project('rm', 'c', version : '2.1.4', license : 'LGPL-2.1+', default_options : ['prefix=/usr'])

# Set internal values
rm_name = meson.project_name()
rm_version = meson.project_version()

# Extract maj/min/mic versions of rm_versions
version_arr = rm_version.split('.')
rm_version_major = version_arr[0]
rm_version_minor = version_arr[1]
rm_version_micro = version_arr[2]

api_version = '2.0'
soversion = 0

# Set library version
libversion = '@0@.@1@.0'.format(soversion, rm_version_minor.to_int() * 100 + rm_version_micro.to_int())

gnome = import('gnome')
msgfmt = find_program('msgfmt')

os_unix   = false
os_linux  = false
os_win32  = false
os_darwin = false

if host_machine.system().contains('darwin')
    os_darwin = true
elif host_machine.system() == 'windows'
    os_win32 = true
elif host_machine.system().contains('linux')
    os_linux = true
endif

os_unix = not os_win32

# Get c compiler to check for libm
cc = meson.get_compiler('c')

# Check for dependencies
rm_dep = []
rm_dep += cc.find_library('m', required: true)
rm_dep += dependency('glib-2.0')
rm_dep += dependency('gdk-pixbuf-2.0')
rm_dep += dependency('libsoup-2.4')
rm_dep += dependency('speex')
rm_dep += dependency('libxml-2.0')
rm_dep += dependency('libtiff-4')
rm_dep += dependency('spandsp')
rm_dep += dependency('json-glib-1.0')
rm_dep += dependency('gmodule-2.0')
rm_dep += dependency('sndfile')

gupnp10_dep = dependency('gupnp-1.0', required : false)
if gupnp10_dep.found()
  rm_dep += gupnp10_dep
  rm_dep += dependency('gssdp-1.0')
else
  rm_dep += dependency('gupnp-1.2')
  rm_dep += dependency('gssdp-1.2')
endif

# Include GTK integration libs for OS X
if os_darwin
    rm_dep += dependency('gtk+-3.0')
    rm_dep += dependency('gtk-mac-integration-gtk3')
endif

rm_inc = include_directories('.')

if host_machine.system() == 'windows'
    rm_plugins_path = join_paths('lib', rm_name)
else
    rm_plugins_path = join_paths(get_option('prefix'), get_option('libdir'), rm_name)
endif

# Set config data
cdata = configuration_data()
cdata.set('RM_NAME', '"rm"')
cdata.set('RM_VERSION', '"@0@"'.format(rm_version))
cdata.set('RM_PLUGINS', '"' + rm_plugins_path + '"')
cdata.set('RM_GETTEXT_PACKAGE', '"rm"')
cdata.set_quoted('G_LOG_DOMAIN', 'rm')

if os_linux
	cdata.set('APP_LOCALE', '"' + join_paths(get_option('prefix'), get_option('localedir')) + '"')
else
	cdata.set('APP_LOCALE', '"' + get_option('localedir') + '"')
endif

configure_file(
    output : 'rmconfig.h',
    configuration : cdata)

gnome.compile_schemas()

install_data(
    'org.tabos.rm.gschema.xml',
    install_dir : 'share/glib-2.0/schemas')

subdir('po')
subdir('rm')
subdir('plugins')

if get_option('enable-documentation')
    subdir('docs')
endif

# Post install script
if get_option('enable-post-install')
    meson.add_install_script('meson_post_install.sh')
endif

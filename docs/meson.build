version_conf = configuration_data()
version_conf.set('RM_VERSION', rm_version)

configure_file(input: 'version.xml.in', output: 'version.xml', configuration: version_conf)

docpath = join_paths('html')

gnome.gtkdoc('rm',
	main_xml: 'rm-docs.xml',
	src_dir: [
		join_paths(meson.source_root(), 'rm'),
	],
	dependencies: rm_dep,
	scan_args: [
		'--ignore-headers=plugins rmconfig.h'
	],
	mkdb_args: [
		'--name-space=Rm'
	],
	install: true)

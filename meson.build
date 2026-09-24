project(
	'SwayAudioIdleInhibit',
	'c',
	'cpp',
	version: '0.2.0',
	default_options: [
		'c_std=c11',
		'cpp_std=c++11',
		'warning_level=2',
		'werror=true',
	],
)

add_project_arguments(
	[
		'-Wno-unused-parameter',
		'-Wno-unused-result',
		'-Wno-missing-braces',
		'-Wno-format-zero-length',
		'-Wno-missing-field-initializers',
		'-Wundef',
		'-Wvla',
		'-Wlogical-op',
		'-Wmissing-include-dirs',
		'-Wpointer-arith',
		'-Winit-self',
		'-Wimplicit-fallthrough=2',
		'-Wendif-labels',
		'-Wstrict-aliasing=2',
		'-Woverflow',
		'-Walloca',
	],
	language: 'cpp',
)

logind_dep = dependency('lib' + get_option('logind-provider'), required: true)

add_project_arguments('-D', 'HAVE_' + get_option('logind-provider').to_upper(), language: 'cpp')

wayland_protos_dep = dependency('wayland-protocols')
wayland_client_dep = dependency('wayland-client', version: '>=1.14.91')

wayland_scanner = find_program('wayland-scanner')
wayland_scanner_code = generator(
	wayland_scanner,
	output: '@BASENAME@-protocol.c',
	arguments: ['private-code', '@INPUT@', '@OUTPUT@'],
)
wayland_scanner_client = generator(
	wayland_scanner,
	output: '@BASENAME@-client-protocol.h',
	arguments: ['client-header', '@INPUT@', '@OUTPUT@'],
)

idle_inhibit_xml = join_paths(
	wayland_protos_dep.get_variable(pkgconfig: 'pkgdatadir'),
	'unstable/idle-inhibit/idle-inhibit-unstable-v1.xml',
)
idle_inhibit_protos_src = wayland_scanner_code.process(idle_inhibit_xml)
idle_inhibit_protos_headers = wayland_scanner_client.process(idle_inhibit_xml)

lib_idle_inhibit_protos = static_library(
	'idle_inhibit_protos',
	[idle_inhibit_protos_src, idle_inhibit_protos_headers],
	dependencies: [wayland_client_dep],
)
idle_inhibit_protos = declare_dependency(
	link_with: lib_idle_inhibit_protos,
	sources: idle_inhibit_protos_headers,
)

# Systemd service unit
if get_option('logind-provider') == 'systemd'
	dep_systemd = dependency('systemd')
	if dep_systemd.found()
		systemd_service_install_dir = dep_systemd.get_variable(pkgconfig: 'systemduserunitdir')
	else
		systemd_service_install_dir = join_paths(get_option('libdir'), 'systemd', 'user')
	endif
	conf_data = configuration_data()
	conf_data.set('bindir', join_paths(get_option('prefix'), get_option('bindir')))
	configure_file(
		configuration: conf_data,
		input: 'services/systemd/sway-audio-idle-inhibit.service.in',
		output: '@BASENAME@',
		install_dir: systemd_service_install_dir,
	)
endif

executable(
	'sway-audio-idle-inhibit',
	files(
		'./src/data.cpp',
		'./src/idle.cpp',
		'./src/main.cpp',
		'./src/pulse.cpp',
	),
	include_directories: include_directories('include'),
	dependencies: [
		dependency('libpulse', version: '>= 15.0'),
		wayland_client_dep,
		wayland_protos_dep,
		idle_inhibit_protos,
		logind_dep,
	],
	install: true,
)

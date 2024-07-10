project('hello_triangle', 'c',
  version: '1.0',
  default_options: ['warning_level=3'])

egl_dep = dependency('egl', required: true)
glesv2_dep = dependency('glesv2', required: true)
wayland_client_dep = dependency('wayland-client', required: true)
wayland_egl_dep = dependency('wayland-egl', required: true)
wayland_protocols_dep = dependency('wayland-protocols', required: true)

wayland_scanner = find_program('wayland-scanner', required: true)

# Locate the xdg-shell.xml in the sysroot
xdg_shell_xml = wayland_protocols_dep.get_pkgconfig_variable('pkgdatadir') / 'stable' / 'xdg-shell' / 'xdg-shell.xml'

xdg_client_header = custom_target('xdg-shell-client-protocol.h',
  input: xdg_shell_xml,
  output: 'xdg-shell-client-protocol.h',
  command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@']
)

xdg_private_code = custom_target('xdg-shell-protocol.c',
  input: xdg_shell_xml,
  output: 'xdg-shell-protocol.c',
  command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@']
)

executable('hello-triangle', 'hello-triangle.c',
  dependencies: [egl_dep, glesv2_dep, wayland_client_dep, wayland_egl_dep],
  sources: [xdg_client_header, xdg_private_code]
)

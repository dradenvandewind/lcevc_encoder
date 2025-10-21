project('gst-lcevc-enc', 'c',
  version : '1.0.0',
  license : 'LGPL',
  meson_version : '>= 0.53.0',
  default_options : [ 'warning_level=1',
                      'buildtype=debugoptimized' ])

gst_version = '>= 1.18.0'

gst_dep = dependency('gstreamer-1.0', version : gst_version,
  required : true)
gstbase_dep = dependency('gstreamer-base-1.0', version : gst_version,
  required : true)
gstvideo_dep = dependency('gstreamer-video-1.0', version : gst_version,
  required : true)

# Fichiers sources
plugin_sources = [
  'gstlcevcenc.c',
]

# Définitions de compilation
plugin_c_args = [
  '-DHAVE_CONFIG_H',
  '-DGST_USE_UNSTABLE_API',
]

# Dépendances
plugin_deps = [gst_dep, gstbase_dep, gstvideo_dep]

# Construire le plugin
gst_plugins_install_dir = join_paths(get_option('prefix'), get_option('libdir'), 'gstreamer-1.0')

shared_library('gstlcevcenc',
  plugin_sources,
  c_args : plugin_c_args,
  dependencies : plugin_deps,
  install : true,
  install_dir : gst_plugins_install_dir,
)

message('LCEVC Encoder plugin configuration:')
message('  Version: ' + meson.project_version())
message('  Install dir: ' + gst_plugins_install_dir)
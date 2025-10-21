project('gst-lcevc-enc', 'cpp',
  version : '1.0.0',
  license : 'LGPL',
  meson_version : '>=0.48.0',
  default_options : [
    'cpp_std=c++11',
    'buildtype=release',
    'warning_level=1',
    'b_ndebug=if-release'
  ]
)

# Variables du projet
project_version = meson.project_version()
plugin_name = 'lcevcenc'
plugin_install_dir = join_paths(get_option('prefix'), get_option('libdir'), 'gstreamer-1.0')

# Dépendances requises
gst_dep = dependency('gstreamer-1.0', required : true)
gst_base_dep = dependency('gstreamer-base-1.0', required : true)
gst_video_dep = dependency('gstreamer-video-1.0', required : true)
glib_dep = dependency('glib-2.0', required : true)
gobject_dep = dependency('gobject-2.0', required : true)

# Dépéndances optionnelles pour les tests
gst_check_dep = dependency('gstreamer-check-1.0', required : false)

# Configuration de la compilation
cpp = meson.get_compiler('cpp')

# Flags de compilation
compiler_flags = []
cpp_flags = [
  '-DHAVE_CONFIG_H',
  '-D_GNU_SOURCE',
  '-D_FILE_OFFSET_BITS=64',
  '-D_LARGEFILE64_SOURCE',
  '-D__STDC_FORMAT_MACROS',
  '-D__STDC_LIMIT_MACROS',
  '-fvisibility=hidden',
  '-fno-strict-aliasing',
]

# Flags spécifiques au compilateur
if cpp.get_id() == 'gcc' or cpp.get_id() == 'clang'
  compiler_flags += [
    '-Wall',
    '-Wextra',
    '-Wpointer-arith',
    '-Wmissing-declarations',
    '-Wredundant-decls',
    '-Wwrite-strings',
    '-Wformat',
    '-Wformat-nonliteral',
    '-Wformat-security',
    '-Wold-style-definition',
    '-Winit-self',
    '-Wmissing-include-dirs',
    '-Waddress',
    '-Wno-multichar',
    '-Wvla',
    '-Wno-packed',
    '-Wno-unknown-pragmas',
  ]
endif

# Ajouter les flags
add_project_arguments(cpp.get_supported_arguments(compiler_flags), language: 'cpp')
add_project_arguments(cpp_flags, language: 'cpp')

# Inclure les répertoires
includes = include_directories([
  '.',
  #'src',
  #'include'
])

# Configuration
config = configuration_data()
config.set('VERSION', '"@0@"'.format(project_version))
config.set('PACKAGE', '"@0@"'.format(meson.project_name()))
config.set('PACKAGE_NAME', '"@0@"'.format(meson.project_name()))
config.set('PACKAGE_STRING', '"@0@ @1@"'.format(meson.project_name(), project_version))
config.set('PACKAGE_VERSION', '"@0@"'.format(project_version))
config.set('PLUGIN_INSTALL_DIR', '"@0@"'.format(plugin_install_dir))

# Vérifier la disponibilité des fonctions
config.set('HAVE_CPU_X86_64', host_machine.cpu_family() == 'x86_64')

# Vérifier les dépendances optionnelles
config.set('HAVE_GST_CHECK', gst_check_dep.found())

# Chercher la bibliothèque LCEVC

# Si la bibliothèque LCEVC n'est pas trouvée avec pkg-config, essayer manuellement
  # Chercher les headers
  lcevc_inc = include_directories([
    '/usr/include',
    '../../LTM/encoder/include/',
    '../../LTM/util/include/',
    '../../LTM/decoder/include',
    '../../LTM/src/'
  ])
  
  # Chercher la bibliothèque
  
  lcevc_dep = declare_dependency(
    include_directories : lcevc_inc,
    #encies : lcevc_lib
  )

# Fichier de configuration
configure_file(
  output : 'config.h',
  configuration : config
)

# Sources du plugin
plugin_sources = [
  'gstlcevcenc.cpp',
]

# Définitions pour le plugin
plugin_defines = [
  '-DGST_USE_UNSTABLE_API',
  '-DBUILDING_GST_LCEVC_ENC',
  '-DG_LOG_DOMAIN="GST-LCEVC"'
]

# Créer la bibliothèque du plugin
gst_lcevc_enc = shared_library('gst' + plugin_name,
  plugin_sources,
  cpp_args : plugin_defines,
  include_directories : includes,
  dependencies : [
    gst_dep,
    gst_base_dep,
    gst_video_dep,
    glib_dep,
    gobject_dep,
    lcevc_dep
  ],
  install : true,
  install_dir : plugin_install_dir,
)

# Dépendance pour les tests
gst_lcevc_enc_dep = declare_dependency(
  include_directories : includes,
  link_with : gst_lcevc_enc,
  dependencies : [
    gst_dep,
    gst_video_dep,
    lcevc_dep
  ]
)

# Pkg-config file
pkgconfig = import('pkgconfig')
pkgconfig.generate(
  gst_lcevc_enc,
  name : 'gst-lcevc-enc',
  description : 'GStreamer LCEVC Encoder Plugin',
  version : project_version,
  filebase : 'gst-lcevc-enc',
  install_dir : join_paths(get_option('libdir'), 'pkgconfig'),
)


# Installation des headers (si nécessaire)
if get_option('dev')
  install_headers([
      'gstlcevcenc.h',
    ],
    subdir : 'gstreamer-1.0/gst/lcevc'
  )
endif

# Message de résumé
summary({
  'Installation directory': plugin_install_dir,
  'LCEVC library': lcevc_dep.found(),
  'Unit tests': gst_check_dep.found(),
}, section: 'Configuration')

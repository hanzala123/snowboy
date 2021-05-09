plugin_cpp_args = ['-DHAVE_CONFIG_H', '-D_GLIBCXX_USE_CXX11_ABI=0']

cdata = configuration_data()
cdata.set_quoted('PACKAGE_VERSION', gst_version)
cdata.set_quoted('PACKAGE', 'gst-snowboy')
cdata.set_quoted('GST_LICENSE', 'LGPL')
cdata.set_quoted('GST_API_VERSION', api_version)
cdata.set_quoted('GST_PACKAGE_NAME', 'Snowboy gst plugin')
cdata.set_quoted('GST_PACKAGE_ORIGIN', 'https://github.com/liamw9534')
configure_file(output : 'config.h', configuration : cdata)

gstaudio_dep = dependency('gstreamer-audio-1.0',
    fallback: ['gst-plugins-base', 'audio_dep'])

cpp = meson.get_compiler('cpp')
basedir = meson.source_root() + '/../../../../'
libpaths = {'x86_64': basedir + 'lib/ubuntu64',
            'aarch64': basedir + 'lib/aarch64-ubuntu1604',
            'arm': basedir + 'lib/rpi' }
snowboy_det = cpp.find_library('snowboy-detect', dirs: libpaths[host_machine.cpu_family()] )
cblas = cpp.find_library('cblas')

# Snowboy plugin
snowboy_sources = [
  'src/gstsnowboy.cc'
  ]

gstsnowboy = library('gstsnowboy',
  snowboy_sources,
  cpp_args: plugin_cpp_args,
  dependencies : [gst_dep, gstaudio_dep, snowboy_det, cblas],
  install : true,
  install_dir : plugins_install_dir,
  link_args : '-lm'
)

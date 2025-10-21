# Configure
meson setup builddir --prefix=/usr

# Compile
meson compile -C builddir 

# Install
meson install -C builddir

# Test plugin
GST_PLUGIN_PATH=./builddir gst-inspect-1.0 lcevcenc

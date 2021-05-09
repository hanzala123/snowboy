# gst-snowboy plugin

## Introduction

The main motivation for writing this plugin is because gstreamer excels
in handling audio streams and allows complex processing pipelines to be
constructed very easily.  The code examples that come with snowboy mainly
use PortAudio which is cumbersome, not very extensible, and only supports
ALSA audio devices on Linux.

It should also be noted that gstreamer supports PulseAudio.  PulseAudio
has its own echo cancellation module (called `module-echo-cancel`)
which can improve the performance of hot word detection which suffers in the
presence of background noise emitted from speakers on the same machine e.g.,
during media play back.  To this end, instantiating a pipeline that employs
a `pulsesrc` element can give you echo cancellation for free provided you
have loaded `module-echo-cancel` in PulseAudio.

## License

This code is provided under a MIT license [MIT], which basically means "do
with it as you wish, but don't blame me if it doesn't work". You can use
this code for any project as you wish, under any license as you wish. It
is recommended to use the LGPL [LGPL] license for applications and plugins,
given the minefield of patents the multimedia is nowadays.

## Compiling

Configure and build as such:

    meson builddir
    ninja -C builddir

See <https://mesonbuild.com/Quick-guide.html> on how to install the Meson
build system and ninja.

Once the plugin is built you can either install it system-wide with `sudo ninja
-C builddir install` (however, this will by default go into the `/usr/local`
prefix where it won't be picked up by a `GStreamer` installed from packages, so
you would need to set the `GST_PLUGIN_PATH` environment variable to include or
point to `/usr/local/lib/gstreamer-1.0/` for your plugin to be found by a
from-package `GStreamer`).

Alternatively, you will find your plugin binary in `builddir/gst-plugins/src/`
as `libgstplugin.so` or similar (the extension may vary), so you can also set
the `GST_PLUGIN_PATH` environment variable to the `builddir/gst-plugins/src/`
directory (best to specify an absolute path though).

You can also check if it has been built correctly with:

    gst-inspect-1.0 builddir/gst-plugins/src/libgstsnowboy.so

[MIT]: http://www.opensource.org/licenses/mit-license.php or COPYING.MIT
[LGPL]: http://www.opensource.org/licenses/lgpl-license.php or COPYING.LIB
[Licensing]: https://gstreamer.freedesktop.org/documentation/application-development/appendix/licensing.html

## Usage

The Snowboy hotword detector gst plugin uses the snowboy detector library
and links results to the `hotword-detect` signal which should be monitored
from your application to determine when a hotword has been found.

There is an example python3 application that shows how a simple pipeline can be
built and the `hotword-detect` signal monitored under `examples/GST`.  The
bulk of the example is just 4 lines of python code:

```
pipeline = Gst.parse_launch('pulsesrc ! snowboy name=sb models={} sensitivity={} ! fakesink'.format(sys.argv[1], sys.argv[2]))
sb = pipeline.get_by_name('sb')
sb.connect('hotword-detect', lambda el, index: print('{} Hotword detected (model={})'.format(el, index)))
pipeline.set_state(Gst.State.PLAYING)
```

Much like the detector library, the plugin allows multiple models to be used at
the same time using the `models` property.  It should be a comma-separated list
of model files e.g., models=file1.umdl,file2.umdl.  The common resource file
should also be passed in using the `resource` property.

The `hotword-detect` signal will provide an integer which is the model index
that was detected as defined in `model` e.g., 0=>file1.umdl, 1=>file2.umdl etc.

This plugin is designed to work in a larger pipeline structure that could be
used to perform additional audio processing downstream.  For this reason the data
on the sinkpad is also passed-through unmodified to the srcpad.  If you don't need
this then just terminate with a `fakesink` element.

The plugin only works with single channel audio, S16 @ 16KHz as reflected by
the fixed caps.  The caps negotiation normally gets magically taken care of if you
use a `pulsesrc` element for your audio.  If your are using a `filesrc` instead then
you could insert `audioconvert` before `snowboy` to get the correct input format.  If
your data is encoded (e.g., MP3) then try to use `decodebin`.

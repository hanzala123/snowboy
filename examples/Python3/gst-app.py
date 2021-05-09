'''
You must first compile the snowboy gst plugin under examples/C++/gstreamer to use this and the
builddir/gst-plugin to your GST_PLUGIN_PATH env
'''
import sys
import os

from gi import require_version
require_version('Gst', '1.0')
from gi.repository import Gst, GObject

Gst.init(None)

def ping():
  Gst.parse_launch('playbin uri=file://{}/resources/dong.wav'.format(os.getcwd())).set_state(Gst.State.PLAYING)

pipeline = Gst.parse_launch('pulsesrc ! snowboy name=sb models={} sensitivity={} ! fakesink'.format(sys.argv[1], sys.argv[2]))
sb = pipeline.get_by_name('sb')
sb.connect('hotword-detect', lambda el, index: [ ping(), print('{} Hotword detected (model={})'.format(el, index))])
pipeline.set_state(Gst.State.PLAYING)

loop = GObject.MainLoop()
loop.run()

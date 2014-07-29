imagineKatana
Peter Pearson
-------------

Integration of my Imagine renderer into Katana 1.5/1.6.

As Imagine doesn't have a proper API, it's a slightly hacky situation of just hosting Imagine's infrastructure within the Katana Render plugin.

Supports Preview Renders, Disk Renders to EXR, some of Imagine's integrated materials to a limited extent (no nodal shading connections), most light types, and polymesh and subdmesh geometry (the latter with proper subdivision), and instanceSource type instancing.

HDR, TIFF and EXR images can be read (both tiled and scanline for the latter two).

Requires Katana plugins_api directory for building Katana API lib, and Imagine's main src/ subdirectory.




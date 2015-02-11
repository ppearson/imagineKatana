imagineKatana
Peter Pearson
-------------

Integration of my Imagine renderer into Katana 1.5/1.6.

As Imagine doesn't have a proper API, it's a slightly hacky situation of just hosting Imagine's infrastructure within the Katana Render plugin.

* Supports Preview Renders and Live Renders to Katana's Monitor and Disk Renders to EXR file
* Most of Imagine's integrated materials as uber-shaders (but no nodal shading connections yet)
* Most light types are exposed (but without viewerManipulators, so using them in Katana is a bit messy)
* polymesh and subdmesh geometry (the latter with proper subdivision), with options for quantising (compressing) attributes
  for memory-efficiency
* instanceSource type instancing
* 2 time sample (single motion segment) motion blur - both transform and deformation of meshes


HDR, TIFF and EXR images can be read (both tiled and scanline for the latter two).

Requires Katana plugins_api directory for building Katana API lib, and Imagine's main src/ subdirectory.




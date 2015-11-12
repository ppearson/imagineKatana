## imagineKatana - Imagine Renderer plugin for Katana
Peter Pearson

This is an integration of my [Imagine Renderer][blog] into Katana 1.x and 2.x (controlled with #ifdefs for the moment).

As Imagine doesn't have a proper API yet, it's a slightly hacky situation of just hosting Imagine's infrastructure within the Katana Renderer plugin.

* Supports both Preview and Live interactive rendering to Katana's monitor, and Disk Renders to EXR files.
* Most of Imagine's integrated materials as uber-shaders (no nodal shading connections yet)
* Most light types are exposed
* Polymesh and Subdmesh geometry (with proper subdivision in render), with options for quantising (compressing) attributes
* instanceSource type instancing
* 2 time sample (single motion segment) motion blur - both transform and deformation of meshes
* HDR, TIFF and EXR image reading (both tiled and scanline for the latter two), although pre-mipmapped EXR is highly recommended for using texture caching


Requires Katana plugins_api directory for building Katana API lib, and Imagine's main src/ directory.

[Video in action against Arnold 4.1][video].


[video]: https://vimeo.com/110120338/
[blog]: http://imagine-rt.blogspot.co.uk/
# Filtered raymarch renderer

This is a standalone executable that filters a raymarched image with neural cellular automata exported from the separate filter training Jupyter notebook also contained in this repo.

To build it you need Visual Studio 2017. More recent versions may work but haven't been tested.
The code is a hacked version of the [Blossom framework](https://github.com/lunasorcery/Blossom).

### Keys

- **Right Control:** reload shaders and restart
- **Enter:** toggle between accumulation and frozen first frame (buggy)
- **Right Shift + Right Control**: Dump first frame to disk as raw data and PNG.

### Other stuff

The "Capture" configuration does not work.

## Credits

* Original micro NCA code by Alexander Mordvintsev [on ShaderToy](https://www.shadertoy.com/view/slGGzD).
* [Shader Minifier](https://github.com/laurentlb/Shader_Minifier) by LLB.
* [Crinkler](https://github.com/runestubbe/Crinkler) by Blueberry & Mentor.

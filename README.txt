Leverages and expands upon some of the utilities/modules within my `graphics-setup` repo, such as: a config/input, file io, and time systems.
- This is a good example of how the setup repo is a useful tool as it encompasses some common systems that can be plucked out and put into another project.
Expanded `c-lib` library and math library.

features:
- basic vertex and fragment shaders
- barycentric interpolation for vertex attributes in the rasterizer
- rotation and translation matrices in the math library
- model vs viewport matrices for transforms
- perspective transform
- cube and ramp meshes with normals and uv data per vertex
- texture binding to materials linked with meshes
- diffuse lighting system with gouraud shading w/ambient lighting
- lit and unlit shaders for render objects that don't respect the general lighting specs
- dynamic light source and light source visual in 3d space

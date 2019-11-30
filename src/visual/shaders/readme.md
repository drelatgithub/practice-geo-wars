# The shader directory

This directory contains the source code for the shader, and is not ready to be used by the main program. In the build system, the shaders should be first be compiled into `.spv` format, and then be converted to a corresponding `.hpp` file containing the byte data of the compiled shader.

The build system must satisfy the requirements for the final header file corresponding to the original shader file:
- The filename name must be `<original-shader-name>.spv.hpp`.
- Must contain the definition of `constexpr unsigned char value[]` variable.

The first step is to compile the shader. For example, if I have a shader named `shader.vert`, I need to compile the shader (e.g. using `glslc` provided in Vulkan SDK) and name the output `shader.vert.spv`.

The next step is to convert the binary `.spv` file to a header file by containing the binary data. `Bin2c` is provided to help with this.

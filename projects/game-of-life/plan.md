The main purpose is to use Conway's Game of Life to explore GPU programming, especially cellular automata.

# Plan

1. Create a Rust application with a `winit` window and event loop.
2. Initialize `wgpu` and clear the window surface.
3. Render a static grid of cells.
4. Implement a small CPU simulation as a correctness reference.
5. Implement and test the Game-of-Life compute kernel with CubeCL.
6. Run CubeCL and rendering on the same `wgpu` device and queue.
7. Render the current GPU simulation state.
8. Add simulation controls and performance experiments.

# Design decision

`wgpu` owns the GPU instance, adapter, device, and queue; CubeCL is initialized from that existing setup. The simulation uses two `u32` buffers—current and next—which are swapped after each generation. The world wraps at its edges. CubeCL and its compatible `wgpu` version will be pinned because CubeCL's API is still evolving. Direct zero-copy buffer sharing is optional and can be explored after the basic integration works.

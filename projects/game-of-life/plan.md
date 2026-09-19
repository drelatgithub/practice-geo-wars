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

`wgpu` owns the GPU instance, adapter, device, and queue; CubeCL is initialized from that existing setup. Two GPU-resident `u32` buffers—current and next—are the authoritative grid state and swap roles after each generation; the CPU keeps only metadata and timing. Simulation and rendering run at independent rates: the CPU schedules bounded batches of compute steps, the shared GPU queue orders them before rendering, and each frame displays only the latest state using `AutoVsync`. The world wraps at its edges. CubeCL and its compatible `wgpu` version will be pinned because CubeCL's API is still evolving. Direct zero-copy buffer sharing is optional; a GPU-to-GPU copy is an acceptable first integration.

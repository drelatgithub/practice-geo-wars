use cubecl::prelude::*;

pub(crate) type CubeRuntime = cubecl::wgpu::WgpuRuntime;
type CubeClient = ComputeClient<CubeRuntime>;

const CUBE_DIM: u32 = 256;

#[cube(launch)]
fn game_of_life_kernel(current: &Array<u32>, next: &mut Array<u32>, width: u32, height: u32) {
    let position = ABSOLUTE_POS as u32;

    if position < width * height {
        let x = position % width;
        let y = position / width;

        let left = (x + width - 1) % width;
        let right = (x + 1) % width;
        let above = (y + height - 1) % height;
        let below = (y + 1) % height;

        let mut neighbors = 0u32;
        neighbors += current[(above * width + left) as usize];
        neighbors += current[(above * width + x) as usize];
        neighbors += current[(above * width + right) as usize];
        neighbors += current[(y * width + left) as usize];
        neighbors += current[(y * width + right) as usize];
        neighbors += current[(below * width + left) as usize];
        neighbors += current[(below * width + x) as usize];
        neighbors += current[(below * width + right) as usize];

        let alive = current[position as usize] != 0;
        let mut next_state = 0u32;
        if neighbors == 3 || (alive && neighbors == 2) {
            next_state = 1;
        }
        next[position as usize] = next_state;
    }
}

pub(crate) struct Simulation {
    client: CubeClient,
    buffers: [cubecl::server::Handle; 2],
    current: usize,
    width: usize,
    height: usize,
}

impl Simulation {
    pub(crate) fn new(
        instance: &wgpu::Instance,
        adapter: &wgpu::Adapter,
        device: &wgpu::Device,
        queue: &wgpu::Queue,
        initial_cells: &[u32],
        width: u32,
        height: u32,
    ) -> Self {
        let cube_device = cubecl::wgpu::init_device(
            cubecl::wgpu::WgpuSetup {
                instance: instance.clone(),
                adapter: adapter.clone(),
                device: device.clone(),
                queue: queue.clone(),
                backend: adapter.get_info().backend,
            },
            Default::default(),
        );
        let client = CubeRuntime::client(&cube_device);

        verify_game_of_life_kernel(&client);

        let width = width as usize;
        let height = height as usize;
        assert_eq!(initial_cells.len(), width * height);
        let current = client.create_from_slice(u32::as_bytes(initial_cells));
        let next = client.empty(std::mem::size_of_val(initial_cells));

        Self {
            client,
            buffers: [current, next],
            current: 0,
            width,
            height,
        }
    }

    pub(crate) fn advance_and_copy(
        &mut self,
        steps: u32,
        encoder: &mut wgpu::CommandEncoder,
        destination: &wgpu::Buffer,
    ) {
        for _ in 0..steps {
            let next = 1 - self.current;
            launch_generation(
                &self.client,
                &self.buffers[self.current],
                &self.buffers[next],
                self.width,
                self.height,
            );
            self.current = next;
        }

        let current = self
            .client
            .get_resource(self.buffers[self.current].clone())
            .expect("failed to access the current CubeCL grid buffer");
        let current = current.resource();
        let byte_len = (self.width * self.height * std::mem::size_of::<u32>()) as u64;
        assert!(current.size >= byte_len);
        assert!(destination.size() >= byte_len);

        encoder.copy_buffer_to_buffer(&current.buffer, current.offset, destination, 0, byte_len);
    }
}

fn launch_generation(
    client: &CubeClient,
    current: &cubecl::server::Handle,
    next: &cubecl::server::Handle,
    width: usize,
    height: usize,
) {
    let cell_count = width * height;
    assert!(cell_count > 0);

    // SAFETY: Both handles are allocated for `cell_count` u32 values by the caller below.
    let current = unsafe { ArrayArg::from_raw_parts(current.clone(), cell_count) };
    let next = unsafe { ArrayArg::from_raw_parts(next.clone(), cell_count) };
    let cube_count = cell_count.div_ceil(CUBE_DIM as usize) as u32;

    game_of_life_kernel::launch::<CubeRuntime>(
        client,
        CubeCount::Static(cube_count, 1, 1),
        CubeDim::new_1d(CUBE_DIM),
        current,
        next,
        width as u32,
        height as u32,
    );
}

fn verify_game_of_life_kernel(client: &CubeClient) {
    const WIDTH: usize = 5;
    const HEIGHT: usize = 5;

    // A horizontal blinker crosses the wrapping left/right boundary.
    let current = [
        0_u32, 0, 0, 0, 0, //
        0, 0, 0, 0, 0, //
        1, 1, 0, 0, 1, //
        0, 0, 0, 0, 0, //
        0, 0, 0, 0, 0,
    ];
    let expected = [
        0_u32, 0, 0, 0, 0, //
        1, 0, 0, 0, 0, //
        1, 0, 0, 0, 0, //
        1, 0, 0, 0, 0, //
        0, 0, 0, 0, 0,
    ];

    let current = client.create_from_slice(u32::as_bytes(&current));
    let next = client.empty(std::mem::size_of_val(&expected));
    launch_generation(client, &current, &next, WIDTH, HEIGHT);

    let bytes = client.read_one_unchecked(next);
    assert_eq!(u32::from_bytes(&bytes), expected);
}

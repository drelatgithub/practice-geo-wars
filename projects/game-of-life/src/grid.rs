use wgpu::util::DeviceExt;

const GRID_WIDTH: u32 = 128;
const GRID_HEIGHT: u32 = 128;
const GLIDER: &[(u32, u32)] = &[(1, 0), (2, 1), (0, 2), (1, 2), (2, 2)];

pub(crate) struct CellGrid {
    width: u32,
    height: u32,
    buffers: [wgpu::Buffer; 2],
    current: usize,
}

impl CellGrid {
    pub(crate) fn new(device: &wgpu::Device) -> Self {
        let mut current_cells = vec![0_u32; (GRID_WIDTH * GRID_HEIGHT) as usize];
        let next_cells = vec![0_u32; current_cells.len()];
        let origin = (GRID_WIDTH / 2 - 1, GRID_HEIGHT / 2 - 1);

        for &(offset_x, offset_y) in GLIDER {
            let x = origin.0 + offset_x;
            let y = origin.1 + offset_y;
            let index = (y * GRID_WIDTH + x) as usize;
            current_cells[index] = 1;
        }

        let usage = wgpu::BufferUsages::STORAGE
            | wgpu::BufferUsages::COPY_SRC
            | wgpu::BufferUsages::COPY_DST;
        let current = device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
            label: Some("current cell grid"),
            contents: bytemuck::cast_slice(&current_cells),
            usage,
        });
        let next = device.create_buffer_init(&wgpu::util::BufferInitDescriptor {
            label: Some("next cell grid"),
            contents: bytemuck::cast_slice(&next_cells),
            usage,
        });

        let grid = Self {
            width: GRID_WIDTH,
            height: GRID_HEIGHT,
            buffers: [current, next],
            current: 0,
        };
        grid.validate();
        grid
    }

    fn validate(&self) {
        let expected_buffer_size =
            u64::from(self.width) * u64::from(self.height) * std::mem::size_of::<u32>() as u64;
        for buffer in &self.buffers {
            assert_eq!(buffer.size(), expected_buffer_size);
        }
    }

    pub(crate) fn width(&self) -> u32 {
        self.width
    }

    pub(crate) fn height(&self) -> u32 {
        self.height
    }

    pub(crate) fn buffers(&self) -> &[wgpu::Buffer; 2] {
        &self.buffers
    }

    pub(crate) fn current_index(&self) -> usize {
        self.current
    }
}

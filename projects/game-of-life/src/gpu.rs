use std::{
    sync::Arc,
    time::{Duration, Instant},
};

use winit::{
    dpi::PhysicalSize,
    window::{Window, WindowId},
};

use crate::{
    grid::{CellGrid, GRID_HEIGHT, GRID_WIDTH, initial_cells},
    kernel::Simulation,
    renderer::GridRenderer,
};

const SIMULATION_INTERVAL: Duration = Duration::from_millis(10);
const FRAME_INTERVAL: Duration = Duration::from_nanos(1_000_000_000 / 60);
const MAX_CATCH_UP_STEPS: u32 = 8;

pub(crate) struct GpuState {
    window: Arc<Window>,
    instance: wgpu::Instance,
    _adapter: wgpu::Adapter,
    device: wgpu::Device,
    queue: wgpu::Queue,
    simulation: Simulation,
    surface: wgpu::Surface<'static>,
    surface_config: wgpu::SurfaceConfiguration,
    renderer: GridRenderer,
    grid: CellGrid,
    next_simulation: Instant,
    next_frame: Instant,
}

impl GpuState {
    pub(crate) async fn new(window: Arc<Window>) -> Self {
        let instance = wgpu::Instance::default();
        let surface = instance
            .create_surface(window.clone())
            .expect("failed to create a wgpu surface");

        let adapter = instance
            .request_adapter(&wgpu::RequestAdapterOptions {
                power_preference: wgpu::PowerPreference::HighPerformance,
                compatible_surface: Some(&surface),
                force_fallback_adapter: false,
            })
            .await
            .expect("failed to find a compatible GPU adapter");

        let (device, queue) = adapter
            .request_device(&wgpu::DeviceDescriptor::default())
            .await
            .expect("failed to create a wgpu device");
        let initial_cells = initial_cells();
        let grid = CellGrid::new(&device, &initial_cells);
        let simulation = Simulation::new(
            &instance,
            &adapter,
            &device,
            &queue,
            &initial_cells,
            GRID_WIDTH,
            GRID_HEIGHT,
        );

        let size = window.inner_size();
        let capabilities = surface.get_capabilities(&adapter);
        let surface_format = capabilities
            .formats
            .iter()
            .copied()
            .find(wgpu::TextureFormat::is_srgb)
            .unwrap_or(capabilities.formats[0]);

        let surface_config = wgpu::SurfaceConfiguration {
            usage: wgpu::TextureUsages::RENDER_ATTACHMENT,
            format: surface_format,
            width: size.width,
            height: size.height,
            present_mode: wgpu::PresentMode::AutoVsync,
            desired_maximum_frame_latency: 2,
            alpha_mode: capabilities.alpha_modes[0],
            view_formats: vec![],
        };
        let renderer = GridRenderer::new(&device, surface_format, &grid);
        let now = Instant::now();

        let state = Self {
            window,
            instance,
            _adapter: adapter,
            device,
            queue,
            simulation,
            surface,
            surface_config,
            renderer,
            grid,
            next_simulation: now + SIMULATION_INTERVAL,
            next_frame: now + FRAME_INTERVAL,
        };

        state.configure_surface();
        state
    }

    pub(crate) fn window_id(&self) -> WindowId {
        self.window.id()
    }

    pub(crate) fn request_redraw(&self) {
        self.window.request_redraw();
    }

    pub(crate) fn update(&mut self) -> Instant {
        let now = Instant::now();
        let mut steps = 0;

        while now >= self.next_simulation && steps < MAX_CATCH_UP_STEPS {
            self.next_simulation += SIMULATION_INTERVAL;
            steps += 1;
        }
        if now >= self.next_simulation {
            self.next_simulation = now + SIMULATION_INTERVAL;
        }
        if steps > 0 {
            self.advance_simulation(steps);
        }

        if now >= self.next_frame {
            self.window.request_redraw();
            self.next_frame += FRAME_INTERVAL;
            if now >= self.next_frame {
                self.next_frame = now + FRAME_INTERVAL;
            }
        }

        self.next_simulation.min(self.next_frame)
    }

    fn advance_simulation(&mut self, steps: u32) {
        let mut encoder = self
            .device
            .create_command_encoder(&wgpu::CommandEncoderDescriptor {
                label: Some("simulation copy encoder"),
            });
        self.simulation
            .advance_and_copy(steps, &mut encoder, self.grid.current_buffer());
        self.queue.submit([encoder.finish()]);
    }

    fn configure_surface(&self) {
        if self.surface_config.width > 0 && self.surface_config.height > 0 {
            self.surface.configure(&self.device, &self.surface_config);
        }
    }

    pub(crate) fn resize(&mut self, new_size: PhysicalSize<u32>) {
        if new_size.width == 0 || new_size.height == 0 {
            return;
        }

        self.surface_config.width = new_size.width;
        self.surface_config.height = new_size.height;
        self.configure_surface();
    }

    pub(crate) fn render(&mut self) {
        if self.surface_config.width == 0 || self.surface_config.height == 0 {
            return;
        }

        let surface_texture = match self.surface.get_current_texture() {
            wgpu::CurrentSurfaceTexture::Success(texture) => texture,
            wgpu::CurrentSurfaceTexture::Occluded | wgpu::CurrentSurfaceTexture::Timeout => return,
            wgpu::CurrentSurfaceTexture::Suboptimal(texture) => {
                drop(texture);
                self.configure_surface();
                self.window.request_redraw();
                return;
            }
            wgpu::CurrentSurfaceTexture::Outdated => {
                self.configure_surface();
                self.window.request_redraw();
                return;
            }
            wgpu::CurrentSurfaceTexture::Lost => {
                self.surface = self
                    .instance
                    .create_surface(self.window.clone())
                    .expect("failed to recreate the wgpu surface");
                self.configure_surface();
                self.window.request_redraw();
                return;
            }
            wgpu::CurrentSurfaceTexture::Validation => {
                unreachable!("wgpu validation errors are reported as panics")
            }
        };

        let view = surface_texture
            .texture
            .create_view(&wgpu::TextureViewDescriptor::default());
        let mut encoder = self
            .device
            .create_command_encoder(&wgpu::CommandEncoderDescriptor {
                label: Some("clear screen encoder"),
            });

        self.renderer.encode(
            &mut encoder,
            &view,
            self.surface_config.width,
            self.surface_config.height,
            self.grid.current_index(),
        );

        self.queue.submit([encoder.finish()]);
        self.window.pre_present_notify();
        surface_texture.present();
    }
}

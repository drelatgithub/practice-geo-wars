use winit::{event::WindowEvent, window::Window};

use crate::patterns::{PATTERNS, Pattern};

/// What a mouse press on the board does.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) enum MouseMode {
    /// Keep the cell under the cursor alive while the button is held.
    Birth,
    /// Print the selected pattern centered on the cursor, once per press.
    Stamp,
}

/// The egui control panel, drawn on top of the grid.
pub(crate) struct Ui {
    context: egui::Context,
    state: egui_winit::State,
    renderer: egui_wgpu::Renderer,
    mouse_mode: MouseMode,
    /// Index into `PATTERNS` used by stamp mode.
    pattern: usize,
}

impl Ui {
    pub(crate) fn new(
        window: &Window,
        device: &wgpu::Device,
        surface_format: wgpu::TextureFormat,
    ) -> Self {
        let context = egui::Context::default();
        let state = egui_winit::State::new(
            context.clone(),
            egui::ViewportId::ROOT,
            window,
            Some(window.scale_factor() as f32),
            None,
            Some(device.limits().max_texture_dimension_2d as usize),
        );
        let renderer = egui_wgpu::Renderer::new(
            device,
            surface_format,
            egui_wgpu::RendererOptions::default(),
        );

        Self {
            context,
            state,
            renderer,
            mouse_mode: MouseMode::Birth,
            pattern: 0,
        }
    }

    pub(crate) fn mouse_mode(&self) -> MouseMode {
        self.mouse_mode
    }

    pub(crate) fn pattern(&self) -> &'static Pattern {
        &PATTERNS[self.pattern]
    }

    /// Passes an event to egui. Returns `true` when egui used it, so the board should not.
    pub(crate) fn on_window_event(&mut self, window: &Window, event: &WindowEvent) -> bool {
        self.state.on_window_event(window, event).consumed
    }

    pub(crate) fn is_pointer_over_ui(&self) -> bool {
        self.context.is_pointer_over_egui()
    }

    /// Runs one egui frame and encodes it on top of `target`. The returned command buffers
    /// must be submitted before `encoder`.
    #[allow(clippy::too_many_arguments)]
    pub(crate) fn encode(
        &mut self,
        window: &Window,
        device: &wgpu::Device,
        queue: &wgpu::Queue,
        encoder: &mut wgpu::CommandEncoder,
        target: &wgpu::TextureView,
        size_in_pixels: [u32; 2],
        paused: &mut bool,
    ) -> Vec<wgpu::CommandBuffer> {
        let raw_input = self.state.take_egui_input(window);
        let mouse_mode = &mut self.mouse_mode;
        let pattern = &mut self.pattern;
        let output = self.context.run_ui(raw_input, |ui| {
            egui::Window::new("Controls")
                .resizable(false)
                .show(ui.ctx(), |ui| {
                    ui.label("Mouse mode");
                    ui.radio_value(mouse_mode, MouseMode::Birth, "Birth");
                    ui.radio_value(mouse_mode, MouseMode::Stamp, "Stamp");
                    if *mouse_mode == MouseMode::Stamp {
                        egui::ComboBox::from_label("Pattern")
                            .selected_text(PATTERNS[*pattern].name)
                            .show_ui(ui, |ui| {
                                for (index, candidate) in PATTERNS.iter().enumerate() {
                                    ui.selectable_value(&mut *pattern, index, candidate.name);
                                }
                            });
                    }
                    ui.separator();
                    ui.checkbox(paused, "Paused (Space)");
                });
        });
        self.state
            .handle_platform_output(window, output.platform_output);

        let screen = egui_wgpu::ScreenDescriptor {
            size_in_pixels,
            pixels_per_point: output.pixels_per_point,
        };
        let paint_jobs = self
            .context
            .tessellate(output.shapes, output.pixels_per_point);

        for (id, delta) in &output.textures_delta.set {
            self.renderer.update_texture(device, queue, *id, delta);
        }
        let command_buffers =
            self.renderer
                .update_buffers(device, queue, encoder, &paint_jobs, &screen);

        let mut render_pass = encoder
            .begin_render_pass(&wgpu::RenderPassDescriptor {
                label: Some("ui render pass"),
                color_attachments: &[Some(wgpu::RenderPassColorAttachment {
                    view: target,
                    depth_slice: None,
                    resolve_target: None,
                    ops: wgpu::Operations {
                        load: wgpu::LoadOp::Load,
                        store: wgpu::StoreOp::Store,
                    },
                })],
                depth_stencil_attachment: None,
                timestamp_writes: None,
                occlusion_query_set: None,
                multiview_mask: None,
            })
            .forget_lifetime();
        self.renderer.render(&mut render_pass, &paint_jobs, &screen);
        drop(render_pass);

        for id in &output.textures_delta.free {
            self.renderer.free_texture(id);
        }

        command_buffers
    }
}

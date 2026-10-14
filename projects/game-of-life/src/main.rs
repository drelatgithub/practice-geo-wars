use std::{error::Error, sync::Arc};

use gpu::GpuState;
use winit::{
    application::ApplicationHandler,
    event::{MouseButton, WindowEvent},
    event_loop::{ActiveEventLoop, ControlFlow, EventLoop},
    keyboard::{Key, NamedKey},
    window::{Window, WindowId},
};

mod gpu;
mod grid;
mod kernel;
mod patterns;
mod renderer;
mod ui;

#[derive(Default)]
struct App {
    gpu: Option<GpuState>,
}

impl ApplicationHandler for App {
    fn resumed(&mut self, event_loop: &ActiveEventLoop) {
        if self.gpu.is_some() {
            return;
        }

        let attributes = Window::default_attributes().with_title("Game of Life");
        let window = Arc::new(
            event_loop
                .create_window(attributes)
                .expect("failed to create a window"),
        );

        self.gpu = Some(pollster::block_on(GpuState::new(window.clone())));
        window.request_redraw();
    }

    fn window_event(
        &mut self,
        event_loop: &ActiveEventLoop,
        window_id: WindowId,
        event: WindowEvent,
    ) {
        let Some(gpu) = &mut self.gpu else {
            return;
        };

        if gpu.window_id() != window_id {
            return;
        }

        let ui_consumed = gpu.ui_event(&event);

        match event {
            WindowEvent::CloseRequested => event_loop.exit(),
            WindowEvent::Resized(new_size) => {
                gpu.resize(new_size);
                gpu.request_redraw();
            }
            WindowEvent::RedrawRequested => gpu.render(),
            WindowEvent::CursorMoved { position, .. } => gpu.cursor_moved(position),
            WindowEvent::CursorLeft { .. } => gpu.cursor_left(),
            // A press on the UI must not start drawing, but a release always stops it.
            WindowEvent::MouseInput {
                state,
                button: MouseButton::Left,
                ..
            } if !(ui_consumed && state.is_pressed()) => gpu.set_mouse_down(state.is_pressed()),
            WindowEvent::KeyboardInput { event, .. }
                if !ui_consumed
                    && event.state.is_pressed()
                    && !event.repeat
                    && event.logical_key == Key::Named(NamedKey::Space) =>
            {
                gpu.toggle_pause()
            }
            _ => {}
        }
    }

    fn about_to_wait(&mut self, event_loop: &ActiveEventLoop) {
        let Some(gpu) = &mut self.gpu else {
            return;
        };

        event_loop.set_control_flow(ControlFlow::WaitUntil(gpu.update()));
    }
}

fn main() -> Result<(), Box<dyn Error>> {
    let event_loop = EventLoop::new()?;
    event_loop.set_control_flow(ControlFlow::Wait);

    let mut app = App::default();
    event_loop.run_app(&mut app)?;

    Ok(())
}

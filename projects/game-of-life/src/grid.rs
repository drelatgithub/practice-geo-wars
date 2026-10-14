pub(crate) const GRID_WIDTH: u32 = 128;
pub(crate) const GRID_HEIGHT: u32 = 128;
const GLIDER: &[(u32, u32)] = &[(1, 0), (2, 1), (0, 2), (1, 2), (2, 2)];

pub(crate) fn initial_cells() -> Vec<u32> {
    let mut cells = vec![0_u32; (GRID_WIDTH * GRID_HEIGHT) as usize];
    let origin = (GRID_WIDTH / 2 - 1, GRID_HEIGHT / 2 - 1);

    for &(offset_x, offset_y) in GLIDER {
        let x = origin.0 + offset_x;
        let y = origin.1 + offset_y;
        cells[(y * GRID_WIDTH + x) as usize] = 1;
    }

    cells
}

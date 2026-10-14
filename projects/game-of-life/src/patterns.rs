/// A known Life pattern in plaintext form: `O` is alive, `.` is dead, one string per row.
pub(crate) struct Pattern {
    pub(crate) name: &'static str,
    rows: &'static [&'static str],
}

impl Pattern {
    pub(crate) fn width(&self) -> u32 {
        self.rows.iter().map(|row| row.len()).max().unwrap_or(0) as u32
    }

    pub(crate) fn height(&self) -> u32 {
        self.rows.len() as u32
    }

    /// Row-major cells over the bounding box, `1` for alive and `0` for dead.
    pub(crate) fn cells(&self) -> Vec<u32> {
        let width = self.width() as usize;
        let mut cells = vec![0_u32; width * self.rows.len()];
        for (y, row) in self.rows.iter().enumerate() {
            for (x, cell) in row.bytes().enumerate() {
                cells[y * width + x] = u32::from(cell == b'O');
            }
        }
        cells
    }
}

pub(crate) const PATTERNS: &[Pattern] = &[
    Pattern {
        name: "Glider",
        rows: &[
            ".O.", //
            "..O", //
            "OOO",
        ],
    },
    Pattern {
        name: "Lightweight spaceship",
        rows: &[
            ".O..O", //
            "O....", //
            "O...O", //
            "OOOO.",
        ],
    },
    Pattern {
        name: "Pulsar",
        rows: &[
            "..OOO...OOO..",
            ".............",
            "O....O.O....O",
            "O....O.O....O",
            "O....O.O....O",
            "..OOO...OOO..",
            ".............",
            "..OOO...OOO..",
            "O....O.O....O",
            "O....O.O....O",
            "O....O.O....O",
            ".............",
            "..OOO...OOO..",
        ],
    },
    Pattern {
        name: "Gosper glider gun",
        rows: &[
            "........................O...........",
            "......................O.O...........",
            "............OO......OO............OO",
            "...........O...O....OO............OO",
            "OO........O.....O...OO..............",
            "OO........O...O.OO....O.O...........",
            "..........O.....O.......O...........",
            "...........O...O....................",
            "............OO......................",
        ],
    },
];

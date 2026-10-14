struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
}

struct GridDimensions {
    width: u32,
    height: u32,
    _padding: vec2<u32>,
}

@group(0) @binding(0)
var<storage, read> cells: array<u32>;

@group(0) @binding(1)
var<uniform> grid: GridDimensions;

@vertex
fn vertex(@builtin(vertex_index) vertex_index: u32) -> VertexOutput {
    let positions = array(
        vec2<f32>(-1.0, -1.0),
        vec2<f32>(3.0, -1.0),
        vec2<f32>(-1.0, 3.0),
    );
    let uvs = array(
        vec2<f32>(0.0, 1.0),
        vec2<f32>(2.0, 1.0),
        vec2<f32>(0.0, -1.0),
    );

    var output: VertexOutput;
    output.position = vec4<f32>(positions[vertex_index], 0.0, 1.0);
    output.uv = uvs[vertex_index];
    return output;
}

@fragment
fn fragment(input: VertexOutput) -> @location(0) vec4<f32> {
    let dimensions = vec2<u32>(grid.width, grid.height);
    let cell = min(
        vec2<u32>(input.uv * vec2<f32>(dimensions)),
        dimensions - vec2<u32>(1u),
    );
    let index = cell.y * grid.width + cell.x;
    let is_alive = cells[index] != 0u;

    let dead_color = vec3<f32>(0.04, 0.06, 0.09);
    let alive_color = vec3<f32>(0.55, 0.95, 0.65);
    let cell_color = select(dead_color, alive_color, is_alive);

    // Grid lines are about one pixel wide at any cell size: dividing the distance to the
    // nearest cell edge by `fwidth` converts it from cells to pixels.
    let position = input.uv * vec2<f32>(dimensions);
    let edge_distance = abs(fract(position - 0.5) - 0.5) / fwidth(position);
    let line = 1.0 - clamp(min(edge_distance.x, edge_distance.y), 0.0, 1.0);
    let color = mix(cell_color, cell_color * 0.45, line);

    return vec4<f32>(color, 1.0);
}

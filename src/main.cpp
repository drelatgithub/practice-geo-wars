#include <chrono>
#include <iostream>

#include "visual/window.hpp"

using namespace pgw;

inline std::vector< Vertex > vertices {
    {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, 0.5f }, {0.0f, 1.0f, 0.0f}},
    {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
};

int main() {
    Window w(800, 600);
    w.mainloop([&]{
        using namespace std;
        using namespace std::chrono;

        // Replace vertices
        if(vertices.size() == 3) {
            vertices.push_back({{0.6f, -0.7f}, {1.0f, 0.0f, 0.0f}});
            vertices.push_back({{0.65f, -0.65f}, {0.0f, 1.0f, 0.0f}});
            vertices.push_back({{0.55f, -0.65f}, {0.0f, 0.0f, 0.0f}});
        }
        else {
            vertices.resize(3);
        }
        w.copy_vertex_data(vertices);

        static auto last_time = steady_clock::now();
        auto this_time = steady_clock::now();
        double fr = 1.0 / duration_cast<duration<double>>(this_time - last_time).count();
        cout << "frame_rate=" << fr << endl;
        last_time = this_time;
    });

    return 0;
}

#ifndef PGW_GAME_GEO_WARS_GAME_HPP
#define PGW_GAME_GEO_WARS_GAME_HPP

#include <chrono>
#include <iostream>

#include "visual/window.hpp"

namespace pgw {

inline void run_game() {
    using namespace std;

    std::vector< Vertex > vertices {
        {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, 0.5f }, {0.0f, 1.0f, 0.0f}},
        {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
    };

    Window w(800, 600);

    // set key callbacks
    w.glfw_callbacks().key_callback = [](int key, int scancode, int action, int mods) -> void {
        cout << "Key pressed " << key << " scancode=" << scancode << endl;
    };

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
}

} // namespace pgw

#endif

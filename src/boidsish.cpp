#include "boidsish.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <deque>
#include <map>
#include <cmath>
#include <chrono>

namespace Boidsish {

// Trail data for each dot
struct Trail {
    std::deque<std::tuple<float, float, float, float>> positions; // x, y, z, alpha
    int max_length;

    Trail(int length = 10) : max_length(length) {}

    void AddPosition(float x, float y, float z) {
        positions.push_back({x, y, z, 1.0f});
        if (positions.size() > static_cast<size_t>(max_length)) {
            positions.pop_front();
        }

        // Update alpha values for fade effect
        for (size_t i = 0; i < positions.size(); ++i) {
            float alpha = static_cast<float>(i) / static_cast<float>(positions.size());
            std::get<3>(positions[i]) = alpha;
        }
    }
};

// Implementation details hidden from header
struct Visualizer::VisualizerImpl {
    GLFWwindow* window;
    int width, height;
    Camera camera;
    DotFunction dot_function;
    std::map<size_t, Trail> trails; // Trail for each dot (using hash of position as key)

    // Mouse and keyboard state
    double last_mouse_x, last_mouse_y;
    bool first_mouse;
    bool keys[1024];

    // Auto-camera state
    bool auto_camera_mode;
    float auto_camera_time;
    float auto_camera_angle;
    float auto_camera_height_offset;
    float auto_camera_distance;

    // Timing
    std::chrono::high_resolution_clock::time_point start_time;

    VisualizerImpl(int w, int h, const char* title)
        : window(nullptr), width(w), height(h), camera(), dot_function(nullptr),
          last_mouse_x(0.0), last_mouse_y(0.0), first_mouse(true),
          auto_camera_mode(false), auto_camera_time(0.0f), auto_camera_angle(0.0f),
          auto_camera_height_offset(0.0f), auto_camera_distance(10.0f) {

        // Initialize all keys to false
        for (int i = 0; i < 1024; ++i) {
            keys[i] = false;
        }

        start_time = std::chrono::high_resolution_clock::now();

        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }

        // Set error callback to get more info
        glfwSetErrorCallback([](int error, const char* description) {
            std::cerr << "GLFW Error " << error << ": " << description << std::endl;
        });

        // Use more flexible OpenGL settings for better compatibility
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);

        // For macOS - try without forward compatibility first
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_FALSE);

        window = glfwCreateWindow(width, height, title, nullptr, nullptr);
        if (!window) {
            std::cerr << "Failed to create window with OpenGL 2.1, trying with default settings..." << std::endl;

            // Reset hints and try with default OpenGL
            glfwDefaultWindowHints();
            window = glfwCreateWindow(width, height, title, nullptr, nullptr);

            if (!window) {
                std::cerr << "Failed to create window with default settings" << std::endl;
                glfwTerminate();
                throw std::runtime_error("Failed to create GLFW window - check OpenGL drivers");
            }
        }

        glfwMakeContextCurrent(window);

        // Initialize GLEW (optional for basic OpenGL)
        GLenum glew_status = glewInit();
        if (glew_status != GLEW_OK) {
            std::cerr << "GLEW initialization failed: " << glewGetErrorString(glew_status) << std::endl;
            std::cerr << "Continuing without GLEW - basic OpenGL should still work" << std::endl;
        } else {
            std::cout << "GLEW initialized successfully" << std::endl;
        }

        // Set up callbacks
        glfwSetWindowUserPointer(window, this);
        glfwSetKeyCallback(window, KeyCallback);
        glfwSetCursorPosCallback(window, MouseCallback);
        glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);

        // Capture cursor
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        // Enable depth testing
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Check OpenGL version
        const GLubyte* version = glGetString(GL_VERSION);
        std::cout << "OpenGL Version: " << version << std::endl;

        // Set background color (black for holodeck effect)
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

        SetupPerspective();
    }

    ~VisualizerImpl() {
        if (window) {
            glfwDestroyWindow(window);
        }
        glfwTerminate();
    }

    void SetupPerspective() {
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();

        float aspect = static_cast<float>(width) / static_cast<float>(height);

        // Use simpler perspective setup that's more compatible
        glViewport(0, 0, width, height);

        // Simple perspective using similar approach to gluPerspective
        float fovy = camera.fov;
        float zNear = 0.1f;
        float zFar = 1000.0f;

        float fH = tan(fovy / 360.0f * M_PI) * zNear;
        float fW = fH * aspect;

        glFrustum(-fW, fW, -fH, fH, zNear, zFar);
    }

    void SetupCamera() {
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        // Apply camera rotation and translation
        glRotatef(-camera.pitch, 1.0f, 0.0f, 0.0f);
        glRotatef(-camera.yaw, 0.0f, 1.0f, 0.0f);
        glTranslatef(-camera.x, -camera.y, -camera.z);
    }

    void ProcessInput(float delta_time) {
        // Skip manual camera controls if auto-camera is active
        if (auto_camera_mode) {
            return;
        }

        float camera_speed = 5.0f * delta_time;

        // Calculate camera direction vectors using standard FPS math
        float yaw_rad = camera.yaw * M_PI / 180.0f;
        float pitch_rad = camera.pitch * M_PI / 180.0f;

        // Forward vector - where the camera is looking
        float forward_x = cos(pitch_rad) * sin(yaw_rad);
        float forward_y = sin(pitch_rad);
        float forward_z = -cos(pitch_rad) * cos(yaw_rad);

        // Right vector - cross product of forward and world up (0,1,0)
        float right_x = cos(yaw_rad);
        float right_z = -sin(yaw_rad);

        // Movement
        if (keys[GLFW_KEY_W]) {
            camera.x += forward_x * camera_speed;
            camera.y += forward_y * camera_speed;
            camera.z += forward_z * camera_speed;
        }
        if (keys[GLFW_KEY_S]) {
            camera.x -= forward_x * camera_speed;
            camera.y -= forward_y * camera_speed;
            camera.z -= forward_z * camera_speed;
        }
        if (keys[GLFW_KEY_A]) {
            camera.x -= right_x * camera_speed;
            camera.z -= right_z * camera_speed;
        }
        if (keys[GLFW_KEY_D]) {
            camera.x += right_x * camera_speed;
            camera.z += right_z * camera_speed;
        }
        if (keys[GLFW_KEY_SPACE]) {
            camera.y += camera_speed;
        }
        if (keys[GLFW_KEY_LEFT_SHIFT]) {
            camera.y -= camera_speed;
        }
    }

    void UpdateAutoCamera(float delta_time, const std::vector<Dot>& dots) {
        if (!auto_camera_mode || dots.empty()) {
            return;
        }

        auto_camera_time += delta_time;

        // Calculate mean position of all dots
        float mean_x = 0.0f, mean_y = 0.0f, mean_z = 0.0f;
        float min_x = dots[0].x, max_x = dots[0].x;
        float min_y = dots[0].y, max_y = dots[0].y;
        float min_z = dots[0].z, max_z = dots[0].z;

        for (const auto& dot : dots) {
            mean_x += dot.x;
            mean_y += dot.y;
            mean_z += dot.z;

            min_x = std::min(min_x, dot.x);
            max_x = std::max(max_x, dot.x);
            min_y = std::min(min_y, dot.y);
            max_y = std::max(max_y, dot.y);
            min_z = std::min(min_z, dot.z);
            max_z = std::max(max_z, dot.z);
        }

        mean_x /= dots.size();
        mean_y /= dots.size();
        mean_z /= dots.size();

        // Calculate bounding box size to determine appropriate distance
        float extent = std::max({max_x - min_x, max_y - min_y, max_z - min_z});
        float target_distance = extent * 2.0f + 5.0f; // Ensure all points are visible

        // Smoothly adjust distance to keep points in center 2/3 of screen
        auto_camera_distance += (target_distance - auto_camera_distance) * delta_time * 0.5f;

        // Sinusoidal height variation (staying above floor, max 30 degrees)
        float height_cycle = sin(auto_camera_time * 0.3f) * 0.5f + 0.5f; // 0 to 1
        float camera_height = mean_y + 1.0f + height_cycle * auto_camera_distance * 0.5f;

        // Gentle rotation around mean point with varying speed
        float rotation_speed = 0.2f + 0.1f * sin(auto_camera_time * 0.15f); // Variable speed
        auto_camera_angle += rotation_speed * delta_time;

        // Gradual direction changes
        float direction_modifier = sin(auto_camera_time * 0.08f) * 0.3f; // Gentle direction variation
        float effective_angle = auto_camera_angle + direction_modifier;

        // Position camera in a circle around the mean point
        camera.x = mean_x + cos(effective_angle) * auto_camera_distance;
        camera.z = mean_z + sin(effective_angle) * auto_camera_distance;
        camera.y = camera_height;

        // Look at the mean point
        float dx = mean_x - camera.x;
        float dy = mean_y - camera.y;
        float dz = mean_z - camera.z;

        float distance_xz = sqrt(dx * dx + dz * dz);

        // Calculate yaw and pitch to look at mean point
        camera.yaw = atan2(dx, -dz) * 180.0f / M_PI;
        camera.pitch = atan2(dy, distance_xz) * 180.0f / M_PI;

        // Constrain pitch to stay below 30 degrees above horizontal
        camera.pitch = std::max(-89.0f, std::min(30.0f, camera.pitch));
    }

    void RenderGrid() {
        glDisable(GL_DEPTH_TEST);
        glLineWidth(1.0f);

        // Holodeck-style grid on z=0 plane
        glBegin(GL_LINES);

        int grid_size = 25;
        float grid_spacing = 1.0f;

        // Main grid lines (brighter cyan)
        glColor4f(0.0f, 0.8f, 1.0f, 0.6f);

        for (int i = -grid_size; i <= grid_size; ++i) {
            // Every 5th line is brighter
            if (i % 5 == 0) {
                glColor4f(0.0f, 0.9f, 1.0f, 0.8f);
            } else {
                glColor4f(0.0f, 0.6f, 0.8f, 0.4f);
            }

            // Lines parallel to X axis (on z=0 plane)
            glVertex3f(-grid_size * grid_spacing, 0.0f, i * grid_spacing);
            glVertex3f(grid_size * grid_spacing, 0.0f, i * grid_spacing);

            // Lines parallel to Z axis (on z=0 plane)
            glVertex3f(i * grid_spacing, 0.0f, -grid_size * grid_spacing);
            glVertex3f(i * grid_spacing, 0.0f, grid_size * grid_spacing);
        }

        // Add subtle border/frame effect
        glColor4f(0.0f, 1.0f, 1.0f, 0.9f);
        glVertex3f(-grid_size * grid_spacing, 0.0f, -grid_size * grid_spacing);
        glVertex3f(grid_size * grid_spacing, 0.0f, -grid_size * grid_spacing);

        glVertex3f(grid_size * grid_spacing, 0.0f, -grid_size * grid_spacing);
        glVertex3f(grid_size * grid_spacing, 0.0f, grid_size * grid_spacing);

        glVertex3f(grid_size * grid_spacing, 0.0f, grid_size * grid_spacing);
        glVertex3f(-grid_size * grid_spacing, 0.0f, grid_size * grid_spacing);

        glVertex3f(-grid_size * grid_spacing, 0.0f, grid_size * grid_spacing);
        glVertex3f(-grid_size * grid_spacing, 0.0f, -grid_size * grid_spacing);

        glEnd();
        glEnable(GL_DEPTH_TEST);
    }

    void RenderDot(const Dot& dot) {
        glPointSize(dot.size);
        glBegin(GL_POINTS);
        glColor4f(dot.r, dot.g, dot.b, dot.a);
        glVertex3f(dot.x, dot.y, dot.z);
        glEnd();

        // Also draw as a small quad for better visibility
        float half_size = dot.size * 0.02f;
        glBegin(GL_QUADS);
        glColor4f(dot.r, dot.g, dot.b, dot.a);
        glVertex3f(dot.x - half_size, dot.y - half_size, dot.z);
        glVertex3f(dot.x + half_size, dot.y - half_size, dot.z);
        glVertex3f(dot.x + half_size, dot.y + half_size, dot.z);
        glVertex3f(dot.x - half_size, dot.y + half_size, dot.z);
        glEnd();
    }

    void RenderTrail(const Trail& trail, float r, float g, float b) {
        if (trail.positions.size() < 2) return;

        glBegin(GL_LINE_STRIP);
        for (const auto& pos : trail.positions) {
            float alpha = std::get<3>(pos);
            glColor4f(r, g, b, alpha * 0.5f);
            glVertex3f(std::get<0>(pos), std::get<1>(pos), std::get<2>(pos));
        }
        glEnd();
    }

    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        (void)scancode; (void)mods; // Suppress unused parameter warnings
        VisualizerImpl* impl = static_cast<VisualizerImpl*>(glfwGetWindowUserPointer(window));

        if (key >= 0 && key < 1024) {
            if (action == GLFW_PRESS) {
                impl->keys[key] = true;
            } else if (action == GLFW_RELEASE) {
                impl->keys[key] = false;
            }
        }

        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }

        // Toggle auto-camera mode with 0 key
        if (key == GLFW_KEY_0 && action == GLFW_PRESS) {
            impl->auto_camera_mode = !impl->auto_camera_mode;
            if (impl->auto_camera_mode) {
                std::cout << "Auto-camera mode enabled" << std::endl;
                // Reset cursor capture when switching to auto mode
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            } else {
                std::cout << "Auto-camera mode disabled" << std::endl;
                // Re-enable cursor capture for manual mode
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                impl->first_mouse = true; // Reset mouse to avoid jumps
            }
        }
    }

    static void MouseCallback(GLFWwindow* window, double xpos, double ypos) {
        VisualizerImpl* impl = static_cast<VisualizerImpl*>(glfwGetWindowUserPointer(window));

        // Skip mouse input if auto-camera is active
        if (impl->auto_camera_mode) {
            return;
        }

        if (impl->first_mouse) {
            impl->last_mouse_x = xpos;
            impl->last_mouse_y = ypos;
            impl->first_mouse = false;
        }

        double xoffset = xpos - impl->last_mouse_x;
        double yoffset = ypos - impl->last_mouse_y; // Fixed: don't reverse Y

        impl->last_mouse_x = xpos;
        impl->last_mouse_y = ypos;

        float sensitivity = 0.1f;
        xoffset *= sensitivity;
        yoffset *= sensitivity;

        impl->camera.yaw += static_cast<float>(xoffset);  // Fixed: add for correct direction
        impl->camera.pitch -= static_cast<float>(yoffset); // Subtract for natural mouse feel

        // Constrain pitch
        if (impl->camera.pitch > 89.0f) {
            impl->camera.pitch = 89.0f;
        }
        if (impl->camera.pitch < -89.0f) {
            impl->camera.pitch = -89.0f;
        }
    }

    static void FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
        VisualizerImpl* impl = static_cast<VisualizerImpl*>(glfwGetWindowUserPointer(window));
        impl->width = width;
        impl->height = height;
        glViewport(0, 0, width, height);
        impl->SetupPerspective();
    }
};

// Visualizer public interface implementation
Visualizer::Visualizer(int width, int height, const char* title) {
    impl = new VisualizerImpl(width, height, title);
}

Visualizer::~Visualizer() {
    delete impl;
}

void Visualizer::SetDotFunction(DotFunction func) {
    impl->dot_function = func;
}

bool Visualizer::ShouldClose() const {
    return glfwWindowShouldClose(impl->window);
}

void Visualizer::Update() {
    glfwPollEvents();

    static auto last_frame = std::chrono::high_resolution_clock::now();
    auto current_frame = std::chrono::high_resolution_clock::now();
    float delta_time = std::chrono::duration<float>(current_frame - last_frame).count();
    last_frame = current_frame;

    impl->ProcessInput(delta_time);
}

void Visualizer::Render() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Get current time for dot function
    auto current_time = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float>(current_time - impl->start_time).count();

    // Get dots first so we can use them for auto-camera
    std::vector<Dot> dots;
    if (impl->dot_function) {
        dots = impl->dot_function(time);

        static bool first_render = true;
        if (first_render) {
            std::cout << "Rendering " << dots.size() << " dots" << std::endl;
            if (!dots.empty()) {
                std::cout << "First dot: pos(" << dots[0].x << ", " << dots[0].y << ", " << dots[0].z
                         << ") color(" << dots[0].r << ", " << dots[0].g << ", " << dots[0].b << ")" << std::endl;
            }
            first_render = false;
        }
    }

    // Update auto-camera before setting up camera
    static auto last_frame_time = current_time;
    float delta_time = std::chrono::duration<float>(current_time - last_frame_time).count();
    last_frame_time = current_time;
    impl->UpdateAutoCamera(delta_time, dots);

    impl->SetupCamera();

    // Render grid
    impl->RenderGrid();

    // Render dots and trails
    if (!dots.empty()) {
        for (size_t i = 0; i < dots.size(); ++i) {
            const Dot& dot = dots[i];

            // Create or update trail
            auto& trail = impl->trails[i];
            trail.max_length = dot.trail_length;
            trail.AddPosition(dot.x, dot.y, dot.z);

            // Render trail
            impl->RenderTrail(trail, dot.r, dot.g, dot.b);

            // Render dot
            impl->RenderDot(dot);
        }
    }

    glfwSwapBuffers(impl->window);
}

void Visualizer::Run() {
    while (!ShouldClose()) {
        Update();
        Render();
    }
}

Camera& Visualizer::GetCamera() {
    return impl->camera;
}

void Visualizer::SetCamera(const Camera& camera) {
    impl->camera = camera;
}

} // namespace Boidsish
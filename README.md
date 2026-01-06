# Boidsish - A Surprisingly Serious 3D Playground

Have you ever wanted to render a truly absurd number of dots, particles, or other simple shapes, and then make them do interesting things?

Me too.

That's why I built Boidsish. It started as a simple C++ framework for creating flocking simulations (hence the name), but it quickly spiraled into a surprisingly robust playground for all sorts of GPU-accelerated visual experiments. It's a bit niche, a bit nerdy, and probably a bit over-engineered, but it's a lot of fun.

## How It Works (The Gist)

At its core, Boidsish is a lean, mean, 3D rendering machine. It's built on a modern OpenGL pipeline and is designed to handle a *lot* of things on the screen at once. Here’s a peek under the hood:

-   **The "Engine":** A C++ application that wrangles OpenGL, GLFW, and Dear ImGui to create a flexible environment for visualization. It handles the low-level stuff like windowing, input, and shader management so you can focus on the fun parts.

-   **The "Shapes":** The framework is built around a simple idea: you give it a list of shapes (dots, spheres, arrows, etc.), and it renders them. Fast. This is managed through a system of `Entity` and `Shape` classes, which lets you easily create and manipulate objects in the scene.

-   **The "Magic":** The real power comes from the GPU. Boidsish is designed to offload as much work as possible to shaders. This includes everything from calculating lighting and color to generating complex geometry on the fly with tessellation shaders.

-   **The "UI":** Thanks to Dear ImGui, most examples come with a simple UI that lets you tweak parameters, toggle effects, and switch camera modes in real-time. It's not always pretty, but it gets the job done.

## Frequently Asked Questions

### Isn't this massively too complicated for rendering a few dots?
Yes. Next question.

### What's with all the C++23 features?
Because life's too short to write C++ like it's 2003. Also, `std::views` is pretty neat.

### Why is the camera so fast/slow?
You can adjust the camera speed with the `[` and `]` keys. I probably should have put that in the UI.

### I ran an example and now my laptop is hot. Why?
You're probably rendering a few hundred thousand particles, each with its own trail, and running a particle simulation on a compute shader. It's a bit demanding.

### What's the deal with the `performance_and_quality_audit.md` file?
That's my laundry list of things I know I could be doing better. It's a mix of "this is a real bottleneck" and "this would be a fun challenge." It's a good place to look if you want to understand the project's future direction.

## Getting Your Hands Dirty

Ready to render some dots? Here’s how to get started.

### Dependencies

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get update && sudo apt-get install -y \
    libgl1-mesa-dev libglew-dev libglfw3-dev libglm-dev \
    libbullet-dev libassimp-dev xvfb imagemagick x11-apps
```

**macOS:**
Good luck. It probably works, but I haven't tried in a while. You'll need `brew` and the equivalent packages.

### Building and Running

1.  **Clone the repo (with submodules!):**
    ```bash
    git clone --recursive https://github.com/your-username/boidsish.git
    cd boidsish
    ```

2.  **Build it:**
    ```bash
    make all
    ```
    This will build the core library and all the examples.

3.  **Run an example:**
    ```bash
    ./build/terrain
    ```
    This one is a personal favorite. It generates infinite, procedurally-generated terrain on the GPU.

## What's Next?

I'm always tinkering with this project. Here are a few of the things I'm thinking about, most of which are shamelessly stolen from the performance audit file:

-   **GPU All the Things:** I want to move more logic to the GPU. This includes frustum culling, trail generation, and maybe even some of the core simulation logic. The CPU should be for thinking, not for number crunching.

-   **Prettier Reflections:** The current planar reflection effect is a bit of a brute-force hack. I'd like to replace it with something more efficient and, well, prettier.

-   **Stable Simulation:** Right now, the simulation speed is tied to the frame rate. This is fine for pretty pictures, but it's not great for physics. A fixed timestep is on the to-do list.

## Disclaimer

This is a hobby project. It's provided as-is, without warranty of any kind. If you manage to set your GPU on fire, I'm not responsible. But I would be slightly impressed.

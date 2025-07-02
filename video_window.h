#include "includes.h"

class VideoWindow {
public:
    VideoWindow()
    {
        SDL_Init(SDL_INIT_VIDEO);
        window = SDL_CreateWindow("Video Playback",
                                  SDL_WINDOWPOS_UNDEFINED,
                                  SDL_WINDOWPOS_UNDEFINED,
                                  1280, 720,
                                  SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN);
        is_fullscreen = true;
    }

    ~VideoWindow()
    {
        if (window)
            SDL_DestroyWindow(window);
        SDL_Quit();
    }

    void toggle_fullscreen()
    {
        if (is_fullscreen)
            SDL_SetWindowFullscreen(window, 0);
        else
            SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
        is_fullscreen = !is_fullscreen;
    }

    // Process SDL events, return false if the window should close
    bool process_events()
    {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                return false;
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_f)
                    toggle_fullscreen();
            }
        }
        return true;
    }

    guintptr get_window_handle()
    {
        SDL_SysWMinfo info;
        SDL_VERSION(&info.version);
        if (!SDL_GetWindowWMInfo(window, &info))
            return 0;
#if defined(SDL_VIDEO_DRIVER_X11)
        return (guintptr)info.info.x11.window;
#elif defined(SDL_VIDEO_DRIVER_WAYLAND)
        return (guintptr)info.info.wl.surface;
#else
        return 0;
#endif
    }

    SDL_Window* get_window() { return window; }

private:
    SDL_Window* window = nullptr;
    bool is_fullscreen = false;
};

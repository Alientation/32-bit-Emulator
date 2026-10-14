#include "emulator32bit/display_window.h"

#include "emulator32bit/host_display.h"
#include "emulator32bit/host_input.h"

#include <utility>

#define SDL_MAIN_HANDLED
#include <SDL.h>

/// How long the window waits for an event before it looks for a new frame, in milliseconds.
static constexpr int kPollMs = 5;

DisplayWindow::DisplayWindow(std::string title, const word scale) :
    m_title(std::move(title)),
    m_scale(scale)
{
}

DisplayWindow::~DisplayWindow()
{
    if (m_texture != nullptr) SDL_DestroyTexture(m_texture);
    if (m_renderer != nullptr) SDL_DestroyRenderer(m_renderer);
    if (m_window != nullptr) SDL_DestroyWindow(m_window);
    if (m_initialized) SDL_Quit();
}

bool DisplayWindow::open(std::string &error)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        error = SDL_GetError();
        return false;
    }
    m_initialized = true;

    // The pixels of the picture stay sharp squares when they are scaled up.
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    m_window = SDL_CreateWindow(m_title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                int(Display::kDefaultWidth * m_scale),
                                int(Display::kDefaultHeight * m_scale), SDL_WINDOW_RESIZABLE);
    if (m_window != nullptr) m_renderer = SDL_CreateRenderer(m_window, -1, 0);
    if (m_renderer == nullptr)
    {
        error = SDL_GetError();
        return false;
    }
    redraw();
    return true;
}

void DisplayWindow::run(HostDisplay &host, HostInput *keys, const std::atomic<bool> &stopped)
{
    Frame frame;
    bool said_stopped = false;
    for (;;)
    {
        SDL_Event event;
        bool has_event = SDL_WaitEventTimeout(&event, kPollMs) != 0;
        while (has_event)
        {
            if (event.type == SDL_QUIT)
            {
                host.close();
                return;
            }
            if (event.type == SDL_WINDOWEVENT) redraw();

            // Text is the characters typed, the keys that are not text are the control
            // characters a terminal would send.
            if (keys != nullptr && event.type == SDL_TEXTINPUT) keys->push(event.text.text);
            if (keys != nullptr && event.type == SDL_KEYDOWN)
            {
                switch (event.key.keysym.sym)
                {
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    keys->push("\n");
                    break;
                case SDLK_BACKSPACE:
                    keys->push("\b");
                    break;
                case SDLK_TAB:
                    keys->push("\t");
                    break;
                case SDLK_ESCAPE:
                    keys->push("\x1b");
                    break;
                default:
                    break;
                }
            }
            has_event = SDL_PollEvent(&event) != 0;
        }

        if (host.take(frame)) show(frame);

        if (!said_stopped && stopped.load(std::memory_order_acquire))
        {
            SDL_SetWindowTitle(m_window, (m_title + " (stopped)").c_str());
            said_stopped = true;
        }
    }
}

void DisplayWindow::show(const Frame &frame)
{
    if (frame.width != m_width || frame.height != m_height)
    {
        if (m_texture != nullptr) SDL_DestroyTexture(m_texture);
        m_texture =
            SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING,
                              int(frame.width), int(frame.height));
        m_width = frame.width;
        m_height = frame.height;
        SDL_RenderSetLogicalSize(m_renderer, int(m_width), int(m_height));
        SDL_SetWindowSize(m_window, int(m_width * m_scale), int(m_height * m_scale));
    }
    SDL_UpdateTexture(m_texture, nullptr, frame.pixels.data(), int(frame.width * sizeof(word)));
    redraw();
}

void DisplayWindow::redraw()
{
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_renderer);
    if (m_texture != nullptr) SDL_RenderCopy(m_renderer, m_texture, nullptr, nullptr);
    SDL_RenderPresent(m_renderer);
}

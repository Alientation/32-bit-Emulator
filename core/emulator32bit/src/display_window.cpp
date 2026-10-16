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
            if (keys != nullptr) forward(event, *keys);
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

/// @param[in] mod The modifiers of SDL.
/// @return The kMod* bits of a key event.
static word modifiers(const Uint16 mod)
{
    return ((mod & KMOD_SHIFT) ? InputDevice::kModShift : 0)
           | ((mod & KMOD_CTRL) ? InputDevice::kModCtrl : 0)
           | ((mod & KMOD_ALT) ? InputDevice::kModAlt : 0)
           | ((mod & KMOD_GUI) ? InputDevice::kModGui : 0);
}

/// @param[in] position A position of the mouse in pixels of the picture, which can be outside of
///                     it when the window has bars around the picture.
/// @param[in] size The pixels of the picture that way, 0 before the first frame.
/// @return The position in the picture.
static word clamp_position(const S32 position, const word size)
{
    if (position < 0) return 0;
    if (size != 0 && word(position) >= size) return size - 1;
    return word(position);
}

/// @param[in] button A button of SDL.
/// @return Its kButton* bit, 0 for one the input device does not have.
static word button_bit(const Uint8 button)
{
    switch (button)
    {
    case SDL_BUTTON_LEFT:
        return InputDevice::kButtonLeft;
    case SDL_BUTTON_RIGHT:
        return InputDevice::kButtonRight;
    case SDL_BUTTON_MIDDLE:
        return InputDevice::kButtonMiddle;
    default:
        return 0;
    }
}

void DisplayWindow::forward(const SDL_Event &event, HostInput &keys)
{
    switch (event.type)
    {
    case SDL_TEXTINPUT:
        // Text is the characters typed, for the console.
        keys.push(event.text.text);
        break;
    case SDL_KEYDOWN:
    case SDL_KEYUP:
    {
        // The code of SDL's scancode is the USB HID usage of the key.
        const bool down = event.type == SDL_KEYDOWN;
        const word flags = (down ? InputDevice::kKeyPressed : 0)
                           | (event.key.repeat != 0 ? InputDevice::kKeyRepeat : 0)
                           | modifiers(event.key.keysym.mod);
        keys.push_event({.kind = InputEvent::Kind::KEY,
                         .value = InputDevice::key_word(word(event.key.keysym.scancode), flags)});
        if (!down) break;

        // The keys that are not text are the control characters a terminal would send.
        switch (event.key.keysym.sym)
        {
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            keys.push("\n");
            break;
        case SDLK_BACKSPACE:
            keys.push("\b");
            break;
        case SDLK_TAB:
            keys.push("\t");
            break;
        case SDLK_ESCAPE:
            keys.push("\x1b");
            break;
        default:
            break;
        }
        break;
    }
    case SDL_MOUSEMOTION:
        // SDL gives the position in pixels of the picture, the logical size of the renderer.
        keys.push_event({.kind = InputEvent::Kind::MOUSE_MOVE,
                         .value = 0,
                         .x = clamp_position(event.motion.x, m_width),
                         .y = clamp_position(event.motion.y, m_height)});
        break;
    case SDL_MOUSEBUTTONDOWN:
        m_buttons |= button_bit(event.button.button);
        keys.push_event({.kind = InputEvent::Kind::MOUSE_BUTTONS, .value = m_buttons});
        break;
    case SDL_MOUSEBUTTONUP:
        m_buttons &= ~button_bit(event.button.button);
        keys.push_event({.kind = InputEvent::Kind::MOUSE_BUTTONS, .value = m_buttons});
        break;
    case SDL_MOUSEWHEEL:
    {
        const S32 steps =
            event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event.wheel.y : event.wheel.y;
        keys.push_event({.kind = InputEvent::Kind::MOUSE_WHEEL, .value = word(steps)});
        break;
    }
    default:
        break;
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

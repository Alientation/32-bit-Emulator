#pragma once

#include "emulator32bit/devices.h"

#include <atomic>
#include <string>

class HostDisplay;
class HostInput;
struct SDL_Renderer;
struct SDL_Texture;
struct SDL_Window;

/// The window of `emu32 --display` (docs/display.md), an SDL window that is only built with the
/// CMake option AEMU_GUI. It runs on the main thread and shows the frames that the display of the
/// machine, on another thread, presents to a HostDisplay. What is typed in it goes to the console.
class DisplayWindow
{
  public:
    /// @param[in] title The title of the window.
    /// @param[in] scale The pixels of the screen for a pixel of the picture, across and down.
    DisplayWindow(std::string title, word scale);

    ~DisplayWindow();

    DisplayWindow(const DisplayWindow &) = delete;
    DisplayWindow &operator=(const DisplayWindow &) = delete;

    /// Opens the window, black until the first frame.
    ///
    /// @param[out] error Why it could not be opened.
    /// @return Whether it is open.
    bool open(std::string &error);

    /// Shows the frames until the window is closed, which closes `host` (so the machine stops). The
    /// window stays when the machine stops, with the last picture, and says so in its title.
    ///
    /// @param[in,out] host Where the frames come from.
    /// @param[in] keys Where typed text goes, or null.
    /// @param[in] stopped Set by the thread of the machine when it stopped.
    void run(HostDisplay &host, HostInput *keys, const std::atomic<bool> &stopped);

  private:
    /// Shows a frame, and makes the window fit it when its size changed.
    ///
    /// @param[in] frame The frame.
    void show(const Frame &frame);

    /// Draws the texture again (the window was uncovered or resized).
    void redraw();

    std::string m_title;
    word m_scale;
    bool m_initialized = false;
    SDL_Window *m_window = nullptr;
    SDL_Renderer *m_renderer = nullptr;
    SDL_Texture *m_texture = nullptr;
    word m_width = 0; /// Of the texture.
    word m_height = 0;
};

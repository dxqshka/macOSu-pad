#pragma once

// macOS reshows its cursor over the menu bar/Dock/notch strip in fullscreen games; hide it there.

#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

#include <ApplicationServices/ApplicationServices.h>

namespace macosu_pad {

struct CursorHider {
    static std::unique_ptr<CursorHider> start();

    ~CursorHider();

    CursorHider(const CursorHider &) = delete;
    CursorHider &operator=(const CursorHider &) = delete;

private:
    CursorHider() = default;

    void watch();

    std::mutex _mutex;
    std::condition_variable _wake;
    bool _stop = false;
    std::thread _thread;
};

// Includes the menu bar strip, so a zoomed window does not count.
bool covers(CGRect window, CGRect display);

}  // namespace macosu_pad

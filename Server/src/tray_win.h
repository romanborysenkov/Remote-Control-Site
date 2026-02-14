#pragma once

#ifdef _WIN32
#include <atomic>

namespace tray {
void run(std::atomic<bool>& quitFlag);
}
#endif

#include <iostream>
#include <string>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <atomic>
#include <algorithm>
#include "websocket_server.h"
#include "keyboard_simulator.h"
#include "logger.h"
#ifdef _WIN32
#include "tray_win.h"
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

class RemoteControlServer {
public:
#ifdef _WIN32
    explicit RemoteControlServer(std::atomic<bool>* trayQuit = nullptr) : running_(true), trayQuit_(trayQuit) {
#else
    RemoteControlServer() : running_(true), trayQuit_(nullptr) {
#endif
#ifndef _WIN32
        signal(SIGTERM, [](int) {
            Logger::info("Отримано сигнал завершення. Зупиняємо програму...");
            exit(0);
        });
        signal(SIGINT, [](int) {
            Logger::info("Отримано сигнал переривання. Зупиняємо програму...");
            exit(0);
        });
#endif
    }

    void run() {
        Logger::info("=== Remote Control Server ===");
        Logger::info("Сервер приймає підключення та виконує команди left/right");

        server_.setMessageCallback([this](const std::string& message) {
            handleMessage(message);
        });

        Logger::info("Запуск WebSocket сервера на порту 8765");
        server_.start();

        while (running_ && server_.isRunning() && (!trayQuit_ || !trayQuit_->load())) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        running_ = false;

        server_.stop();
        Logger::info("Програма завершена.");
    }

private:
    void handleMessage(const std::string& message) {
        std::string cleanMessage = message;
        cleanMessage.erase(std::remove_if(cleanMessage.begin(), cleanMessage.end(),
            [](char c) { return c < 32 || c > 126; }), cleanMessage.end());

        if (cleanMessage == "right" || cleanMessage == "RIGHT") {
            KeyboardSimulator::simulateKey("right");
        } else if (cleanMessage == "left" || cleanMessage == "LEFT") {
            KeyboardSimulator::simulateKey("left");
        } else {
            Logger::warning("Невідома команда: " + cleanMessage);
        }
    }

    WebSocketServer server_{8765};
    std::atomic<bool> running_;
    std::atomic<bool>* trayQuit_ = nullptr;
};

#ifndef _WIN32
static void daemonize() {
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);
    if (setsid() < 0) exit(EXIT_FAILURE);
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);
    umask(0);
    int fd = open("/dev/null", O_RDWR);
    if (fd != -1) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > 2) close(fd);
    }
    chdir("/");
}

static std::string defaultLogPath() {
    return "/tmp/remotecontrol_server.log";
}
#else
static std::string defaultLogPath() {
    const char* tmp = getenv("TEMP");
    if (tmp && tmp[0]) return std::string(tmp) + "\\remotecontrol_server.log";
    tmp = getenv("TMP");
    if (tmp && tmp[0]) return std::string(tmp) + "\\remotecontrol_server.log";
    return "remotecontrol_server.log";
}
#endif

int main(int argc, char* argv[]) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return 1;
    }
#endif

    bool runAsDaemon = true;
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--foreground" || std::string(argv[i]) == "-f") {
            runAsDaemon = false;
            break;
        }
    }

#ifndef _WIN32
    if (runAsDaemon) {
        daemonize();
    }
#endif

    Logger::init(defaultLogPath());

    try {
        Logger::info("Запуск Remote Control Server");
#ifdef _WIN32
        std::atomic<bool> trayQuit{false};
        std::thread trayThread([&trayQuit] { tray::run(trayQuit); });
        RemoteControlServer server(&trayQuit);
        server.run();
        trayQuit.store(true);
        HWND h = FindWindowW(L"RemoteControlTray", nullptr);
        if (h) PostMessageW(h, WM_QUIT, 0, 0);
        if (trayThread.joinable()) trayThread.join();
#else
        RemoteControlServer server;
        server.run();
#endif
    } catch (const std::exception& e) {
        Logger::error("Помилка: " + std::string(e.what()));
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}

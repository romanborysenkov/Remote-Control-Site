#pragma once

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <cstdint>

class WebSocketServer {
public:
    using MessageCallback = std::function<void(const std::string&)>;

    explicit WebSocketServer(uint16_t port = 8765);
    ~WebSocketServer();

    void start();
    void stop();

    void setMessageCallback(MessageCallback callback);

    bool isRunning() const { return running_; }

private:
    void run();
    bool doHandshake(intptr_t clientFd);
    void handleClient(intptr_t clientFd);
    std::string decodeWebSocketFrame(const char* data, size_t len);
    static std::string computeAcceptKey(const std::string& key);

    uint16_t port_;
    MessageCallback messageCallback_;
    std::thread workerThread_;
    std::atomic<bool> running_;
};

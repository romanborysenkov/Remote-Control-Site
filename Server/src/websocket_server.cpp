#include "websocket_server.h"
#include "logger.h"
#include <cstring>
#include <sstream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#define close_socket closesocket
typedef SOCKET socket_fd_t;
#define INVALID_FD INVALID_SOCKET
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define close_socket close
typedef int socket_fd_t;
#define INVALID_FD (-1)
#endif

#if __APPLE__
#include <CommonCrypto/CommonDigest.h>
#else
#include "sha1.h"
#endif

namespace {

const char* wsMagic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
const size_t kFrameBufSize = 4096;

std::string base64Encode(const unsigned char* data, size_t len) {
    static const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((len + 2) / 3 * 4);
    for (size_t i = 0; i < len; i += 3) {
        unsigned int n = data[i] << 16;
        if (i + 1 < len) n |= data[i + 1] << 8;
        if (i + 2 < len) n |= data[i + 2];
        out += tbl[(n >> 18) & 63];
        out += tbl[(n >> 12) & 63];
        out += (i + 1 < len) ? tbl[(n >> 6) & 63] : '=';
        out += (i + 2 < len) ? tbl[n & 63] : '=';
    }
    return out;
}

} // namespace

std::string WebSocketServer::computeAcceptKey(const std::string& key) {
    std::string input = key + wsMagic;
#if __APPLE__
    unsigned char hashBuf[20];
    CC_SHA1(input.data(), static_cast<CC_LONG>(input.size()), hashBuf);
    return base64Encode(hashBuf, 20);
#else
    std::string hashBin = sha1::hash(input);
    return base64Encode(reinterpret_cast<const unsigned char*>(hashBin.data()), hashBin.size());
#endif
}

WebSocketServer::WebSocketServer(uint16_t port)
    : port_(port), running_(false) {}

WebSocketServer::~WebSocketServer() {
    stop();
}

void WebSocketServer::setMessageCallback(MessageCallback callback) {
    messageCallback_ = std::move(callback);
}

void WebSocketServer::start() {
    if (running_) return;
    running_ = true;
    workerThread_ = std::thread(&WebSocketServer::run, this);
}

void WebSocketServer::stop() {
    running_ = false;
    if (workerThread_.joinable())
        workerThread_.join();
}

void WebSocketServer::run() {
    socket_fd_t listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd == INVALID_FD) {
        Logger::error("Помилка створення сокета");
        running_ = false;
        return;
    }

    int opt = 1;
    if (setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt)) < 0) {
        Logger::error("setsockopt SO_REUSEADDR");
        close_socket(listenFd);
        running_ = false;
        return;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(listenFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        Logger::error("Помилка bind на порт " + std::to_string(port_));
        close_socket(listenFd);
        running_ = false;
        return;
    }

    if (listen(listenFd, 1) < 0) {
        Logger::error("Помилка listen");
        close_socket(listenFd);
        running_ = false;
        return;
    }

    Logger::info("WebSocket сервер слухає на порту " + std::to_string(port_));

    while (running_) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        socket_fd_t clientFd = accept(listenFd, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientFd == INVALID_FD) {
            if (running_) Logger::error("Помилка accept");
            break;
        }

        char clientIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, sizeof(clientIp));
        Logger::info("Клієнт підключено: " + std::string(clientIp));

        if (doHandshake(static_cast<intptr_t>(clientFd))) {
            handleClient(static_cast<intptr_t>(clientFd));
        }
        close_socket(clientFd);
        Logger::info("Клієнт відключено, очікуємо нового...");
    }

    close_socket(listenFd);
    running_ = false;
}

bool WebSocketServer::doHandshake(intptr_t clientFd) {
    char buf[1024];
#ifdef _WIN32
    int n = recv(reinterpret_cast<SOCKET>(clientFd), buf, sizeof(buf) - 1, 0);
#else
    ssize_t n = recv(static_cast<int>(clientFd), buf, sizeof(buf) - 1, 0);
#endif
    if (n <= 0) return false;
    buf[n] = '\0';
    std::string request(buf);

    std::string key;
    size_t pos = request.find("Sec-WebSocket-Key:");
    if (pos != std::string::npos) {
        pos = request.find_first_not_of(" \t", pos + 18);
        size_t end = request.find("\r\n", pos);
        if (end != std::string::npos)
            key = request.substr(pos, end - pos);
    }
    if (key.empty()) {
        Logger::error("Немає Sec-WebSocket-Key у запиті");
        return false;
    }

    std::string acceptKey = computeAcceptKey(key);
    if (acceptKey.empty()) {
        Logger::error("Помилка обчислення Accept key");
        return false;
    }

    std::ostringstream resp;
    resp << "HTTP/1.1 101 Switching Protocols\r\n"
         << "Upgrade: websocket\r\n"
         << "Connection: Upgrade\r\n"
         << "Sec-WebSocket-Accept: " << acceptKey << "\r\n"
         << "\r\n";
    std::string response = resp.str();
#ifdef _WIN32
    int sent = send(reinterpret_cast<SOCKET>(clientFd), response.data(), static_cast<int>(response.size()), 0);
#else
    ssize_t sent = send(static_cast<int>(clientFd), response.data(), response.size(), 0);
#endif
    if (sent != static_cast<decltype(sent)>(response.size())) {
        Logger::error("Помилка відправки handshake");
        return false;
    }
    Logger::info("WebSocket handshake успішний");
    return true;
}

void WebSocketServer::handleClient(intptr_t clientFd) {
    char buffer[kFrameBufSize];
    while (running_) {
#ifdef _WIN32
        int n = recv(reinterpret_cast<SOCKET>(clientFd), buffer, sizeof(buffer), 0);
#else
        ssize_t n = recv(static_cast<int>(clientFd), buffer, sizeof(buffer), 0);
#endif
        if (n <= 0) break;

        std::string msg = decodeWebSocketFrame(buffer, static_cast<size_t>(n));
        if (!msg.empty() && messageCallback_) {
            messageCallback_(msg);
        }
    }
}

std::string WebSocketServer::decodeWebSocketFrame(const char* data, size_t len) {
    if (len < 2) return "";

    const unsigned char* u = reinterpret_cast<const unsigned char*>(data);
    int opcode = u[0] & 0x0F;
    bool masked = (u[1] & 0x80) != 0;
    uint64_t payloadLen = u[1] & 0x7F;
    size_t headerLen = 2;

    if (payloadLen == 126) {
        if (len < 4) return "";
        payloadLen = (u[2] << 8) | u[3];
        headerLen = 4;
    } else if (payloadLen == 127) {
        if (len < 10) return "";
        payloadLen = (static_cast<uint64_t>(u[2]) << 56) | (static_cast<uint64_t>(u[3]) << 48)
                   | (static_cast<uint64_t>(u[4]) << 40) | (static_cast<uint64_t>(u[5]) << 32)
                   | (u[6] << 24) | (u[7] << 16) | (u[8] << 8) | u[9];
        headerLen = 10;
    }

    if (opcode != 0x1) return "";
    if (!masked || len < headerLen + 4 + payloadLen) return "";

    const unsigned char* mask = u + headerLen;
    std::string result;
    result.reserve(payloadLen);
    for (size_t i = 0; i < payloadLen; i++)
        result += static_cast<char>(u[headerLen + 4 + i] ^ mask[i % 4]);
    return result;
}

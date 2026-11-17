#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>

#include <string>
#include <thread>
#include <utility>

#pragma comment(lib, "ws2_32.lib")

struct Script {
    int status = 200;
    std::string headers;
    std::string body;
    int delay_ms = 0;
};

class TestServer {
public:
    explicit TestServer(Script script) : script_(std::move(script)) {
        WSADATA data{};
        ::WSAStartup(MAKEWORD(2, 2), &data);
        socket_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        int yes = 1;
        ::setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes));

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
        ::bind(socket_, (sockaddr*)&address, sizeof(address));
        ::listen(socket_, 4);

        int length = sizeof(address);
        ::getsockname(socket_, (sockaddr*)&address, &length);
        port_ = ::ntohs(address.sin_port);
        thread_ = std::thread([this] { Serve(); });
    }

    ~TestServer() {
        ::closesocket(socket_);
        if (thread_.joinable()) thread_.join();
        ::WSACleanup();
    }

    int port() const { return port_; }

    std::wstring url(const wchar_t* path) const {
        return L"http://127.0.0.1:" + std::to_wstring(port_) + path;
    }

private:
    void Serve() {
        // Keep accepting so retried requests each get an answer, and so the
        // destructor's closesocket() unblocks accept instead of leaking a thread.
        for (;;) {
            SOCKET client = ::accept(socket_, nullptr, nullptr);
            if (client == INVALID_SOCKET) return;

            char buffer[8192];
            ::recv(client, buffer, sizeof(buffer), 0);
            if (script_.delay_ms) ::Sleep(script_.delay_ms);

            const std::string reply = "HTTP/1.1 " + std::to_string(script_.status) +
                                      " X\r\n" + script_.headers + "Content-Length: " +
                                      std::to_string(script_.body.size()) +
                                      "\r\nConnection: close\r\n\r\n" + script_.body;
            ::send(client, reply.data(), (int)reply.size(), 0);
            ::closesocket(client);
        }
    }

    Script script_;
    SOCKET socket_ = INVALID_SOCKET;
    int port_ = 0;
    std::thread thread_;
};

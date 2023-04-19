#pragma once

#include "pch.h"

#include <string>
#include <iostream>

#define DEFAULT_BUFLEN 512

class WsaApi {
public:
	WsaApi() {
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) < 0) {
			throw std::runtime_error("Failed to initialize winsock api");
		}

	}

	~WsaApi() {
		WSACleanup();
	}
private:
	WSADATA wsaData = { 0 };
};

class Socket {
public:
	Socket(const char* Ip, USHORT Port) : conn(INVALID_SOCKET) {
		struct addrinfo *addresses, hints = { 0 };
		
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		auto result = getaddrinfo(Ip, std::to_string(Port).c_str(), &hints, &addresses);
		if (result != 0) {
			throw std::runtime_error("Failed to get address info");
		}

		for (auto* addrinfo = addresses; addrinfo != nullptr; addrinfo = addrinfo->ai_next) {
			this->conn = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
			if (this->conn == INVALID_SOCKET) {
				throw std::runtime_error("Couldn't create socket");
			}

			result = connect(this->conn, addrinfo->ai_addr, addrinfo->ai_addrlen);
			if (result == SOCKET_ERROR) {
				closesocket(this->conn);
				this->conn = INVALID_SOCKET;
				continue;
			}
			break;
		}

		freeaddrinfo(addresses);
		if (this->conn == INVALID_SOCKET) {
			throw std::runtime_error("Couldn't connect to socket");
		}
	}

	void Send(const std::string& Data) {
		if (send(this->conn, Data.c_str(), Data.size(), 0) == SOCKET_ERROR) {
			throw std::runtime_error("Failed to send data");
		}
	}

	std::string Recv() {
		char recvbuf[DEFAULT_BUFLEN + 1] = { 0 };
		int result = 0;
		std::string received = "";
		while ((result = recv(this->conn, recvbuf, DEFAULT_BUFLEN, 0)) == DEFAULT_BUFLEN) {
			received.append(recvbuf);
			memset(recvbuf, 0, DEFAULT_BUFLEN);
		}
		if (result == SOCKET_ERROR) {
			throw std::runtime_error("Failed to recv data");
		}
		received.append(recvbuf);
		return received;
	}

	~Socket() noexcept {
		auto result = shutdown(this->conn, SD_BOTH);
		if (result != SOCKET_ERROR) {
			closesocket(this->conn);
		}
	}

private:
	SOCKET conn;
};
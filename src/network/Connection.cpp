/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: strodrig <strodrig@student.42lisboa.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/10/26 17:38:33 by tborges-          #+#    #+#             */
/*   Updated: 2025/11/08 12:50:55 by strodrig         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * Connection.cpp
 * Implementation of Connection class
 */
#include "includes/network/Connection.hpp"
#include "includes/network/Socket.hpp"
#include "includes/config/Server.hpp"
#include "includes/http/RequestHandler.hpp"
#include "includes/utils/Logger.hpp"
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <ctime>
#include <sstream>
#include <cstdlib>

// Constructors
Connection::Connection(int fd, const struct sockaddr_in& addr, const Server* server)
	: _fd(fd)
	, _addr(addr)
	, _clientHost(Socket::getHostString(addr))
	, _clientPort(Socket::getPortNumber(addr))
	, _server(server)
	, _state(READING_REQUEST)
	, _lastActivity(std::time(NULL))
	, _responseOffset(0)
	, _keepAlive(false)
	, _shouldClose(false) {

	Logger::info << "New connection from " << _clientHost << ":" << _clientPort
	             << " (fd: " << _fd << ")" << std::endl;
}

Connection::~Connection() {
	if (_fd >= 0) {
		::close(_fd);
		Logger::debug << "Connection closed (fd: " << _fd << ")" << std::endl;
	}
}

// I/O operations
bool Connection::readRequest() {
	const size_t BUFFER_SIZE = 4096;
	char buffer[BUFFER_SIZE];

	ssize_t bytesRead = recv(_fd, buffer, BUFFER_SIZE - 1, 0);

	if (bytesRead < 0) {
		// Non-blocking socket: would block means no data available
		// Don't check errno - just return success and try again later
		return true; // Not an error for non-blocking sockets
	}

	if (bytesRead == 0) {
		// Client closed connection
		Logger::debug << "Client closed connection (fd: " << _fd << ")" << std::endl;
		_shouldClose = true;
		return false;
	}

	// Append to request buffer
	buffer[bytesRead] = '\0';
	_requestBuffer.append(buffer, bytesRead);
	updateActivity();

	Logger::debug << "Read " << bytesRead << " bytes from connection (fd: " << _fd
	              << "), total: " << _requestBuffer.size() << " bytes" << std::endl;

	// Check if we have received at least the headers (look for \r\n\r\n)
	size_t headerEndPos = _requestBuffer.find("\r\n\r\n");
	if (headerEndPos != std::string::npos) {
		// Headers received, now check if we need to wait for body
		size_t bodyStartPos = headerEndPos + 4; // +4 for "\r\n\r\n"

		// Extract headers to check Content-Length
		std::string headersOnly = _requestBuffer.substr(0, headerEndPos);
		size_t contentLength = 0;
		bool hasContentLength = false;

		// Parse Content-Length from headers
		size_t clPos = headersOnly.find("Content-Length:");
		if (clPos == std::string::npos) {
			clPos = headersOnly.find("content-length:");
		}
		if (clPos != std::string::npos) {
			size_t lineEnd = headersOnly.find("\r\n", clPos);
			if (lineEnd != std::string::npos) {
				std::string clLine = headersOnly.substr(clPos, lineEnd - clPos);
				size_t colonPos = clLine.find(':');
				if (colonPos != std::string::npos) {
					std::string clValue = clLine.substr(colonPos + 1);
					// Trim whitespace
					while (!clValue.empty() && (clValue[0] == ' ' || clValue[0] == '\t')) {
						clValue = clValue.substr(1);
					}
					contentLength = static_cast<size_t>(atoi(clValue.c_str()));
					hasContentLength = true;
				}
			}
		}

		// Check against max body size limit FIRST
		if (hasContentLength && contentLength > _server->getMaxBodySize()) {
			Logger::warning << "Request body too large: " << contentLength
			                << " bytes (max: " << _server->getMaxBodySize() << " bytes)" << std::endl;
			HTTP::Response errorResp = HTTP::Response::errorResponse(413,
				"Request entity too large. Maximum allowed size is " +
				std::string(static_cast<std::ostringstream&>(std::ostringstream() << _server->getMaxBodySize()).str()) +
				" bytes.");
			_responseBuffer = errorResp.build();
			_responseOffset = 0;
			_state = WRITING_RESPONSE;
			_shouldClose = true;
			return true;
		}

		// Check if request buffer size already exceeds max (for safety)
		if (_requestBuffer.size() > _server->getMaxBodySize() + 8192) { // +8192 for headers overhead
			Logger::warning << "Request buffer too large: " << _requestBuffer.size() << " bytes" << std::endl;
			HTTP::Response errorResp = HTTP::Response::errorResponse(413, "Request entity too large");
			_responseBuffer = errorResp.build();
			_responseOffset = 0;
			_state = WRITING_RESPONSE;
			_shouldClose = true;
			return true;
		}

		// If we have Content-Length, check if body is complete
		bool bodyComplete = false;
		if (hasContentLength) {
			size_t bodyReceived = _requestBuffer.size() - bodyStartPos;
			Logger::debug << "Body progress: " << bodyReceived << "/" << contentLength << " bytes" << std::endl;
			bodyComplete = (bodyReceived >= contentLength);
		} else {
			// No Content-Length, assume body is complete (or not expected)
			bodyComplete = true;
		}

		// Only process if body is complete
		if (bodyComplete) {
			Logger::debug << "Complete request received (fd: " << _fd << ")" << std::endl;
			_state = PROCESSING;

			// Parse HTTP request
			HTTP::Request request;
			if (!request.parse(_requestBuffer)) {
				Logger::error << "Failed to parse HTTP request" << std::endl;
				HTTP::Response errorResp = HTTP::Response::errorResponse(400, "Bad Request");
				_responseBuffer = errorResp.build();
				_responseOffset = 0;
				_state = WRITING_RESPONSE;
				_shouldClose = true;
				return true;
			}

			// Debug: print request
			if (Logger::debug << "") {
				request.print();
			}

			// Handle request
			HTTP::RequestHandler handler(_server);
			HTTP::Response response = handler.handle(request);

			// Build response
			_responseBuffer = response.build();
			_responseOffset = 0;
			_state = WRITING_RESPONSE;
			// Don't set _shouldClose here - let writeResponse handle it
		} else {
			// Body not complete yet, keep reading
			Logger::debug << "Waiting for more body data (fd: " << _fd << ")" << std::endl;
		}
	}

	return true;
}

bool Connection::writeResponse() {
	if (_responseBuffer.empty() || _responseOffset >= _responseBuffer.size()) {
		// Nothing to write or already written everything
		_shouldClose = true;
		return true;
	}

	const char* data = _responseBuffer.c_str() + _responseOffset;
	size_t remaining = _responseBuffer.size() - _responseOffset;

	ssize_t bytesWritten = send(_fd, data, remaining, 0);

	if (bytesWritten < 0) {
		// Non-blocking socket: would block means socket not ready
		// Don't check errno - just return success and try again later
		return true; // Not an error for non-blocking sockets
	}

	_responseOffset += bytesWritten;
	updateActivity();

	Logger::debug << "Wrote " << bytesWritten << " bytes to connection (fd: " << _fd
	              << "), total: " << _responseOffset << "/" << _responseBuffer.size()
	              << " bytes" << std::endl;

	// Check if response is complete
	if (_responseOffset >= _responseBuffer.size()) {
		Logger::info << "Response complete (fd: " << _fd << ")" << std::endl;
		_shouldClose = true;
		_state = CLOSING;
	}

	return true;
}

// State management
Connection::State Connection::getState() const {
	return _state;
}

void Connection::setState(State state) {
	_state = state;
}

// Getters
int Connection::getFd() const {
	return _fd;
}

const std::string& Connection::getClientHost() const {
	return _clientHost;
}

int Connection::getClientPort() const {
	return _clientPort;
}

time_t Connection::getLastActivity() const {
	return _lastActivity;
}

bool Connection::shouldClose() const {
	return _shouldClose;
}

// Timeout check
bool Connection::isTimedOut(time_t timeout) const {
	return (std::time(NULL) - _lastActivity) > timeout;
}

// Buffer management
const std::string& Connection::getRequestBuffer() const {
	return _requestBuffer;
}

void Connection::clearRequestBuffer() {
	_requestBuffer.clear();
}

// Helper methods
void Connection::updateActivity() {
	_lastActivity = std::time(NULL);
}

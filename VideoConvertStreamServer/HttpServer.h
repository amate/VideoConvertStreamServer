#pragma once

#include <memory>
#include <thread>
#include <string>
#include <functional>
#include <boost\asio.hpp>

class HttpServer
{
public:
	static std::shared_ptr<HttpServer> RunHttpServer();

	void SetCallback(std::function<void(int, std::wstring)> func) { m_funcCallback = func; }

	void StopHttpServer();

private:

	HttpServer();

	std::thread	m_serverThread;
	boost::asio::io_service m_ioService;
	std::function<void (int, std::wstring)> m_funcCallback;
};


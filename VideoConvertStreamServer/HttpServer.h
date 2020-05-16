#pragma once

#include <memory>
#include <thread>
#include <string>
#include <boost\asio.hpp>

class HttpServer
{
public:
	static std::shared_ptr<HttpServer> RunHttpServer();

	void StopHttpServer();

private:
	HttpServer();

	std::thread	m_serverThread;
	boost::asio::io_service m_ioService;
};


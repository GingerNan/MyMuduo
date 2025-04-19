#include "TcpServer.h"
#include "Logger.h"

#include <functional>

static EventLoop* CheckLoopNotNull(EventLoop* loop)
{
	if (loop == nullptr)
		LOG_FATAL("%s:%s:%d mainLop is null!", __FILE__, __FUNCTION__, __LINE__);
	return loop;
}

TcpServer::TcpServer(EventLoop* loop, const InetAddress& listenAddr, const std::string& nameArg, Option option)
	: loop_(CheckLoopNotNull(loop))
	, ipPort_(listenAddr.toIpPort())
	, name_(nameArg)
	, acceptor_(new Acceptor(loop, listenAddr, option == KReusePort))
	, threadPool_(new EventLoopThreadPool(loop, name_))
	, connectionCallback_()
	, messageCallback_()
	, nextConnId_(1)
{
	// 当有新用户连接时，会执行TcpServer::newConnection
	acceptor_->setNewConnectionCallback(
		std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer()
{
}

void TcpServer::setThreadNum(int numThreads)
{
	threadPool_->setThreadNum(numThreads);
}

void TcpServer::start()
{
	if (started_ ++ == 0)	// 防止一个TcpServer对象被start多次
	{
		threadPool_->start(threadInitCallback_);	// 启动底层的loop线程池
		loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
	}
}

void TcpServer::newConnection(int sockfd, const InetAddress& perrAddr)
{

}

void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{
}

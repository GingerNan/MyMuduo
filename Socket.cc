#include "Socket.h"
#include "Logger.h"
#include "InetAddress.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>


Socket::~Socket()
{
	close(sockfd_);
}

void Socket::bindAddress(const InetAddress& localaddr)
{
	if (0 != ::bind(sockfd_, (sockaddr*)localaddr.getSockAddr(), sizeof(sockaddr_in)))
	{
		LOG_FATAL("bind sockfd:%d fail\n", sockfd_);
	}
}

void Socket::listen()
{
	if (0 != ::listen(sockfd_, 1024))
	{
		LOG_FATAL("bind sockfd:%d fail\n", sockfd_);
	}
}

int Socket::accpet(InetAddress* perraddr)
{
	sockaddr_in addr;
	socklen_t len;
	bzero(&addr, sizeof addr);
	int connfd = ::accept(sockfd_, (sockaddr*)&addr, &len);
	if (connfd >= 0)
	{
		perraddr->setSockAddr(addr);
	}
	return connfd;
}

void Socket::shutdownWrite()
{
	if (::shutdown(sockfd_, SHUT_WR) < 0)
	{
		LOG_ERROR("Sockets::shutdownWrite error");
	}
}

void Socket::setTcpNoDelay(bool on)
{
	int optval = on ? 1 : 0;
	::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY,
		&optval, static_cast<socklen_t>(sizeof optval));
}

void Socket::setReuseAddr(bool on)
{
	int optval = on ? 1 : 0;
	::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR,
		&optval, static_cast<socklen_t>(sizeof optval));
}

void Socket::setReusePort(bool on)
{
	int optval = on ? 1 : 0;
	::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT,
		&optval, static_cast<socklen_t>(sizeof optval));
}

void Socket::setKeepAlive(bool on)
{
	int optval = on ? 1 : 0;
	::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE,
		&optval, static_cast<socklen_t>(sizeof optval));
}

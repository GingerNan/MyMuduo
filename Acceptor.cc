#include "Acceptor.h"
#include "InetAddress.h"
#include "Logger.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <error.h>

static int createNonblocking()
{
	int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
	if (sockfd < 0)
	{
		LOG_FATAL("%s:%s:%d listen socket create err:%d\n", __FILE__, __FUNCTION__, __LINE__, errno);
	}
	return sockfd;
}

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport)
	: loop_(loop)
	, acceptSocket_(createNonblocking())
	, acceptChannel_(loop, acceptSocket_.fd())
	, listenning_(false)
{
	acceptSocket_.setReuseAddr(true);
	acceptSocket_.setReusePort(true);
	acceptSocket_.bindAddress(listenAddr);
	// TcpServer:;start() Accpetor.listen �����û������ӣ�Ҫִ��һ���ص� (connfd => channel => subloop)
	// baseLoop => accpetChannel_(listenfd) =>
	acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor()
{
	acceptChannel_.disableAll();
	acceptChannel_.remove();
}

void Acceptor::listen()
{
	listenning_ = true;
	acceptSocket_.listen();	// listen
	acceptChannel_.enableReading();	// acceptCahnnel_ => Pooler
}

// listenfd��ʱ�䷢���ˣ����������û�����
void Acceptor::handleRead()
{
	InetAddress perrAddr;
	int connfd = acceptSocket_.accpet(&perrAddr);
	if (connfd > 0)
	{
		if (newConnectionCallback_)
		{
			newConnectionCallback_(connfd, perrAddr);	// ��ѯ�ҵ�subloop�����ѣ��ַ���ǰ���¿ͻ��˵�channel
		}
		else
		{
			::close(connfd);
		}
	}
	else
	{
		LOG_FATAL("%s:%s:%d accpet err:%d\n", __FILE__, __FUNCTION__, __LINE__, errno);
		if (errno == EMFILE)
		{
			LOG_FATAL("%s:%s:%d accpet err:%d\n", __FILE__, __FUNCTION__, __LINE__, errno);
		}
	}
}

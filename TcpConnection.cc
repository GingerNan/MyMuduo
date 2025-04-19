#include "TcpConnection.h"
#include "Logger.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"

#include <errno.h>
#include <functional>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>
#include <string>

static EventLoop* CheckLoopNotNull(EventLoop* loop)
{
	if (loop == nullptr)
		LOG_FATAL("%s:%s:%d mainLop is null!", __FILE__, __FUNCTION__, __LINE__);
	return loop;
}

TcpConnection::TcpConnection(EventLoop* loop, const std::string& nameArg, int sockfd, const InetAddress& localAddr, const InetAddress& peerAddr)
	: loop_(CheckLoopNotNull(loop))
	, name_(nameArg)
	, state_(kConnecting)
	, reading_(true)
	, socket_(new Socket(sockfd))
	, channel_(new Channel(loop, sockfd))
	, localAddr_(localAddr)
	, peerAddr_(peerAddr)
	, highWaterMark_(64 * 1024 * 1024)	// 64M
{
	// �����channel������Ӧ�Ļص�������poller��channel֪ͨ����Ȥ���¼� ������ channel�������Ӧ�Ĳ�������
	channel_->setReadCallback(
		std::bind(&TcpConnection::handlerRead, this, std::placeholders::_1));
	channel_->setWriteCallback(
		std::bind(&TcpConnection::handlerWrite, this));
	channel_->setCloseCallback(
		std::bind(&TcpConnection::handlerClose, this));
	channel_->setErrorCallback(
		std::bind(&TcpConnection::handlerClose, this));

	LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n", name_.c_str(), sockfd);
	socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
	LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d\n", name_.c_str(), channel_->fd(), state_);
}

void TcpConnection::send(const std::string& buf)
{
	if (state_ == kConnected)
	{
		if (loop_->isInLoopThread())
		{
			sendInLoop(buf.c_str(), buf.size());
		}
		else
		{
			loop_->runInLoop(std::bind(&TcpConnection::sendInLoop, this, buf.c_str(), buf.size()));
		}
	}
}

void TcpConnection::shutdown()
{
}

void TcpConnection::connectEstablished()
{
}

void TcpConnection::connectDestroyed()
{
}

void TcpConnection::handlerRead(Timestamp receiveTime)
{
	int saveErrno = 0;
	ssize_t n = inputBuffer_.readFd(channel_->fd(), &saveErrno);
	if (n > 0)
	{
		// �ѽ������ӵ��û����пɶ��¼������ˣ������û�����Ļص�����onMessage
		messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
	}
	else if (n == 0)
	{
		handlerClose();
	}
	else
	{
		errno = saveErrno;
		LOG_ERROR("TcpConnection::handlerRead");
		handlerError();
	}
}

void TcpConnection::handlerWrite()
{
	int saveErrno = 0;
	if (channel_->isWriting())
	{
		ssize_t n = outputBuffer_.writeFd(channel_->fd(), &saveErrno);
		if (n > 0)
		{
			outputBuffer_.retrieve(n);
			if (outputBuffer_.readableBytes() == 0)
			{
				channel_->disableWriting();
				if (writeCompleteCallback_)
				{
					// ����loop_��Ӧ��thread�̣߳�ִ�лص�
					loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
				}

				if (state_ == kDisconnecting)
				{
					shutdownInLoop();
				}
			}
		}
		else
		{
			LOG_ERROR("TcpConnection::handleWrite");
		}
	}
	else
	{
		LOG_ERROR("TcpConnection fd=%d is down, no more writing", channel_->fd());
	}
}

void TcpConnection::handlerClose()
{
	LOG_INFO("fd=%d state=%d\n", channel_->fd(), state_);
	setState(kDisconnected);
	channel_->disableAll();

	TcpConnectionPtr guardThis(shared_from_this());
	connectionCallback_(guardThis);	// ִ�����ӹرյĻص�
	closeCallback_(guardThis);		// �ر����ӵĻص�
}

void TcpConnection::handlerError()
{
	int optval;
	socklen_t optlen = sizeof optval;
	int err = 0;
	if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen))
	{
		err = optval;
	}
	else
	{
		err = optval;
	}
	LOG_ERROR("TcpConnection::handlerError [%s] - SO_ERROR = %d\n", name_.c_str(), err);
}

/**
 * �������� Ӧ��д�Ŀ飬���ں˷�������������Ҫ�Ѵ���������д�뻺����������������ˮλ�ص� 
 */
void TcpConnection::sendInLoop(const void* data, size_t len)
{
	ssize_t nwrote = 0;
	size_t remaining = len;
	bool faultError = false;
	
	// ֮ǰ���ù���connection��shutdown�������ڽ��з�����
	if (state_ == kDisconnected)
	{
		LOG_ERROR("disconnected, give up writing!");
		return;
	}

	// ��ʾchannel_��һ�ο�ʼд���ݣ����һ�����û�д���������
	if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
	{
		nwrote = ::write(channel_->fd(), data, len);
		if (nwrote >= 0)
		{
			remaining = len - nwrote;
			if (remaining == 0 && writeCompleteCallback_)
			{
				// ��Ȼ����������ȫ��������ɣ��Ͳ����ٸ�channel����epolloout�¼���
				loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
			}
		}
		else // nwrote < 0
		{
			nwrote = 0;
			if (errno != EWOULDBLOCK)
			{
				LOG_ERROR("TcpConnection::sendInLoop");
				if (errno == EPIPE || errno == ECONNREFUSED)	// SIGPIPE RESET
				{
					faultError = true;;
				}
			}
		}
	}

	// ˵����ǰ��һ��write����û�а�����ȫ�����ͳ�ȥ��ʣ���������Ҫ���浽���������У�Ȼ���channel
	// ע��epollout�¼���poller����tcp�ķ��ͻ������пռ䣬��֪ͨ��Ӧ��sock-channel������handleWrite�ص�����
	if (!faultError && remaining > 0)
	{

	}

}

void TcpConnection::shutdownInLoop()
{
}

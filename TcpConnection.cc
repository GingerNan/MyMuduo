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
	// 下面给channel设置相应的回调函数，poller给channel通知感兴趣的事件 发生了 channel会调用相应的操作函数
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
		// 已建立连接的用户，有可读事件发生了，调用用户传入的回调操作onMessage
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
					// 唤醒loop_对应的thread线程，执行回调
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
	connectionCallback_(guardThis);	// 执行连接关闭的回调
	closeCallback_(guardThis);		// 关闭连接的回调
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
 * 发送数据 应用写的块，而内核发送数据慢，需要把带发送数据写入缓冲区，而且设置了水位回调 
 */
void TcpConnection::sendInLoop(const void* data, size_t len)
{
	ssize_t nwrote = 0;
	size_t remaining = len;
	bool faultError = false;
	
	// 之前调用过该connection的shutdown，不能在进行发送了
	if (state_ == kDisconnected)
	{
		LOG_ERROR("disconnected, give up writing!");
		return;
	}

	// 表示channel_第一次开始写数据，而且缓冲区没有待发送数据
	if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
	{
		nwrote = ::write(channel_->fd(), data, len);
		if (nwrote >= 0)
		{
			remaining = len - nwrote;
			if (remaining == 0 && writeCompleteCallback_)
			{
				// 既然在这里数据全部发送完成，就不用再给channel设置epolloout事件了
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

	// 说明当前这一次write，并没有把数据全部发送出去，剩余的数据需要保存到缓冲区当中，然后给channel
	// 注册epollout事件，poller发现tcp的发送缓冲区有空间，会通知相应的sock-channel，调用handleWrite回调方法
	if (!faultError && remaining > 0)
	{

	}

}

void TcpConnection::shutdownInLoop()
{
}

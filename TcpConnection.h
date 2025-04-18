#pragma once
#include "noncopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "Timestamp.h"

#include <memory>
#include <string>
#include <atomic>

class Channel;
class EventLoop;
class Socket;

/**
 * TcpServer => Acceptor => 有一个新用户连接，通过accept函数拿到connfd
 *
 * => TcpConnection 设置回调 => Channel => Poller => Channel的回调操作
 */
class TcpConnection : noncopyable,
	public std::enable_shared_from_this<TcpConnection>
{
public:
	TcpConnection(EventLoop* loop,
		const std::string& nameArg,
		int sockfd,
		const InetAddress& localAddr,
		const InetAddress& peerAddr);
	~TcpConnection();

	EventLoop* getLoop() const { return loop_; }
	const std::string& name() const { return name_; }
	const InetAddress& localAddress() const { return localAddr_; }
	const InetAddress& peerAddress() const { return peerAddr_; }

	bool connected() const { return state_ == kConnected; }

	// 发送数据
	void send(const std::string& buf);
	// 关闭连接
	void shutdown();

	void setCloseCallback(const CloseCallback& cb) { closeCallback_ = cb; }
	void setConnectionCallback(const ConnectionCallback& cb) { connectionCallback_ = cb; }
	void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }
	void setWriteCompleteCallback(const WriteCompleteCallback& cb) { writeCompleteCallback_ = cb; }
	void setHighWaterMarkCallback(const HighWaterMarkCallback& cb) { highWaterMarkCallback_ = cb; }

	// 连接建立
	void connectEstablished();
	// 连接销毁
	void connectDestroyed();
private:

	void handlerRead(Timestamp receiveTime);
	void handlerWrite();
	void handlerClose();
	void handlerError();

	void sendInLoop(const void* data, size_t len);
	void shutdownInLoop();
private:
	enum StateE { kDisconnected, kConnecting, kConnected, kDisconnecting };

	void setState(StateE state) { state_ = state; }

	EventLoop* loop_;	// 这里绝对不是baseloop，因为TcpConnection都是在subloop里面管理的.
	const std::string name_;
	std::atomic_int state_;
	bool reading_;

	// 这里和Accpetor类似 Acceptor=> mainLoop TcpConnection => subLoop
	std::unique_ptr<Socket> socket_;
	std::unique_ptr<Channel> channel_;

	const InetAddress localAddr_;
	const InetAddress peerAddr_;

	ConnectionCallback connectionCallback_;
	MessageCallback messageCallback_;
	WriteCompleteCallback writeCompleteCallback_;
	CloseCallback closeCallback_;

	HighWaterMarkCallback highWaterMarkCallback_;
	size_t highWaterMark_;

	Buffer inputBuffer_;	// 接受数据的缓冲区
	Buffer outputBuffer_;	// 发送数据的缓冲区

};
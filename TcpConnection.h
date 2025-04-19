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
 * TcpServer => Acceptor => ��һ�����û����ӣ�ͨ��accept�����õ�connfd
 *
 * => TcpConnection ���ûص� => Channel => Poller => Channel�Ļص�����
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

	// ��������
	void send(const std::string& buf);
	// �ر�����
	void shutdown();

	void setCloseCallback(const CloseCallback& cb) { closeCallback_ = cb; }
	void setConnectionCallback(const ConnectionCallback& cb) { connectionCallback_ = cb; }
	void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }
	void setWriteCompleteCallback(const WriteCompleteCallback& cb) { writeCompleteCallback_ = cb; }
	void setHighWaterMarkCallback(const HighWaterMarkCallback& cb) { highWaterMarkCallback_ = cb; }

	// ���ӽ���
	void connectEstablished();
	// ��������
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

	EventLoop* loop_;	// ������Բ���baseloop����ΪTcpConnection������subloop��������.
	const std::string name_;
	std::atomic_int state_;
	bool reading_;

	// �����Accpetor���� Acceptor=> mainLoop TcpConnection => subLoop
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

	Buffer inputBuffer_;	// �������ݵĻ�����
	Buffer outputBuffer_;	// �������ݵĻ�����

};
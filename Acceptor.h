#pragma once
#include "noncopyable.h"
#include "Socket.h"
#include "Channel.h"

#include <functional>

class EventLoop;
class InetAddress;

class Acceptor : noncopyable
{
public:
	using NewConnectionCallBack = std::function<void(int sockfd, const InetAddress&)>;
	Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport);
	~Acceptor();

	void setNewConnectionCallback(const NewConnectionCallBack& cb)
	{ newConnectionCallback_ = cb; }

	bool listenning() const { return listenning_; }
	void listen();
private:
	void handleRead();

private:
	EventLoop* loop_;	// Acceptor用的就是用户定义的那个baseLoop，也称作mainLoop
	Socket acceptSocket_;
	Channel acceptChannel_;
	NewConnectionCallBack newConnectionCallback_;
	bool listenning_;
};
#pragma once

/**
 * �û�ʹ��muduo��д���������� 
 */
#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include "EventLoopThreadPool.h"
#include "Callbacks.h"

#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include <unordered_map>

// ����ķ��������ʹ�õ���
class TcpServer : noncopyable
{
public:
	using ThreadInitCallback = std::function<void(EventLoop*)>;

	enum Option
	{
		KNoReusePort,
		KReusePort,
	};

	TcpServer(EventLoop* loop,
		const InetAddress& listenAddr,
		const std::string& nameArg,
		Option option = KNoReusePort);
	~TcpServer();

	void setThreadInitCallback(const ThreadInitCallback& cb) { threadInitCallback_ = cb; }
	void setConnectionCallback(const ConnectionCallback& cb) { connectionCallback_ = cb; }
	void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }
	void setWriteCompleteCallback(const WriteCompleteCallback& cb) { writeCompleteCallback_ = cb; }

	// ���õײ�subloop�ĸ���
	void setThreadNum(int numThreads);

	// ��������������
	void start();
private:
	void newConnection(int sockfd, const InetAddress& perrAddr);
	void removeConnection(const TcpConnectionPtr& conn);
	void removeConnectionInLoop(const TcpConnectionPtr& conn);
private:
	using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

	EventLoop* loop_;	// baseLoop �û��Զ����loop
	const std::string ipPort_;
	const std::string name_;

	std::unique_ptr<Acceptor> acceptor_;	// ������mainloop��������Ǽ����������¼�
	std::shared_ptr<EventLoopThreadPool> threadPool_;	// one loop per thread

	ConnectionCallback connectionCallback_;			// ��������ʱ�Ļص�
	MessageCallback messageCallback_;				// �ж�д��Ϣʱ�Ļص�
	WriteCompleteCallback writeCompleteCallback_;	// ��Ϣ�������֮��Ļص�
	ThreadInitCallback threadInitCallback_;			// loop�̳߳�ʼ���Ļص�

	std::atomic_int started_;
	int nextConnId_;
	ConnectionMap connections_;	//�������е�����
};
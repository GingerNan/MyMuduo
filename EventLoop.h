#pragma once
#include "noncopyable.h"
#include "functional"
#include "CurrentThread.h"
#include "Timestamp.h"

#include <vector>
#include <atomic>
#include <memory>
#include <mutex>

class Channel;
class Poller;

// �¼�ѭ���� ��Ҫ������������ģ�� Channel Poller(epoll�ĳ���)
class EventLoop : noncopyable
{
public:
	using Functor = std::function<void()>;

	EventLoop();
	~EventLoop();

	// �����¼�ѭ��
	void loop();
	// �˳��¼�ѭ��
	void quit();

	Timestamp pollReturnTime() const { return pollReturnTime_; }

	// �ڵ�ǰloop��ִ��
	void runInLoop(Functor callback);
	// ��cb��������У�����loop���ڵ��̣߳�ִ��cb
	void queueInLoop(Functor callback);

	// ����loop���ڵ��߳�
	void wakeup();

	// EventLoop�ķ��� => Poller�ķ���
	void updateChannel(Channel* channel);
	void removeChannel(Channel* channel);
	bool hasChannel(Channel* channel);

	// �ж�EventLoop�����Ƿ����Լ����߳�����
	bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }
private:
	void handleRead();	// waked up

	void doPendingFunctors();
private:
	using ChannelList = std::vector<Channel*>;

	std::atomic_bool looping_;	// ԭ�Ӳ�����ͨ��CASʵ�ֵ�
	std::atomic_bool quit_;		// ��־�Ƴ�loopѭ��

	const pid_t threadId_;		// ��¼��ǰloop�����̵߳�id

	Timestamp pollReturnTime_;	// poller���ط����¼���channels��ʱ���
	std::unique_ptr<Poller> poller_;

	std::unique_ptr<Channel> wakeupChannel_;
	int wakeupFd_;				// ��Ҫ���ã���mainLoop��ȡһ�����û���channel��ͨ����ѯ�㷨ѡ��
								//һ��subloop��ͨ���ó�Ա����subloop����channel����man eventfd��

	ChannelList activeChannels_;

	std::atomic_bool callingPendingFunctors_;	// ��ʶ��ǰloop�Ƿ�����Ҫִ�еĻص�����
	std::mutex mutex_;							// ������������������vecotr�������̰߳�ȫ����
	std::vector<Functor> pendingFunctors_;		// �洢loop��Ҫִ�е����лص�����
};
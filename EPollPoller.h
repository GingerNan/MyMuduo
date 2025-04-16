#pragma once

#include "Poller.h"
#include "Timestamp.h"

#include <sys/epoll.h>
#include <vector>

class Channel;
/**
 * epoll的使用
 * -> epoll_create
 * -> epoll_ctl		add/mod/del
 * -> epoll_wait
 */
class EPollPoller : public Poller
{
public:
	EPollPoller(EventLoop* loop);
	~EPollPoller() override;

	// 重写基类Poller的抽象方法
	Timestamp poll(int timeoutMs, ChannelList* activeChannels) override;
	void updateChannel(Channel* channel) override;
	void removeChannel(Channel* channel) override;
private:
	/**
	 * fill 填写
	 * active 活跃
	 */
	// 填写活跃的连接
	void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;
	// 更新channel通道
	void update(int operatrion, Channel* channel);

private:
	static const int kInitEventListSize = 16;

	using EventList = std::vector<epoll_event>;

	int epollfd_;
	EventList events_;
};
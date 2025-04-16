#pragma once

#include "Poller.h"
#include "Timestamp.h"

#include <sys/epoll.h>
#include <vector>

class Channel;
/**
 * epoll��ʹ��
 * -> epoll_create
 * -> epoll_ctl		add/mod/del
 * -> epoll_wait
 */
class EPollPoller : public Poller
{
public:
	EPollPoller(EventLoop* loop);
	~EPollPoller() override;

	// ��д����Poller�ĳ��󷽷�
	Timestamp poll(int timeoutMs, ChannelList* activeChannels) override;
	void updateChannel(Channel* channel) override;
	void removeChannel(Channel* channel) override;
private:
	/**
	 * fill ��д
	 * active ��Ծ
	 */
	// ��д��Ծ������
	void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;
	// ����channelͨ��
	void update(int operatrion, Channel* channel);

private:
	static const int kInitEventListSize = 16;

	using EventList = std::vector<epoll_event>;

	int epollfd_;
	EventList events_;
};
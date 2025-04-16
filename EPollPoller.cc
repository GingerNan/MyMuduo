#include "EPollPoller.h"
#include "Logger.h"
#include "Channel.h"
#include <unistd.h>
#include <errno.h>
#include <strings.h>

// channelδ��ӵ�poller��
const int kNew = -1;	// channel�ĳ�Աindex_ = -1
// channel����ӵ�poller��
const int kAdded = 1;
// channel��poller��ɾ��
const int kDeleted = 2;

EPollPoller::EPollPoller(EventLoop* loop)
	:Poller(loop)
	, epollfd_(::epoll_create1(EPOLL_CLOEXEC))
	, events_(kInitEventListSize) // vector<epoll_event>
{
	if (epollfd_ < 0)
		LOG_FATAL("epoll_create error:%d\n", errno);
}

EPollPoller::~EPollPoller()
{
	close(epollfd_);
}

Timestamp EPollPoller::poll(int timeoutMs, ChannelList* activeChannels)
{
	// ʵ����Ӧ����LOG_DEBUG�����־��Ϊ����
	LOG_INFO("func=%s => fd total count:%d\n",__FUNCTION__, channels_.size());

	int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
	int saveErrno = errno;
	Timestamp now(Timestamp::now());

	if (numEvents > 0)
	{
		LOG_INFO("%d events happened\n", numEvents);
		fillActiveChannels(numEvents, activeChannels);
		if (numEvents == static_cast<int>(events_.size()))
		{
			events_.reserve(events_.size() * 2);
		}
	}
	else if (numEvents == 0)	// ��ʱ
	{
		LOG_DEBUG("%s timeout! \n", __FUNCTION__);
	}
	else
	{
		if (saveErrno != EINTR)
		{
			errno = saveErrno;
			LOG_ERROR("EPollPoller::poll() err!");
		}
	}
	return now;
}

// channel update remove => EventLoop updateChannel removeChannel => Poller updateChannel removeChannel
/**
 *				EventLoop	=>	poller.poll
 *		ChannelList		poller
 *						channelMap	<fd, channel*>
 */
void EPollPoller::updateChannel(Channel* channel)
{
	const int index = channel->index();
	LOG_INFO("func=%s => fd=%d events=%d index=%d \n", __FUNCTION__, channel->fd(), channel->events(), channel->index());
	if (index == kNew || index == kDeleted)
	{
		if (index == kNew)
		{
			int fd = channel->fd();
			channels_[fd] = channel;
		}
		channel->set_index(kAdded);
		update(EPOLL_CTL_ADD, channel);
	}
	else  // channel�Ѿ���poller��ע�����
	{
		if (channel->isNoneEvent())
		{
			update(EPOLL_CTL_DEL, channel);
			channel->set_index(kDeleted);
		}
		else
		{
			update(EPOLL_CTL_MOD, channel);
		}
	}
}

// ��poller��ɾ��channel
void EPollPoller::removeChannel(Channel* channel)
{
	int fd = channel->fd();
	channels_.erase(fd);

	LOG_INFO("func=%s => fd=%d\n", __FUNCTION__, fd);

	int index = channel->index();
	if (index == kAdded)
		update(EPOLL_CTL_DEL, channel);
	channel->set_index(kNew);
}

void EPollPoller::fillActiveChannels(int numEvents, ChannelList* activeChannels) const
{
	for (int i = 0; i < numEvents; ++i)
	{
		Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
		
		channel->set_revents(events_[i].events);
		activeChannels->push_back(channel); // EventLoop���õ�������poller�������ص����з����¼���channel�б���
	}
}

// ����channelͨ�� epoll_ctl add/mod/del
void EPollPoller::update(int operatrion, Channel* channel)
{
	epoll_event event;
	bzero(&event, sizeof(event));
	event.events = channel->events();
	event.data.ptr = channel;
	int fd = channel->fd();

	if (::epoll_ctl(epollfd_, operatrion, fd, &event) < 0)
	{
		if (operatrion == EPOLL_CTL_DEL)
			LOG_ERROR("epoll_ctl del error:%d\n", errno);
		else
			LOG_FATAL("epoll_ctl add/mod error:%d\n", errno);
	}
}

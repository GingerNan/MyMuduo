#pragma once
#include "noncopyable.h"
#include "Timestamp.h"

#include <vector>
#include <unordered_map>

class Channel;
class EventLoop;

// muduo���ж�·�¼��ַ����ĺ���IO����ģ��
class Poller
{
public:
	using ChannelList = std::vector<Channel*>;

	Poller(EventLoop* loop);
	virtual ~Poller() = default;

	// ������IO���ñ���ͳһ�Ľӿ�
	virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels) = 0;
	virtual void updateChannel(Channel* channel) = 0;
	virtual void removeChannel(Channel* channel) = 0;

	// �жϲ���channel�Ƿ��ڵ�ǰPoller����
	bool hasChannel(Channel* channel) const;

	// EventLoop����ͨ���ýӿ�Ĭ�ϵ�IO���õľ���ʵ��
	static Poller* newDefaultPoller(EventLoop* loop);
protected:
	// map��key: sockfd   value: sockfd������channelͨ������
	using ChannelMap = std::unordered_map<int, Channel*>;
	ChannelMap channels_;
private:
	EventLoop* ownerLoop_;	//����Poller�������¼�ѭ��EventLoop
};
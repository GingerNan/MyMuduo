#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

// ��ֹһ���̴߳������EventLoop
__thread EventLoop* t_loopInThisThread = nullptr;

// ����Ĭ�ϵ�Poller IO���ýӿڵĳ�ʱʱ��
const int kPollTimeMs = 10000;

// ����wakeupfd������notify����subReactor��������channel
int createEventfd()
{
	int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
	if (evtfd < 0)
	{
		LOG_FATAL("eventfd error:%d\n", errno);
	}
	return evtfd;
}

EventLoop::EventLoop()
	: looping_(false)
	, quit_(false)
	, callingPendingFunctors_(false)
	, threadId_(CurrentThread::tid())
	, poller_(Poller::newDefaultPoller(this))
	, wakeupFd_(createEventfd())
	, wakeupChannel_(new Channel(this, wakeupFd_))
{
	LOG_DEBUG("EventLoop created %p in thread %d\n", this, threadId_);
	if (t_loopInThisThread)
	{
		LOG_FATAL("Another EventLoop %p exists in this thread %d\n", t_loopInThisThread, threadId_);
	}
	else
	{
		t_loopInThisThread = this;
	}

	// ����wakeupfd���¼������Լ������¼���Ļص�����
	wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
	// ÿһ��eventloop��������wakeupchannel��EPOLLIN���¼�
	wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
	wakeupChannel_->disableAll();
	wakeupChannel_->remove();
	::close(wakeupFd_);
	t_loopInThisThread = nullptr;
}

void EventLoop::loop()
{
	looping_ = true;
	quit_ = false;
	
	LOG_INFO("EventLoop %p start looping", this);
	
	while (!quit_)
	{
		activeChannels_.clear();
		// ��������fd		һ����client��fd��һ��wakeupfd
		pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
		for (Channel* channel : activeChannels_)
		{
			// Poller������Щchannel�����¼��ˣ�Ȼ���ϱ���EventLoop��֪ͨchannel������Ӧ���¼�
			channel->handleEvent(pollReturnTime_);
		}
		// ִ�е�ǰEventLoop�¼�ѭ����Ҫ����Ļص�����
		/**
		 * IO�߳� mainLoop accept fd ��= channel subloop 
		 * mainLoop ����ע��һ���ص�cb����Ҫsubloopִ�У�	wakeup subloop��ִ������ķ�����ִ��֮ǰmainloopע���cb����
		 */
		doPendingFunctors();
	}

	LOG_INFO("EventLoop %p stop looping.\n", this);
	looping_ = false;
}

// �˳��¼�ѭ�� 1.loop���Լ����߳��е���quit
/**
 *					mainLoop
 * 
 *												no =============================== ������-�����ߵ��̰߳�ȫ�Ķ���
 *	
 *	subLoop1		subLoop2		subLoop3
 */
void EventLoop::quit()
{
	quit_ = true;

	// ���ʵ�������߳��У�����quit		��һ��subloop(work)�У�������mainloop(IO)��quit
	if (!isInLoopThread())	
	{
		wakeup();
	}
}

// �ڵ�ǰloop��ִ��
void EventLoop::runInLoop(Functor callback)
{
	if (isInLoopThread())	// �ڵ�ǰ��loop�߳��У�ִ��cb
	{
		callback();
	}
	else // �ڷǵ�ǰloop�߳���ִ��cb������Ҫ����loop�����̣߳�ִ��cb
	{
		queueInLoop(callback);
	}
}

// ��cb��������У�����loop���ڵ��̣߳�ִ��cb
void EventLoop::queueInLoop(Functor callback)
{
	{
		std::unique_lock<std::mutex> lock(mutex_);
		pendingFunctors_.emplace_back(callback);
	}

	// ������Ӧ�ģ���Ҫִ������ص�������loop���߳���
	// || callingPendingFunctors_����˼�ǣ���ǰloop����ִ�лص�������loop�������µĻص���
	if (!isInLoopThread() || callingPendingFunctors_)
	{
		wakeup();	// ����loop�����߳�
	}
	
}

// ��wakeupfd_дһ�����ݣ�wakeupChannel�ͷ������¼�����ǰloop�߳̾ͻᱻ����
void EventLoop::wakeup()
{
	uint64_t one = 1;
	ssize_t n = write(wakeupFd_, &one, sizeof(one));
	if (n != sizeof(one))
	{
		LOG_ERROR("EventLoop::wakeup() writes %d bytes instead of 8", n);
	}
}

void EventLoop::updateChannel(Channel* channel)
{
	poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel)
{
	poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel* channel)
{
	return poller_->hasChannel(channel);
}

void EventLoop::handleRead()
{
	uint64_t one = 1;
	ssize_t n = read(wakeupFd_, &one, sizeof(one));
	if (n != sizeof(one))
	{
		LOG_ERROR("EventLoop::handleRead() reads %d bytes instead of 8", n);
	}
}

void EventLoop::doPendingFunctors()
{
	std::vector<Functor> functors;
	callingPendingFunctors_ = true;

	{
		std::unique_lock<std::mutex> lock(mutex_);
		functors.swap(pendingFunctors_);
	}

	for (const Functor& functor : functors)
	{
		functor();	// ִ�е�ǰloop��Ҫִ�еĻص�����
	}
	
	callingPendingFunctors_ = false;
}

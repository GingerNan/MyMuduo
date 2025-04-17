#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h>

std::atomic_int Thread::numCreated_(0);

Thread::Thread(ThreadFunc func, const std::string& name)
	: started_(false)
	, joined_(false)
	, tid_(0)
	, func_(std::move(func))
	, name_(name)
{
	setDefualtName();
}

Thread::~Thread()
{
	if (started_ && !joined_)
	{
		thread_->detach();	// thread���ṩ�����÷����̵߳ķ���
	}
}

void Thread::start()	// һ��Thread���󣬼�¼�ľ���һ�����̵߳���ϸ��Ϣ
{
	started_ = true;
	sem_t sem;
	sem_init(&sem, false, 0);
	// �����߳�
	thread_ = std::shared_ptr<std::thread>(new std::thread([&]() {
		// ��ȡ�̵߳�tidֵ
		tid_ = CurrentThread::tid();
		sem_post(&sem);
		// ����һ�����̣߳�ר��ִ�и��̺߳���
		func_();
	}));

	// �������ȴ���ȡ�����´������̵߳�tidֵ
	sem_wait(&sem);
}

void Thread::join()
{
	joined_ = true;
	thread_->join();
}

void Thread::setDefualtName()
{
	int num = ++numCreated_;
	if (name_.empty())
	{
		char buf[32] = { 0 };
		snprintf(buf, sizeof buf, "Thread%d", num);
		name_ = buf;
	}
}

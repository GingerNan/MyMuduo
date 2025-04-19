#include "Buffer.h"

#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

/**
 * ��fd�϶�ȡ���� Poller������LTģʽ
 * Buffer���������д�С�ģ����Ǵ�fd�϶����ݵ�ʱ��ȴ��֪��tcp�������յĴ�С
 */
ssize_t Buffer::readFd(int fd, int* saveErrno)
{
	char extrabuf[65536] = { 0 };	// ջ�ϵ��ڴ�ռ�
	struct iovec vec[2];
	const size_t writable = writableBytes();	// ����buffer�ײ㻺����ʣ��Ŀ�д�ռ��С
	vec[0].iov_base = begin() + writerIndex_;
	vec[0].iov_len = writable;

	vec[1].iov_base = extrabuf;
	vec[1].iov_len = sizeof extrabuf;

	const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
	const ssize_t n = ::readv(fd, vec, iovcnt);
	if (n < 0)
	{
		*saveErrno = errno;
	}
	else if (n <= writable)	// Buffer�Ŀ�д�������Ѿ����洢��������������
	{
		writerIndex_ += n;
	}
	else // extrabuf����Ҳд��������
	{
		writerIndex_ = buffer_.size();
		append(extrabuf, n - writable); // writerIndex_
	}

	return n;
}

ssize_t Buffer::writeFd(int fd, int* saveErrno)
{
	ssize_t n = ::write(fd, peek(), readableBytes());
	if (n < 0)
	{
		*saveErrno = errno;
	}
	return n;
}

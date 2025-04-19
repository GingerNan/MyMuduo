#pragma once
#include "noncopyable.h"

#include <vector>
#include <cstddef>
#include <string>
#include <algorithm>

/// A buffer class modeled after org.jboss.netty.buffer.ChannelBuffer
///
/// @code
/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size
/// @endcode
/// �����ײ�Ļ��������Ͷ���
class Buffer : noncopyable
{
public:
	static const size_t	kCheapPrepend = 8;
	static const size_t	kInitialSize = 1024;

	explicit Buffer(size_t initialSize = kInitialSize)
		: buffer_(kCheapPrepend + kInitialSize)
		, readerIndex_(kCheapPrepend)
		, writerIndex_(kCheapPrepend)
	{}

	size_t readableBytes() const { return writerIndex_ - readerIndex_; }
	size_t writableBytes() const { buffer_.size() - writerIndex_; }
	size_t prependableBytes() const { return readerIndex_; }

	// ���ػ������пɶ����ݵ���ʼ��ַ
	const char* peek() const { return begin() + readerIndex_; }

	// onMessage 
	void retrieve(size_t len)
	{
		if (len < readableBytes())
		{
			readerIndex_ += len;	// Ӧ��ֻ��ȡ�˿ɶ����������ݵ�һ���֣�����len���ȣ���ʣ�� readIndex_+=len -> writeIndex_
		}
		else
		{
			retrieveAll();
		}
	}

	void retrieveAll()
	{
		readerIndex_ = writerIndex_ = kCheapPrepend;
	}

	// ��onMessage�����ϱ���Buffer���ݣ�ת��string���͵����ݷ���
	std::string retrieveAllAsString()
	{
		return retrieveAsString(readableBytes()); //Ӧ�ÿɶ�ȡ���ݵĳ���
	}

	std::string retrieveAsString(size_t len)
	{
		std::string result(peek(), len);
		retrieve(len);	// ����һ��ѻ������пɶ������ݣ��Ѿ���ȡ����������϶�Ҫ�Ի��������и�λ����
		return result;
	}

	// buffer_size - writerIndex_
	void ensureWriteableByres(size_t len)
	{
		if (writableBytes() < len)
		{
			makeSpace(len);	//���ݺ���
		}
	}

	void append(const char* data, size_t len)
	{
		ensureWriteableByres(len);
		std::copy(data, data + len, beginWrite());
		writerIndex_ += len;
	}

	char* beginWrite() { return begin() + writerIndex_; }
	const char* beginWrite() const { return begin() + writerIndex_; }

	// ��fd�϶�ȡ����
	ssize_t readFd(int fd, int* saveErrno);
	// ͨ��fd��������
	ssize_t writeFd(int fd, int* saveErrno);

private:
	char* begin()
	{
		return &*buffer_.begin();	// vecotr�ײ�������Ԫ�صĵ�ַ��Ҳ�����������ʼ��ַ
	}

	const char* begin() const
	{
		return &*buffer_.begin();
	}

	void makeSpace(size_t len)
	{
		/*
		KCheapPrepend | reader | writer |
		KCheaPPrepend |       len          |
		*/
		if (writableBytes() + prependableBytes() < len + kCheapPrepend)
		{
			buffer_.resize(writerIndex_ + len);
		}
		else
		{
			size_t readable = readableBytes();
			std::copy(begin() + readerIndex_,
				begin() + writerIndex_,
				begin() + kCheapPrepend);

			readerIndex_ = kCheapPrepend;
			writerIndex_ = readerIndex_ + readable;
		}
	}

private:
	std::vector<char> buffer_;
	size_t readerIndex_;
	size_t writerIndex_;
};
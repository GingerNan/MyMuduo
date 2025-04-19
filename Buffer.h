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
/// 网络库底层的缓冲器类型定义
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

	// 返回缓冲区中可读数据的起始地址
	const char* peek() const { return begin() + readerIndex_; }

	// onMessage 
	void retrieve(size_t len)
	{
		if (len < readableBytes())
		{
			readerIndex_ += len;	// 应用只读取了可读缓冲区数据的一部分，就是len长度，还剩下 readIndex_+=len -> writeIndex_
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

	// 把onMessage函数上报的Buffer数据，转成string类型的数据返回
	std::string retrieveAllAsString()
	{
		return retrieveAsString(readableBytes()); //应用可读取数据的长度
	}

	std::string retrieveAsString(size_t len)
	{
		std::string result(peek(), len);
		retrieve(len);	// 上面一句把缓冲区中可读的数据，已经读取出来，这里肯定要对缓冲区进行复位操作
		return result;
	}

	// buffer_size - writerIndex_
	void ensureWriteableByres(size_t len)
	{
		if (writableBytes() < len)
		{
			makeSpace(len);	//扩容函数
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

	// 从fd上读取数据
	ssize_t readFd(int fd, int* saveErrno);
	// 通过fd发送数据
	ssize_t writeFd(int fd, int* saveErrno);

private:
	char* begin()
	{
		return &*buffer_.begin();	// vecotr底层数组首元素的地址，也就是数组的起始地址
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
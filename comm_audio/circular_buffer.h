#pragma once
#include <cstdio>
#include <memory>
#include <mutex>

template <class T>
class circular_buffer {
public:
	explicit circular_buffer(size_t size) :
		buf_(std::unique_ptr<T[]>(new T[size])),
		max_size_(size)
	{
		//empty constructor
	}

	void put(T item)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		buf_[head_] = item;

		if (full_)
		{
			tail_ = (tail_ + 1) % max_size_;
		}

		head_ = (head_ + 1) % max_size_;
		full_ = head_ == tail_;
	}

	void update(T item)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		buf_[head_] = item;
	}

	T peek()
	{
		std::lock_guard<std::mutex> lock(mutex_);

		if (empty())
		{
			return T();
		}
		auto val = buf_[tail_];
		full_ = false;

		return val;
	}

	T get()
	{
		std::lock_guard<std::mutex> lock(mutex_);

		if (empty())
		{
			return T();
		}

		//Read data and advance the tail
		auto val = buf_[tail_];
		full_ = false;
		tail_ = (tail_ + 1) % max_size_;

		return val;
	}

	void reset()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		head_ = tail_;
		full_ = false;
	}

	bool empty() const
	{
		return (!full_ && (head_ == tail_));
	}

	bool full() const
	{
		return full_;
	}

	size_t capacity() const
	{
		return max_size_;
	}

	size_t size() const
	{
		size_t size = max_size_;

		if (!full_)
		{
			if (head_ >= tail_)
			{
				size = head_ - tail_;
			}
			else
			{
				size = max_size_ + head_ - tail_;
			}
		}

		return size;
	}

private:
	std::mutex mutex_;
	std::unique_ptr<T[]> buf_;
	size_t head_ = 0;
	size_t tail_ = 0;
	const size_t max_size_;
	bool full_ = 0;
};
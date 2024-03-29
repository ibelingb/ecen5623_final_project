/***********************************************************************************
 * @author Joshua Malburg
 * joshua.malburg@colorado.edu
 * 
 * Real-time Embedded Systems
 * ECEN5623 - Sam Siewert
 * @date 25Jul2020
 * Ubuntu 18.04 LTS and RPi 3B+
 ************************************************************************************
 *
 * @file circular_buffer.h
 * @brief references: 
 * https://embeddedartistry.com/blog/2017/05/17/creating-a-circular-buffer-in-c-and-c/
 * https://stackoverflow.com/questions/9743605/thread-safe-implementation-of-circular-buffer
 * https://stackoverflow.com/questions/9472880/how-to-implement-a-circular-buffer-of-cvmat-objects-opencv
 * https://www.codeproject.com/Articles/1185449/Performance-of-a-Circular-Buffer-vs-Vector-Deque-a
 * https://docs.opencv.org/3.4.10/d1/dfb/intro.html
 ************************************************************************************
 */

#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

/*---------------------------------------------------------------------------------*/
/* INCLUDES */
#include <memory>
//#include <mutex>

template <class T>
class circular_buffer {
public:
	explicit circular_buffer(size_t size) :
		buf_(std::unique_ptr<T[]>(new T[size])),
		max_size_(size)
	{

	}

	void put(T item)
	{
		//std::lock_guard<std::mutex> lock(mutex_);

		buf_[head_] = item;

		if(full_)
		{
			tail_ = (tail_ + 1) % max_size_;
		}
		head_ = (head_ + 1) % max_size_;
		full_ = head_ == tail_;
	}

	T get()
	{
		//std::lock_guard<std::mutex> lock(mutex_);

		if(empty()) {
			return T();
		}

		// Read data and advance the tail (we now have a free space)
		auto val = buf_[tail_];
		full_ = false;
		tail_ = (tail_ + 1) % max_size_;

		return val;
	}

  T peek()
	{
		//std::lock_guard<std::mutex> lock(mutex_);

		if(empty()) {
			return T();
		}

		// Read data and DO NOT advance the tail
		auto val = buf_[tail_];
		return val;
	}

	void reset()
	{
		//std::lock_guard<std::mutex> lock(mutex_);
		head_ = tail_;
		full_ = false;
	}

	bool empty() const
	{
		//if head and tail are equal, we are empty
		return (!full_ && (head_ == tail_));
	}

	bool full() const
	{
		//If tail is ahead the head by 1, we are full
		return full_;
	}

	size_t capacity() const
	{
		return max_size_;
	}

	size_t size() const
	{
		size_t size = max_size_;

		if(!full_) {
			if(head_ >= tail_) {
				size = head_ - tail_;
			} else {
				size = max_size_ + head_ - tail_;
			}
		}
		return size;
	}

private:
	//std::mutex mutex_;
	std::unique_ptr<T[]> buf_;
	size_t head_ = 0;
	size_t tail_ = 0;
	const size_t max_size_;
	bool full_ = 0;
};

#endif
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

#ifndef CIRCULAR_CV_BUFFER_H
#define CIRCULAR_CV_BUFFER_H

/*---------------------------------------------------------------------------------*/
/* INCLUDES */
#include <memory>
#include <opencv2/core.hpp>

class circular_cv_buffer {
public:
	explicit circular_cv_buffer(size_t size) :
		buf_(std::unique_ptr<cv::Mat[]>(new cv::Mat[size])),
		max_size_(size)
	{

	}

	int put(const cv::Mat &item)
	{
		buf_[head_] = item.clone();

		if(full_)
		{
			tail_ = (tail_ + 1) % max_size_;
		}
		head_ = (head_ + 1) % max_size_;
		full_ = head_ == tail_;
    return 0;
	}

	int get(cv::Mat &img)
	{
		if(empty()) {
			return -1;
		}

		// Read data and advance the tail (we now have a free space)
		img = buf_[tail_].clone();
		full_ = false;
		tail_ = (tail_ + 1) % max_size_;

		return 0;
	}

   int peek(cv::Mat &img)
	{
		if(empty()) {
			return -1;
		}

		// Read data and DO NOT advance the tail
		img = buf_[tail_].clone();
		return 0;
	}

	void reset()
	{
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
	std::unique_ptr<cv::Mat[]> buf_;
	size_t head_ = 0;
	size_t tail_ = 0;
	const size_t max_size_;
	bool full_ = 0;
};

#endif
/**
 * @file ringbuffer.cpp
 * @brief RingBuffer implementation
 * @author 박민근
 * @date 2023-06-06
 */


#include "ringbuffer.h"


namespace rtos {


    /**
     * @brief Default Constructor which creates a buffer of length 10.
     */
    RingBuffer::RingBuffer(): BUFFER_SIZE(10) {
        _pBuffer = new int[10];
        _front = 0;
        _back = 0;
        _isFull = false;
    }


    /**
     * @brief Creates a ring buffer of length n.
     *
     * @param n Buffer size.
     */
    RingBuffer::RingBuffer(size_t n): BUFFER_SIZE(n) {
        _pBuffer = new int[BUFFER_SIZE];
        _front = 0;
        _back = 0;
        _isFull = false;
    }


    RingBuffer::~RingBuffer() {
        delete [] _pBuffer;
    }


    /**
     * @brief 아직 소비되지 않은 데이터가 덮어써지는 것을 허용한다.
     *
     * @param item 버퍼에 저장할 새로운 데이터.
     */
    void RingBuffer::put(int item) noexcept {

        unique_lock<mutex> lock(_mutex);

        /* Ciritcal section start */

        _pBuffer[_front] = item;
        _front = (_front + 1) % BUFFER_SIZE;
        _isFull = (_front == _back);

         /* ciritcal section end */

        lock.unlock();
    }



    /**
     * @brief 데이터를 덮어쓰지 않고 빈 공간이 생길때까지 대기한다.
     *
     * @param item 버퍼에 저장할 데이터.
     */
    void RingBuffer::putWithoutOverride(int item) noexcept {

        unique_lock<mutex> lock(_mutex);

        /* Ciritcal section start */
        _notFull.wait(lock, [this]() { return !_isFull; }); // 버퍼에 공간이 생길 때까지 기다린다.

        _pBuffer[_front] = item;
        _front = (_front + 1) % BUFFER_SIZE;
        _isFull = (_front == _back);
         /* ciritcal section end */

        lock.unlock();
        _notEmpty.notify_one();
    }



    /**
     * @brief 버퍼에서 FIFO 방식으로 값을 꺼낸다. 버퍼가 비어있을 경우 EmptyBufferReadException을 던진다.
     *
     * @return 버퍼의 데이터.
     */
    int RingBuffer::get() {

        unique_lock<mutex> lock(_mutex);

        /* Critical section start */
        if ( (_front == _back) && !_isFull ) {
            lock.unlock();
            throw EmptyBufferReadException();
        }

        int item = _pBuffer[_back];
        _back = (_back + 1) % BUFFER_SIZE;
        _isFull = false;

        lock.unlock();

        /* Critical section end */

        return item;
    }



    /**
     * @brief 버퍼가 비어 있는 경우 값이 저장될 때까지 기다린다.
     *
     * @return 버퍼의 데이터.
     */
    int RingBuffer::getFromNotEmptyBuffer() noexcept {

        unique_lock<mutex> lock(_mutex);

        /* Critical section start */
        /*
         * 버퍼가 비어있는 동안 대기한다.
         * (_front == _back) && !_isFull 인 동안 대기한다.*/
        _notEmpty.wait(lock,
            [this]() { return (_front != _back) || _isFull; }); 
        int item = _pBuffer[_back];
        _back = (_back + 1) % BUFFER_SIZE;
        _isFull = false;

        lock.unlock();
        _notFull.notify_one();

        /* Critical section end */

        return item;
    }
        
}; // rtos

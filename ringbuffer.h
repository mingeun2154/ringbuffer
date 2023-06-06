/**
 * @file ringbuffer.cpp
 * @brief RingBuffer interface
 * @author 박민근
 * @date 2023-06-06
 */

#ifndef _RING_BUFFER_H_
#define _RING_BUFFER_H_

#include <cstddef>
#include <vector>
#include <mutex>
#include <exception>
#ifdef __linux__
    #include <condition_variable>
#endif

typedef size_t period;

using namespace std;

namespace rtos {

    class RingBuffer {

        private:
            int* _pBuffer;
            const size_t BUFFER_SIZE;
            size_t _front;
            size_t _back;
            mutex _mutex;
            condition_variable _notEmpty; // 빈 버퍼에서 값을 읽는 것을 방지한다.
            condition_variable _notFull; // 버퍼에 빈 공간이 없는 경우 새로운 값이 덮어써지는 것을 방지한다.
            bool _isFull;

        public:
            RingBuffer();
            RingBuffer(size_t n);
            ~RingBuffer();

            void put (int item) noexcept;
            void putWithoutOverride (int item) noexcept;
            int get();
            int getFromNotEmptyBuffer () noexcept;

    }; // RingBuffer
    

    class DataOverrideException : public exception {

        private:
            const string _message = "Data override detected";

        public:
            const char* what() const throw() {
                return _message.c_str();
            }
        
    }; // DataOverrideException


    class EmptyBufferReadException : public exception {

        private:
            const string _message = "Buffer is empty";

        public:
            const char* what() const throw() {
                return _message.c_str();
            }

    }; // EmptyBufferReadException


    /**@struct ProducerArgs
     * @brief 데이터 생성 쓰레드에게 전달될 정보.
     * @var ProducerArgs::data
     * Buffer에 저장할 데이터
     * @var ProducerArgs::interval
     * Buffer에 접근하는 주기(정수값)
     * @var ProducerArgs::pRingBuffer
     * 데이터를 저장할 버퍼 주소
     */
    typedef struct thread_args {
        size_t threadNum;
        int* item;
        mutex* pMutex;
        period interval;
        RingBuffer* pRingBuffer;
    } ThreadArgs;

        
}; // rtos

#endif // _RING_BUFFER_H_

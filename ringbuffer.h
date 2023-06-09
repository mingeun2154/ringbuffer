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
#include <queue>
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


    /**@struct ThreadArgs
     * @brief 데이터 생성, 처리 쓰레드에게 전달될 정보.
     * @var ProducerArgs::data
     * Buffer에 저장할 데이터
     * @var ProducerArgs::interval
     * Buffer에 접근하는 주기(정수값)
     * @var ProducerArgs::pRingBuffer
     * 데이터를 저장할 버퍼 주소
     * @var ProducerArgs::pMsgq
     * 출력할 메세지를 저장할 큐
     * @var ProducerArgs::pDistribution
     * 실행시간이 저장된 배열의 포인터
     */
    typedef struct thread_args {
        size_t threadNum;
        int* item;
        mutex* pMutex;
        period interval;
        RingBuffer* pRingBuffer;
        queue<char*>* pMsgq;
        mutex* pMsgqMutex;
        period* pDistribution;
    } ThreadArgs;



    /**@struct ObserverArgs
     * @brief stdout에 출력을 담당하는 observer 쓰레드에게 전달할 정보.
     * @var ObserverArgs::pMsgq
     * 출력할 메세지가 저장된 큐에 대한 포인터
     * @var ObserverArgs::pMsgqMutex
     * 메세지큐에 접근하기 위한 mutex
     * @var ObserverArgs::counter
     * 종료 조건
     */
    typedef struct observer_args {
        queue<char*>* pMsgq;
        mutex* pMsgqMutex;
        size_t counter;
    } ObserverArgs;
        
}; // rtos

#endif // _RING_BUFFER_H_

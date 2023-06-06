/**
 * @file testcase.cpp
 * @brief Testcases for ringbuffer.
 * @author 박민근
 * @date 2023-06-06
 */


#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <ostream>
#include <pthread.h>
#include <stdio.h>
#include <sys/signal.h>
#include <unistd.h>
#include <chrono>

#include "ringbuffer.h"


using namespace rtos;
using namespace std;


namespace ANSI_CONTROL {
    
    const char* RED = "\033[0;31m";
    const char* GREEN = "\033[0;32m";
    const char* BLUE = "\033[0;34m";
    const char* DEFAULT = "\033[0m";
    const char* CLEAR = "\033[2J\033[2H";

}; // ANSI_CONTROL



/**
 * @brief 시뮬레이션 변수
 */
namespace SIMUL_PARAM {

    const int BUFFER_SIZE = 5;

    // testcase a
    const period TA_PROD_PERIOD = 3;
    const period TA_CONS_PERIOD = 1;
    const int TA_PROD_NUM = 2;
    const int TA_CONS_NUM = 3;

    // testcase b
    const period TB_PROD_PERIOD = 1;
    const period TB_CONS_PERIOD = 1;
    const int TB_PROD_NUM = 3;
    const int TB_CONS_NUM = 2;

    // testcase c
    const period TC_PROD_PERIOD = 1;
    const period TC_CONS_PERIOD = 3;
    const int TC_PROD_NUM = 5;
    const int TC_CONS_NUM = 2;
    
}; // SIMUL_PARAM


void sigintHandler(int);
void* produce(void*); // Body of producer thread.
void* consume(void*); // Body of consumer thread.

void testbody(period, period, size_t, size_t);
void testcaseA(); // Data의 평균 발생속도 < 평균 처리속도
void testcaseB(); // Data의 평균 발생속도 = 평균 처리속도
void testcaseC(); // Data의 평균 발생속도 > 평균 처리속도
void printUsage();


int main(int argc, char* argv[]) {

    printf("%s", ANSI_CONTROL::CLEAR);

    if (argc != 2) {
        printUsage();
        return 0;
    }

    char testcaseNum = (char)(*argv[1]);
    switch (testcaseNum) {
        case 'a': testcaseA(); break;
        case 'b': testcaseB(); break;
        case 'c': testcaseC(); break;
        default: printUsage(); break;
    }

    return 0;
}


void printUsage() {
    cout << ANSI_CONTROL::RED
        << "usage: exe <option>"
        << ANSI_CONTROL::DEFAULT << endl;
    cout << "options\n";
    cout << "a: 평균처리속도가 평균발생속도보다 빠른 경우\n";
    cout << "b: 평균처리속도와 평균발생속도보다 같은 경우\n";
    cout << "c: 평균처리속도가 평균발생속도보다 느린 경우\n";
    cout << "ctrl-c: 프로그램 종료\n";
}



/**
 * @brief 프로그램 시작시간을 기준으로 경과 시간(초) 반환.
 *
 * @return 정수 값(초)
 */
long long elapsedtime() {

    static auto start = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();

    auto now = chrono::system_clock::now();  // 현재 시각 가져오기

    auto duration = now.time_since_epoch();  // 에포크로부터의 시간 간격 계산
    auto milliseconds = chrono::duration_cast<chrono::milliseconds>(duration).count();  // 밀리초로 변환

    return  (milliseconds - start) % 10000;
}


void sigintHandler(int signum) {
    printf("\r  \nExit program\n\n");
    exit(signum);
}


void* consume(void* param) {

    ThreadArgs* pArgs = (ThreadArgs*)param;

    size_t threadNum = pArgs->threadNum;
    RingBuffer* pBuffer = pArgs->pRingBuffer;
    period t = pArgs->interval;

    while (true) {
        sleep(t);
        try {
            printf("[timestamp:%07lldms] %s[Consumer%2zu] 소비한 데이터: %d%s\n",
                elapsedtime(),
                ANSI_CONTROL::BLUE,
                threadNum,
                (int)(pBuffer->get()),
                ANSI_CONTROL::DEFAULT);
            fflush(stdout);
        }
        catch (const EmptyBufferReadException& e) {
            printf("[timestamp:%07lldms] %s[Consumer%2zu] %s%s\n",
                elapsedtime(),
                ANSI_CONTROL::BLUE,
                threadNum,
                e.what(),
                ANSI_CONTROL::DEFAULT);
        }
    }

    return nullptr;

}


void* produce(void* param) {

    ThreadArgs* pArgs = (ThreadArgs*)param;

    size_t threadNum = pArgs->threadNum;
    int* pData = pArgs->item;
    mutex* pMutex = pArgs->pMutex;
    RingBuffer* pBuffer = pArgs->pRingBuffer;
    period t = pArgs->interval;

    while (true) {
        sleep(t);

        unique_lock<mutex> lock(*pMutex);
        /* Critical section start */
        int data = (*pData);
        pBuffer->put((*pData)++);
        /* Critical section end */
        lock.unlock();

        elapsedtime();
        printf("[timestamp:%07lldms] %s[Producer%2zu] 생성한 데이터: %d%s\n",
            elapsedtime(),
            ANSI_CONTROL::GREEN,
            threadNum,
            data,
            ANSI_CONTROL::DEFAULT);
        fflush(stdout);
    }
    
    return nullptr;

}


/**
 * @brief Producer, Consumer 쓰레드를 생성하고 실행한다.
 *
 * @param c Consumer의 동작 주기
 * @param p Producer의 동작 주기
 */
void testbody(period p, period c, size_t pn, size_t cn) {
    
    signal(SIGINT, sigintHandler);

    printf("Buffer size: %d\n", SIMUL_PARAM::BUFFER_SIZE);
    printf("Producer thread 개수: %ld\n", pn);
    printf("Consumer thread 개수: %ld\n", cn);
    printf("Producer의 데이터 생성속도 평균: 4byte/%zus\n", p);
    printf("Consumer의 데이터 소비속도 평균: 4byte/%zus\n\n", c);

    RingBuffer buffer(SIMUL_PARAM::BUFFER_SIZE);
    mutex m;
    int item = 0;
    pthread_t producer;
    pthread_t consumer;

    /* CONSUMER_NUM만큼 producer thread를 생성하고 실행한다. */
    for (size_t i=0; i<pn; i++) {
        ThreadArgs* pProducerArgs = new ThreadArgs; //TODO 메모리 해제 안 했음
        pProducerArgs->threadNum = i;
        pProducerArgs->item = &item;
        pProducerArgs->pMutex = &m;
        pProducerArgs->interval = p;
        pProducerArgs->pRingBuffer = &buffer;
        pthread_create(&producer, NULL, produce, (void*)(pProducerArgs));
    }
    
    /* CONSUMER_NUM만큼 consumer thread를 생성하고 실행한다. */
    for (size_t i=0; i<cn; i++) {
        ThreadArgs* pConsumerArgs = new ThreadArgs; //TODO 메모리 해제 안 했음
        pConsumerArgs->threadNum = i;
        pConsumerArgs->item = &item;
        pConsumerArgs->pMutex = &m;
        pConsumerArgs->interval = p;
        pConsumerArgs->pRingBuffer = &buffer;
        pthread_create(&consumer, NULL, consume, (void*)(pConsumerArgs));
    }

    for (size_t i=0; i<pn; i++)
        pthread_join(producer, nullptr);

    for (size_t i=0; i<cn; i++)
        pthread_join(consumer, nullptr);

}


void testcaseA() {

    printf("tescase a\n");
    testbody(SIMUL_PARAM::TA_PROD_PERIOD, SIMUL_PARAM::TA_CONS_PERIOD,
        SIMUL_PARAM::TA_PROD_NUM, SIMUL_PARAM::TA_CONS_NUM);

}


void testcaseB() {

    printf("tescase b\n");
    testbody(SIMUL_PARAM::TB_PROD_PERIOD, SIMUL_PARAM::TB_CONS_PERIOD,
        SIMUL_PARAM::TB_PROD_NUM, SIMUL_PARAM::TB_CONS_NUM);
}


void testcaseC() {

    printf("tescase c\n");
    testbody(SIMUL_PARAM::TC_PROD_PERIOD, SIMUL_PARAM::TC_CONS_PERIOD,
        SIMUL_PARAM::TC_PROD_NUM, SIMUL_PARAM::TC_CONS_NUM);

}

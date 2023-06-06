/**
 * @file testcase.cpp
 * @brief Testcases for ringbuffer.
 * @author 박민근
 * @date 2023-06-06
 */


#include <cstdlib>
#include <iostream>
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


const int BUFFER_SIZE = 5;

void sigintHandler(int);
void* produce(void*); // Body of producer thread.
void* consume(void*); // Body of consumer thread.

void testbody(period, period);
void testcase1(); // Data의 평균 발생속도 < 평균 처리속도
void testcase2(); // Data의 평균 발생속도 = 평균 처리속도
void testcase3(); // Data의 평균 발생속도 > 평균 처리속도
void printUsage();


int main(int argc, char* argv[]) {

    cout << ANSI_CONTROL::CLEAR;

    if (argc != 2) {
        printUsage();
        return 0;
    }

    char testcaseNum = (char)(*argv[1]);
    switch (testcaseNum) {
        case 'a': testcase1(); break;
        case 'b': testcase2(); break;
        case 'c': testcase3(); break;
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
void printElapsedtime() {

    static auto start = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();

    auto now = chrono::system_clock::now();  // 현재 시각 가져오기

    auto duration = now.time_since_epoch();  // 에포크로부터의 시간 간격 계산
    auto milliseconds = chrono::duration_cast<chrono::milliseconds>(duration).count();  // 밀리초로 변환

    auto ts = (milliseconds - start) % 10000;
    printf("[timestamp:%07lldms] ", ts);

}


void sigintHandler(int signum) {
    printf("\rExit program\n");
    exit(signum);
}


void* consume(void* param) {

    ThreadArgs* pArgs = (ThreadArgs*)param;
    RingBuffer* pBuffer = pArgs->pRingBuffer;
    period t = pArgs->interval;

    while (true) {
        sleep(t);
        try {
            printElapsedtime();
            printf("%s[Consumer] 소비한 데이터: %d%s\n",
                ANSI_CONTROL::BLUE,
                (int)(pBuffer->get()),
                ANSI_CONTROL::DEFAULT);
        }
        catch (const EmptyBufferReadException& e) {
            printf("%s[Consumer] %s%s\n",
                ANSI_CONTROL::RED,
                e.what(),
                ANSI_CONTROL::DEFAULT);
        }
    }

    return nullptr;

}


void* produce(void* param) {

    ThreadArgs* pArgs = (ThreadArgs*)param;
    RingBuffer* pBuffer = pArgs->pRingBuffer;
    period t = pArgs->interval;
    int data = 0;

    while (true) {
        sleep(t);
        printElapsedtime();
        printf("%s[Producer] 생성한 데이터: %d%s\n",
            ANSI_CONTROL::GREEN,
            data,
            ANSI_CONTROL::DEFAULT);
        pBuffer->put(data++);
    }
    
    return nullptr;

}



/**
 * @brief Producer, Consumer 쓰레드를 생성하고 실행한다.
 *
 * @param c Consumer의 동작 주기
 * @param p Producer의 동작 주기
 */
void testbody(period c, period p) {
    
    signal(SIGINT, sigintHandler);

    cout << "Buffer size: " << BUFFER_SIZE << endl;
    printf("producer의 데이터 생성속도 평균: 4byte/%zus\n", p);
    printf("consumer의 데이터 소비속도 평균: 4byte/%zus\n", c);

    RingBuffer buffer(BUFFER_SIZE);
    pthread_t producer;
    pthread_t consumer;

    ThreadArgs consumerArgs { c, &buffer };
    ThreadArgs producerArgs { p, &buffer };

    // Execute producer threads.
    pthread_create(&producer, NULL, produce, (void*)(&producerArgs));

    // Execute consumer threads.
    pthread_create(&consumer, NULL, consume, (void*)(&consumerArgs));

    pthread_join(producer, nullptr);
    pthread_join(consumer, nullptr);
}


void testcase1() {

    period consumerPeriod = 1;
    period producerPeriod = 3;

    cout << "tescase a\n";
    testbody(consumerPeriod, producerPeriod);

}


void testcase2() {

    period consumerPeriod = 1;
    period producerPeriod = 1;

    cout << "tescase b\n";
    testbody(consumerPeriod, producerPeriod);
}


void testcase3() {

    period consumerPeriod = 5;
    period producerPeriod = 1;

    cout << "tescase b\n";
    testbody(consumerPeriod, producerPeriod);

}

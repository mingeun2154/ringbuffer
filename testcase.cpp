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
#include <ratio>
#include <stdio.h>
#include <sys/signal.h>
#include <thread>
#include <unistd.h>
#include <chrono>
#include <random>

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

    const double PROD_SIGMA = 50.0; // 데이터 발생속도 표준편차
    const double CONS_SIGMA = 50.0; // 데이터 처리속도 표준편차

    const int SAMPLE_SIZE = 100; // 데이터 발생/처리 횟수

    // testcase a
    const period TA_PROD_PERIOD = 100; // 데이터 평균발생주기 (milliseconds)
    const period TA_CONS_PERIOD = 50; // 데이터 평균처리주기 (milliseconds)
    const int TA_PROD_NUM = 1;
    const int TA_CONS_NUM = 1;

    // testcase b
    const period TB_PROD_PERIOD = 50; // 데이터 평균발생주기 (milliseconds)
    const period TB_CONS_PERIOD = 50; // 데이터 평균처리주기 (milliseconds)
    const int TB_PROD_NUM = 1;
    const int TB_CONS_NUM = 1;

    // testcase c
    const period TC_PROD_PERIOD = 50; // 데이터 평균발생주기 (milliseconds)
    const period TC_CONS_PERIOD = 150; // 데이터 평균처리주기 (milliseconds)
    const int TC_PROD_NUM = 1;
    const int TC_CONS_NUM = 1;

    const size_t MSG_LENGTH = 80;

}; // SIMUL_PARAM


void initThreadPeriod(period ndist[], double sigma, int m);

void sigintHandler(int);
void* produce(void*); // Body of producer thread.
void* consume(void*); // Body of consumer thread.
void* observe(void*); // Body of observer thread.

size_t miss = 0; // 빈 버퍼에 접근한 횟수
void testbody(period, period, size_t, size_t, queue<char*>&, mutex&);
void testcaseA(queue<char*>&, mutex&); // Data의 평균 발생속도 < 평균 처리속도
void testcaseB(queue<char*>&, mutex&); // Data의 평균 발생속도 = 평균 처리속도
void testcaseC(queue<char*>&, mutex&); // Data의 평균 발생속도 > 평균 처리속도
void printUsage();


int main(int argc, char* argv[]) {

    printf("%s", ANSI_CONTROL::CLEAR);

    if (argc != 2) {
        printUsage();
        return 0;
    }

    // Execute observer.
    // Observer thread print messages to stdout.
    queue<char*> msgq;
    mutex msgMutex;
    ObserverArgs* pObserverArgs = new ObserverArgs; //TODO 메모리 해제 안 했음
    pObserverArgs->pMsgqMutex = &msgMutex;
    pObserverArgs->pMsgq = &msgq;
    pthread_t observer;

    char testcaseNum = (char)(*argv[1]);
    switch (testcaseNum) {
        case 'a':
            pObserverArgs->counter =
                (SIMUL_PARAM::TA_CONS_NUM + SIMUL_PARAM::TA_PROD_NUM) * (SIMUL_PARAM::SAMPLE_SIZE);
            pthread_create(&observer, NULL, observe, (void*)(pObserverArgs));
            testcaseA(msgq, msgMutex);
            break;
        case 'b':
            pObserverArgs->counter =
                (SIMUL_PARAM::TB_CONS_NUM + SIMUL_PARAM::TB_PROD_NUM) * (SIMUL_PARAM::SAMPLE_SIZE);
            pthread_create(&observer, NULL, observe, (void*)(pObserverArgs));
            testcaseB(msgq, msgMutex);
            break;
        case 'c':
            pObserverArgs->counter =
                (SIMUL_PARAM::TC_CONS_NUM + SIMUL_PARAM::TC_PROD_NUM) * (SIMUL_PARAM::SAMPLE_SIZE);
            pthread_create(&observer, NULL, observe, (void*)(pObserverArgs));
            testcaseC(msgq, msgMutex);
            break;
        default: printUsage(); break;
    }

    pthread_join(observer, NULL);

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

    // 프로그램 시작 시각 초기화
    static auto start = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();

    auto now = chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto milliseconds = chrono::duration_cast<chrono::milliseconds>(duration).count();  // 밀리초로 변환

    return  (milliseconds - start) % 10000000;
}



/**
 * @brief 정규분포를 따르는 정수값을 생성하여 배열에 저장한다.
 *
 * @param ndist[] 길이가 SIMUL_PARAM::SAMPLE_SIZE인 period 타입 배열
 * @param sigma 표준편차
 * @param m 평균
 */
void initThreadPeriod(period ndist[], double sigma, period m) {
    // random_device rd;
    // mt19937 generator(rd());
    default_random_engine generator;
    normal_distribution<double> distribution(static_cast<double>(m), sigma);
    // 평균이 m이고 표준편차가 sigma인 정규분포를 따르는 정수값 생성.
    // 생성된 정수값은 데이터 발생/처리 쓰레드의 실행 주기가 된다.
    for (int i=0; i<SIMUL_PARAM::SAMPLE_SIZE; ++i) {
        period t = static_cast<period>(distribution(generator));
        ndist[i] = t;
    }
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
    queue<char*>* pMsgq = pArgs->pMsgq;
    mutex* pMsgqMutex = pArgs->pMsgqMutex;
    period* pDistribution = pArgs->pDistribution;

    for (size_t i=0; i<SIMUL_PARAM::SAMPLE_SIZE; ++i) {
        this_thread::sleep_for(chrono::milliseconds(pDistribution[i]));
        char* pMsgbuff = new char[SIMUL_PARAM::MSG_LENGTH];
        try {
            sprintf(pMsgbuff,"[timestamp:%07lldms] %s[Consumer%2zu] 소비한 데이터: %d%s\n",
                elapsedtime(),
                ANSI_CONTROL::BLUE,
                threadNum,
                (int)(pBuffer->get()),
                ANSI_CONTROL::DEFAULT);
        }
        catch (const EmptyBufferReadException& e) {
            sprintf(pMsgbuff, "[timestamp:%07lldms] %s[Consumer%2zu] %s%s\n",
                elapsedtime(),
                ANSI_CONTROL::BLUE,
                threadNum,
                e.what(),
                ANSI_CONTROL::DEFAULT);
            miss++;
        }

        // 출력 메세지 저장
        unique_lock<mutex> msgqLock(*pMsgqMutex);
        /* Critical section start */
        pMsgq->push(pMsgbuff);
        /* Critical section end */
        msgqLock.unlock();
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
    queue<char*>* pMsgq = pArgs->pMsgq;
    mutex* pMsgqMutex = pArgs->pMsgqMutex;
    period* pDistribution = pArgs->pDistribution;

    for (size_t i=0; i<SIMUL_PARAM::SAMPLE_SIZE; ++i) {
        this_thread::sleep_for(chrono::milliseconds(pDistribution[i]));

        unique_lock<mutex> lock(*pMutex);
        /* Critical section start */
        int data = (*pData);
        pBuffer->put((*pData)++);
        /* Critical section end */
        lock.unlock();

        char* pMsgbuff = new char[SIMUL_PARAM::MSG_LENGTH];
        elapsedtime();
        sprintf(pMsgbuff, "[timestamp:%07lldms] %s[Producer%2zu] 생성한 데이터: %d%s\n",
            elapsedtime(),
            ANSI_CONTROL::GREEN,
            threadNum,
            data,
            ANSI_CONTROL::DEFAULT);

        // 출력 메세지 저장
        unique_lock<mutex> msgqLock(*pMsgqMutex);
        /* Critical section start */
        pMsgq->push(pMsgbuff);
        /* Critical section end */
        msgqLock.unlock();
    }
    
    return nullptr;

}



/**
 * @brief 큐에 저장된 메세지를 출력한다.
 *
 * @param param
 * queue<char*>* 메세지가 저장된 큐의 주소
 *
 * @return nullptr
 */
void* observe(void* param) {
    
    ObserverArgs* pArgs = static_cast<ObserverArgs*>(param);
    queue<char*>* pMsgq = pArgs->pMsgq;
    mutex* pMsgqMutex = pArgs->pMsgqMutex;
    size_t end = pArgs->counter;
    size_t progress = 0;

    char* msg = nullptr;

    while (progress != end) {
         unique_lock<mutex> msgqLock(*pMsgqMutex);
        /* Critical section start */
        if (!pMsgq->empty()) {
            msg = pMsgq->front();
            pMsgq->pop();
            progress++;
        }
        /* Critical section end */
        msgqLock.unlock();

        if (msg != nullptr) {
            printf("%s", msg);
            delete[] msg;
            msg = nullptr;
        }
    }

    return nullptr;
}


/**
 * @brief Producer, Consumer 쓰레드를 생성하고 실행한다.
 *
 * @param c Consumer의 동작 주기
 * @param p Producer의 동작 주기
 */
void testbody(period p, period c, size_t pn, size_t cn,
    queue<char*>& msgq, mutex& msgqMutex) {
    
    signal(SIGINT, sigintHandler);

    printf("Buffer size: %d\n", SIMUL_PARAM::BUFFER_SIZE);
    printf("표본의 크기(Producer, Consumer 각각의 실행 횟수): %d\n", SIMUL_PARAM::SAMPLE_SIZE);
    printf("Producer의 데이터 생성주기 평균: %zums\n", p);
    printf("Consumer의 데이터 소비주기 평균: %zums\n", c);
    printf("Producer의 데이터 생성주기 표준편차: %.2f\n", SIMUL_PARAM::PROD_SIGMA);
    printf("Consumer의 데이터 소비주기 표준편차: %.2f\n\n", SIMUL_PARAM::CONS_SIGMA);

    // 각 쓰레드의 실행주기 초기화
    mutex producerPeriodMutex;
    mutex consumerPeriodMutex;
    period producerPeriod[SIMUL_PARAM::SAMPLE_SIZE];
    period consumerPeriod[SIMUL_PARAM::SAMPLE_SIZE];
    initThreadPeriod(producerPeriod, SIMUL_PARAM::PROD_SIGMA, p);
    initThreadPeriod(consumerPeriod, SIMUL_PARAM::CONS_SIGMA, c);

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
        pProducerArgs->pMsgq = &msgq;
        pProducerArgs->pMsgqMutex = &msgqMutex;
        pProducerArgs->pDistribution = producerPeriod;
        pthread_create(&producer, NULL, produce, (void*)(pProducerArgs));
    }
    
    /* CONSUMER_NUM만큼 consumer thread를 생성하고 실행한다. */
    for (size_t i=0; i<cn; i++) {
        ThreadArgs* pConsumerArgs = new ThreadArgs; //TODO 메모리 해제 안 했음
        pConsumerArgs->threadNum = i;
        pConsumerArgs->item = &item;
        pConsumerArgs->pMutex = &m;
        pConsumerArgs->interval = c;
        pConsumerArgs->pRingBuffer = &buffer;
        pConsumerArgs->pMsgq = &msgq;
        pConsumerArgs->pMsgqMutex = &msgqMutex;
        pConsumerArgs->pDistribution = consumerPeriod;
        pthread_create(&consumer, NULL, consume, (void*)(pConsumerArgs));
    }

    for (size_t i=0; i<pn; i++)
        pthread_join(producer, NULL);

    for (size_t i=0; i<cn; i++)
        pthread_join(consumer, NULL);

    printf("\n표본의 크기(Producer, Consumer 각각의 실행 횟수): %d\n", SIMUL_PARAM::SAMPLE_SIZE);
    printf("Producer의 데이터 생성주기 평균: %zums\n", p);
    printf("Consumer의 데이터 소비주기 평균: %zums\n", c);
    printf("Producer의 데이터 생성주기 표준편차: %.2f\n", SIMUL_PARAM::PROD_SIGMA);
    printf("Consumer의 데이터 소비주기 표준편차: %.2f\n", SIMUL_PARAM::CONS_SIGMA);
    printf("%s빈 버퍼에 접근한 횟수: %zu%s\n\n", ANSI_CONTROL::RED, miss, ANSI_CONTROL::DEFAULT);
}


void testcaseA(queue<char*>& msgq, mutex& msgqMutex) {

    printf("tescase a\n");
    testbody(SIMUL_PARAM::TA_PROD_PERIOD, SIMUL_PARAM::TA_CONS_PERIOD,
        SIMUL_PARAM::TA_PROD_NUM, SIMUL_PARAM::TA_CONS_NUM,
        msgq, msgqMutex);

}


void testcaseB(queue<char*>& msgq, mutex& msgqMutex) {

    printf("tescase b\n");
    testbody(SIMUL_PARAM::TB_PROD_PERIOD, SIMUL_PARAM::TB_CONS_PERIOD,
        SIMUL_PARAM::TB_PROD_NUM, SIMUL_PARAM::TB_CONS_NUM,
        msgq, msgqMutex);
}


void testcaseC(queue<char*>& msgq, mutex& msgqMutex) {

    printf("tescase c\n");
    testbody(SIMUL_PARAM::TC_PROD_PERIOD, SIMUL_PARAM::TC_CONS_PERIOD,
        SIMUL_PARAM::TC_PROD_NUM, SIMUL_PARAM::TC_CONS_NUM,
        msgq, msgqMutex);

}

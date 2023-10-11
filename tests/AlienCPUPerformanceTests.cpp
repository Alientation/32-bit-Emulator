#include <chrono>

#define private public
#include <../src/Motherboard/AlienCPU.h>

void stackTestPushByteToStack_CHAININGMETHODS() {
    std::cout << "Running Stack Test: Push Byte, Chaining Methods" << std::endl;
    AlienCPU cpu = AlienCPU();
    auto start = std::chrono::system_clock::now();
    int count = 0; 

    for (int test = 0; test < 1000; test++) {
        for (int i = 0; i < 256; i++) {
            for (int byte = 0; byte < 0xFF; byte++) {
                cpu.pushByteToStack(byte);
                count++;
            }
        }

        cpu.SP = cpu.SP_INIT;
    }

    auto end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds = end-start;

    std::cout << "Stack Test COMPLETED [" << count << " calls in " << elapsed_seconds.count() << "s]" << std::endl << std::endl;
}

void stackTestPopByteToStack_CHAININGMETHODS() {
    std::cout << "Running Stack Test: Pop Byte, Chaining Methods" << std::endl;
    AlienCPU cpu = AlienCPU();
    auto start = std::chrono::system_clock::now();
    int count = 0; 

    for (int test = 0; test < 1000; test++) {
        cpu.SP = cpu.SP_INIT - 0x10000;

        for (int i = 0; i < 256; i++) {
            for (int j = 0; j < 0xFF; j++) {
                cpu.popByteFromStack();
                count++;
            }
        }
    }

    auto end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds = end-start;

    std::cout << "Stack Test COMPLETED [" << count << " calls in " << elapsed_seconds.count() << "s]" << std::endl << std::endl;
}

// runs some perfomance tests on the AlienCPU internal stack methods
void runStackPerformanceTests() {
    std::cout << "Running Stack Performance Tests" << std::endl << std::endl;
    auto start = std::chrono::system_clock::now();

    stackTestPushByteToStack_CHAININGMETHODS();
    stackTestPopByteToStack_CHAININGMETHODS();

    auto end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds = end-start;

    std::cout << "Stack Performance Tests COMPLETED (" << elapsed_seconds.count() << "s)" << std::endl << std::endl;
}

// runs some perfomance tests on the AlienCPU
int main() {
    std::cout << "Running AlienCPU Performance Tests" << std::endl << std::endl;
    auto start = std::chrono::system_clock::now();


    runStackPerformanceTests();


    auto end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds = end-start;

    std::cout << "AlienCPU Performance Tests COMPLETED (" << elapsed_seconds.count() << "s)" << std::endl;
    return 0;
}
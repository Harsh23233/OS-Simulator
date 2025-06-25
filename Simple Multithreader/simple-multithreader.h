#include <iostream>
#include <functional>
#include <pthread.h>
#include <chrono>
#include <cmath>
using namespace std;

// Struct for thread arguments in a 1D loop
struct thread_args_vector {
    int low;
    int high;
    function<void(int)> lambda;
};

// Struct for thread arguments in a 2D loop
struct thread_args_matrix {
    int low1, high1;
    int low2, high2;
    function<void(int, int)> lambda;
};

// Thread function for processing a vector range
void* processVectorRange(void* ptr) {
    auto* args = static_cast<thread_args_vector*>(ptr);
    for (int i = args->low; i < args->high; ++i) {
        args->lambda(i);
    }
    delete args; // Free memory after use
    return nullptr;
}

// Thread function for processing a matrix range
void* processMatrixRange(void* ptr) {
    auto* args = static_cast<thread_args_matrix*>(ptr);
    for (int i = args->low1; i < args->high1; ++i) {
        for (int j = args->low2; j < args->high2; ++j) {
            args->lambda(i, j);
        }
    }
    delete args; // Free memory after use
    return nullptr;
}

// Helper function to create threads
template <typename ThreadArgs>
void createThreads(pthread_t* threadIds, ThreadArgs** threadArgs, int numThreads, void* (*threadFunc)(void*)) {
    for (int i = 0; i < numThreads; ++i) {
        if (pthread_create(&threadIds[i], NULL, threadFunc, threadArgs[i]) != 0) {
            cerr << "Error: Failed to create thread " << i << endl;
            exit(1);
        }
    }
}

// Helper function to join threads
void joinThreads(pthread_t* threadIds, int numThreads) {
    for (int i = 0; i < numThreads; ++i) {
        if (pthread_join(threadIds[i], NULL) != 0) {
            cerr << "Error: Failed to join thread " << i << endl;
            exit(1);
        }
    }
}

// Parallel for loop for a 1D range
void parallel_for(int l, int h, function<void(int)> lambda, int numThreads) {
    if (numThreads <= 0) {
        cerr << "Error: The number of threads must be greater than zero. Exiting." << endl;
        return;
    }
    if (l >= h) {
        cerr << "Error: Invalid range specified. Ensure 'l' < 'h'." << endl;
        return;
    }

    auto start = chrono::high_resolution_clock::now();

    pthread_t threadIds[numThreads];
    auto** threadArgs = new thread_args_vector*[numThreads];

    int chunk = (h - l) / numThreads;
    int remainder = (h - l) % numThreads;

    for (int i = 0; i < numThreads; ++i) {
        threadArgs[i] = new thread_args_vector{
            l + i * chunk + (i < remainder ? i : remainder),
            l + (i + 1) * chunk + (i < remainder ? i + 1 : remainder),
            lambda
        };
    }

    createThreads(threadIds, threadArgs, numThreads, processVectorRange);
    joinThreads(threadIds, numThreads);

    delete[] threadArgs; 

    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double> totalTime = end - start;
    cout << "[Execution Report] Total Time Taken: " << totalTime.count() << " seconds" << endl;
}

// Parallel for loop for a 2D range
void parallel_for(int l1, int h1, int l2, int h2, function<void(int, int)> lambda, int numThreads) {
    if (numThreads <= 0) {
        cerr << "Error: The number of threads must be greater than zero. Exiting." << endl;
        return;
    }
    if (l1 >= h1 || l2 >= h2) {
        cerr << "Error: Invalid range specified. Ensure 'l1' < 'h1' and 'l2' < 'h2'." << endl;
        return;
    }

    auto start = chrono::high_resolution_clock::now();

    pthread_t threadIds[numThreads];
    auto** threadArgs = new thread_args_matrix*[numThreads];

    int chunk1 = (h1 - l1) / numThreads;
    int remainder1 = (h1 - l1) % numThreads;

    for (int i = 0; i < numThreads; ++i) {
        threadArgs[i] = new thread_args_matrix{
            l1 + i * chunk1 + (i < remainder1 ? i : remainder1),
            l1 + (i + 1) * chunk1 + (i < remainder1 ? i + 1 : remainder1),
            l2,
            h2,
            lambda
        };
    }

    createThreads(threadIds, threadArgs, numThreads, processMatrixRange);
    joinThreads(threadIds, numThreads);

    delete[] threadArgs; 

    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double> totalTime = end - start;
    cout << "[Execution Report] Total Time Taken: " << totalTime.count() << " seconds" << endl;
}

// Main function provided by the user
int user_main(int argc, char** argv);

// Entry point
int main(int argc, char** argv) {
    int rc = user_main(argc, argv);
    return rc;
}

// 'main' -> 'user_main'
#define main user_main

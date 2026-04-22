#pragma once
#include <windows.h>
#include <vector>
#include <cstring>
#include "../include/KernelInterface.hpp"

class BatchMemoryReader {
private:
    struct ReadRequest {
        ULONGLONG address;
        void* buffer;
        SIZE_T size;
    };

    std::vector<ReadRequest> requests;
    KernelInterface* kernel;
    ULONG pid;

public:
    BatchMemoryReader() : kernel(nullptr), pid(0) {}

    void Initialize(KernelInterface& k, ULONG p) {
        kernel = &k;
        pid = p;
        requests.clear();
    }

    void QueueRead(ULONGLONG address, void* buffer, SIZE_T size) {
        requests.push_back({address, buffer, size});
    }

    template<typename T>
    void QueueRead(ULONGLONG address, T* out) {
        QueueRead(address, out, sizeof(T));
    }

    bool ExecuteAll() {
        if (!kernel || !kernel->IsConnected() || requests.empty()) return false;

        bool allSuccess = true;
        for (auto& req : requests) {
            if (!kernel->ReadMemoryBlock(pid, req.address, req.buffer, req.size)) {
                std::memset(req.buffer, 0, req.size);
                allSuccess = false;
            }
        }

        requests.clear();
        return allSuccess;
    }

    void Reset() {
        requests.clear();
    }

    size_t GetPendingCount() const {
        return requests.size();
    }
};

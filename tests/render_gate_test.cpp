#include "RenderGate.h"
#include <future>
#include <iostream>
int main() {
    pg::RenderGate gate;
    { auto token=gate.tryRead(); if(!token.isLocked()) return 1; }
    gate.beginWrite(); { auto token=gate.tryRead(); if(token.isLocked()) return 2; } gate.endWrite();
    std::promise<void> entered; auto started=entered.get_future();
    std::future<void> writer;
    {
        auto token=gate.tryRead();
        writer=std::async(std::launch::async,[&] { entered.set_value(); gate.beginWrite(); gate.endWrite(); });
        started.wait();
        if(writer.wait_for(std::chrono::milliseconds(10))!=std::future_status::timeout) return 3;
    }
    writer.get();
    std::cout << "realtime gate exclusion and model writer drain passed\n";
}

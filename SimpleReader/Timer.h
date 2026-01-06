#pragma once

#include <string>
#include <chrono>

#include <Windows.h>

class Timer {
public:
    // 构造函数开始计时
    Timer(const std::string& name = "Timer");


    // 析构函数结束计时并输出结果
    ~Timer();

    // 禁止拷贝和赋值
    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;

    // 可以移动构造
    Timer(Timer&&) = default;
    Timer& operator=(Timer&&) = default;

private:
    std::string name_;
    std::chrono::time_point<std::chrono::high_resolution_clock> start_;
};


class TimerOutput
{
public:
    static TimerOutput& Instance()
    {
        static TimerOutput instance;
        return instance;
    }
    void add(std::string name, uint64_t duration);
    void print();

    void clear();
    void start(std::string info = "timer") { timer = std::make_unique<Timer>(info); }
    void end() { if (timer) { timer.reset(); print(); } }
private:
    TimerOutput() = default;
    ~TimerOutput() = default;

    TimerOutput(const TimerOutput&) = delete;
    TimerOutput& operator=(const TimerOutput&) = delete;

    std::string format_duration(double seconds);
    struct data
    {
        std::string name = "";
        uint64_t duration = 0;
        uint64_t times = 0;
    };
    std::unique_ptr<Timer> timer;
    std::vector<data> m_map;
};
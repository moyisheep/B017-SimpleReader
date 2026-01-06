#include "Timer.h"

Timer::Timer(const std::string& name) :
    name_(name), start_(std::chrono::high_resolution_clock::now())
{
    //std::cout << name_ << " started...\n";
}
Timer::~Timer() {
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_);
    TimerOutput::Instance().add(name_, duration.count());
    //std::string txt = name_ + ":" + std::to_string(duration.count() / 1000000.0f) + " s\n";
    //OutputDebugStringA(txt.c_str());
    //std::cout << name_ << " ended. Duration: " << duration.count() << " us\n";
}

void TimerOutput::add(std::string name, uint64_t duration)
{
    for (auto& m : m_map)
    {
        if (m.name == name)
        {
            m.duration += duration;
            m.times += 1;
            return;
        }

    }

    m_map.push_back(data{ name, duration, 1 });
}

void TimerOutput::print()
{
    //std::sort(m_map.begin(), m_map.end(), [](const data& a, const data& b)
    //    {
    //        return a.duration < b.duration;
    //    });

    for (auto& m : m_map)
    {
        // 格式化为 9 位小数（纳秒精度）
        std::ostringstream time_oss;
        time_oss << std::fixed << std::setprecision(9) << (m.duration / 1000000000.0);
        std::string time_str = time_oss.str();

        // 在 . 后每 3 位插入一个空格
        size_t dot_pos = time_str.find('.');
        if (dot_pos != std::string::npos) {
            for (size_t i = dot_pos + 4; i < time_str.length(); i += 4) {
                time_str.insert(i, " ");
            }
        }

        // 构建完整输出字符串
        std::ostringstream oss;
        oss << std::left << std::setw(30) << m.name << ": "
            << std::right << std::setw(12) << time_str << " 秒, "
            << std::right << std::setw(6) << m.times << " 次\n";

        OutputDebugStringA(oss.str().c_str());
    }

    clear();
}
void TimerOutput::clear()
{
    m_map = {};
}

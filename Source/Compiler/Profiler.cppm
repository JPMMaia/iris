export module h.compiler.profiler;

import std;

namespace h::compiler
{
    using Clock = std::chrono::high_resolution_clock;
    using Time_point = Clock::time_point;
    using Duration = Clock::duration;

    export struct Profiler
    {
        std::pmr::vector<std::pair<std::pmr::string, Time_point>> time_points;
        std::pmr::vector<std::pair<std::pmr::string, Duration>> durations;
    };

    export void start_timer(
        Profiler* const profiler,
        std::string_view const key
    )
    {
        if (profiler == nullptr)
            return;

        Time_point const time_point = Clock::now();
        profiler->time_points.push_back(std::make_pair(std::pmr::string{key}, time_point));
    }

    export void end_timer(
        Profiler* const profiler,
        std::string_view const key
    )
    {
        if (profiler == nullptr)
            return;

        Time_point const end_time_point = Clock::now();

        auto const time_point_location = std::find_if(profiler->time_points.begin(), profiler->time_points.end(), [&](auto const& pair) -> bool { return pair.first == key; });
        if (time_point_location == profiler->time_points.end())
            return;

        Time_point const start_time_point = time_point_location->second;
        Duration const duration = end_time_point - start_time_point;

        profiler->durations.push_back(std::make_pair(std::pmr::string{key}, duration));
    }

    export void print_profiler_timings(
        Profiler const* const profiler
    )
    {
        if (profiler == nullptr)
            return;

        std::cout << "<-- Begin Profiler Timings -->\n";
        for (auto const& pair : profiler->durations)
        {
            std::chrono::milliseconds const milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(pair.second);
            std::cout << "    " << pair.first << ": " << milliseconds << '\n';
        }
        std::cout << "<-- End Profiler Timings -->\n";
        std::cout.flush();
    }
}

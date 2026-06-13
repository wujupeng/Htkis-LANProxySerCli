#pragma once
#include <string>
#include <vector>

struct sys_check_result {
    std::string item;
    std::string current_value;
    std::string recommend_value;
    bool is_ok{false};
    std::string suggestion;
};

class system_tuner {
public:
    static system_tuner& instance();

    std::vector<sys_check_result> run_startup_check();
    std::vector<sys_check_result> run_runtime_check();
    std::string generate_report();
    std::vector<sys_check_result> get_results() const;
    std::string generate_tune_script() const;

private:
    system_tuner() = default;
    std::vector<sys_check_result> m_results;
};

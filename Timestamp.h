#pragma once

#include <iostream>
#include <string>

// 时间类
class Timestamp
{
public:
    Timestamp();
    explicit Timestamp(int64_t microSecondsSinceEpoch);

    // 获取当前时间
    static Timestamp now(); 

    // 转换为字符串
    std::string toString() const; 
private:
    int64_t microSecondsSinceEpoch_; // 微秒数
};
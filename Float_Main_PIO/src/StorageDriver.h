#ifndef STORAGE_DRIVER_H
#define STORAGE_DRIVER_H

#include <Arduino.h>
#include <LittleFS.h> // 引入文件系统库

class StorageDriver
{
public:
    void init()
    {
        // 挂载文件系统。如果失败（第一次运行），尝试格式化。
        if (!LittleFS.begin(true))
        {
            Serial.println("Storage: Mount Failed");
            return;
        }
        Serial.println("Storage: Mounted Successfully");
    }

    // 记录一条数据
    // 格式建议: "时间戳,深度"
    void logData(unsigned long time, float depth)
    {
        // 以"追加模式" (FILE_APPEND) 打开文件
        File file = LittleFS.open("/mission_log.txt", FILE_APPEND);
        if (!file)
        {
            Serial.println("Storage: Failed to open file for appending");
            return;
        }

        // 写入一行数据，例如: "12500,2.54\n"
        file.print(time);
        file.print(",");
        file.println(depth, 3);

        file.close(); // 必须关闭，否则可能保存失败
        // 调试打印 (测试时开启，正式比赛为了速度可以注释掉)
        // Serial.printf("Storage: Logged -> %lu, %.2f\n", time, depth);
    }

    // 准备读取数据（打开文件）
    File openFileForRead()
    {
        // 以"读取模式" (FILE_READ) 打开
        return LittleFS.open("/mission_log.txt", FILE_READ);
    }

    // 清空旧数据 (每次新任务开始前调用)
    void clearLogs()
    {
        if (LittleFS.exists("/mission_log.txt"))
        {
            LittleFS.remove("/mission_log.txt");
            Serial.println("Storage: Old logs cleared!");
        }
    }
};
#endif
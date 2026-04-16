#ifndef SENSOR_DRIVER_H
#define SENSOR_DRIVER_H

#include <Arduino.h>
#include <Wire.h>
#include "MS5837.h"
#include "Config.h"

class SensorDriver
{
public:
    // 构造函数
    SensorDriver() {}

    void init()
    {
        Serial.println("Sensor: Initializing...");

        // 1. 启动 I2C
        Wire.begin(PIN_SDA, PIN_SCL);

        // 2. 初始化传感器
        if (!sensor.init())
        {
            Serial.println("Sensor: Failed to init! Checking connection...");
            return;
        }
        sensor.setModel(MS5837::MS5837_02BA);

        // 3. 设置流体密度
        sensor.setFluidDensity(FLUID_DENSITY);

        Serial.println("Sensor: Init Success! (Model forced to 30BA)");

        tare();
    }

    void tare()
    {
        Serial.print("Sensor: Calibrating zero depth (DO NOT SUBMERGE)... ");

        // 丢弃前几次不稳定的读取
        for (int i = 0; i < 3; i++)
        {
            sensor.read();
            delay(10);
        }
        // 读取 10 次求平均值，消除波动噪声
        float sum = 0;
        const int samples = 10;
        for (int i = 0; i < samples; i++)
        {
            sensor.read();
            sum += sensor.depth(); // 获取此时空气中的假深度
            delay(10);
        }

        // 记录空气中的深度偏移量
        depthOffset = sum / samples;
        resetDepthFilter();

        Serial.print("Done. Offset: ");
        Serial.print(depthOffset);
        Serial.println(" m");
    }

    void update()
    {
        // --- 关键点 D: 读取数据 ---
        // 这一步库会自动发送 PDF 第9页的转换命令 (D1, D2)
        // 并读取 ADC 结果，计算温度补偿
        sensor.read();

        // 获取结果
        currentDepthRaw = sensor.depth() - depthOffset; // 单位: m (米)
        currentPressure = sensor.pressure();            // 单位: mbar (毫巴)
        currentTemp = sensor.temperature();             // 单位: C (摄氏度)
        updateDepthFilter(currentDepthRaw);

        // 简单的调试打印 (不要在高速循环里一直开)
        // Serial.print("Depth: "); Serial.println(currentDepth);
    }

    float getDepth()
    {
        return currentDepthRaw;
    }

    float getDepthFilter()
    {
        return currentDepthFiltered;
    }

    float getPressure()
    {
        return currentPressure;
    }

    float getTemp()
    {
        return currentTemp;
    }

private:
    static constexpr size_t kDepthMedianWindowSize = 5;
    static constexpr float kDepthEmaAlpha = 0.35f;

    MS5837 sensor; // 实例化库对象
    float depthOffset = 0.0;
    float currentDepthRaw = 0.0;
    float currentDepthFiltered = 0.0;
    float currentPressure = 0.0;
    float currentTemp = 0.0;
    float depthWindow[kDepthMedianWindowSize] = {0.0f};
    size_t depthWindowCount = 0;
    size_t depthWindowIndex = 0;
    bool depthFilterReady = false;

    void resetDepthFilter()
    {
        currentDepthRaw = 0.0f;
        currentDepthFiltered = 0.0f;
        depthWindowCount = 0;
        depthWindowIndex = 0;
        depthFilterReady = false;

        for (size_t i = 0; i < kDepthMedianWindowSize; ++i)
        {
            depthWindow[i] = 0.0f;
        }
    }

    void updateDepthFilter(float rawDepth)
    {
        depthWindow[depthWindowIndex] = rawDepth;
        depthWindowIndex = (depthWindowIndex + 1) % kDepthMedianWindowSize;

        if (depthWindowCount < kDepthMedianWindowSize)
        {
            depthWindowCount++;
        }

        float sortedWindow[kDepthMedianWindowSize];
        for (size_t i = 0; i < depthWindowCount; ++i)
        {
            sortedWindow[i] = depthWindow[i];
        }

        for (size_t i = 1; i < depthWindowCount; ++i)
        {
            float value = sortedWindow[i];
            size_t j = i;
            while (j > 0 && sortedWindow[j - 1] > value)
            {
                sortedWindow[j] = sortedWindow[j - 1];
                j--;
            }
            sortedWindow[j] = value;
        }

        size_t mid = depthWindowCount / 2;
        float medianDepth = sortedWindow[mid];
        if ((depthWindowCount % 2) == 0)
        {
            medianDepth = 0.5f * (sortedWindow[mid - 1] + sortedWindow[mid]);
        }

        if (!depthFilterReady)
        {
            currentDepthFiltered = medianDepth;
            depthFilterReady = true;
            return;
        }

        currentDepthFiltered +=
            kDepthEmaAlpha * (medianDepth - currentDepthFiltered);
    }
};

#endif

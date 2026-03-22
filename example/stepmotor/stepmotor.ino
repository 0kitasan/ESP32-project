#include <AccelStepper.h>

// 定义引脚
const int pulPin = 4; // D2
const int dirPin = 5; // D3

// 初始化库。注意：因为是共阳极接法，信号是反向的（LOW有效），
// 但 AccelStepper 库会自动处理，通常不需要特殊设置。
AccelStepper stepper(AccelStepper::DRIVER, pulPin, dirPin);

void setup() {
  // 设置最大速度（不要设太快，先测试 800 步/秒）
  stepper.setMaxSpeed(800); 
  // 设置加速度
  stepper.setAcceleration(400); 
  
  // 假设你设置了 1600 细分，这里就是让它转一圈
  stepper.moveTo(1600); 
}

void loop() {
  // 到达目标位置后切换方向
  if (stepper.distanceToGo() == 0) {
    delay(1000); // 停顿1秒观察
    stepper.moveTo(-stepper.currentPosition()); 
  }

  // 驱动电机运行
  stepper.run();
}


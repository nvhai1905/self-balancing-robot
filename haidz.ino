// Self Balancing Robot
#include <PID_v1.h>
#include <LMotorController.h>
#include "I2Cdev.h"
#include "MPU6050_6Axis_MotionApps20.h"

#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    #include "Wire.h"
#endif

#define MIN_ABS_SPEED 30

MPU6050 mpu;

// MPU control/status vars
bool dmpReady = false;
uint8_t mpuIntStatus;
uint8_t devStatus;
uint16_t packetSize;
uint16_t fifoCount;
uint8_t fifoBuffer[64];

// orientation/motion vars
Quaternion q;
VectorFloat gravity;
float ypr[3];

// PID
double originalSetpoint = 178;
double setpoint = originalSetpoint;
double movingAngleOffset = 1;
double input, output;

// Adjust these values to fit your own design
double Kp = 40;
double Kd = 1.3;
double Ki = 150;
PID pid(&input, &output, &setpoint, Kp, Ki, Kd, DIRECT);

double motorSpeedFactorLeft = 0.5;
double motorSpeedFactorRight = 0.5;

// Motor Controller
int ENA = 11;
int IN1 = 7;
int IN2 = 6;
int IN3 = 5;
int IN4 = 4;
int ENB = 10;
LMotorController motorController(
    ENA, IN1, IN2,
    ENB, IN3, IN4,
    motorSpeedFactorLeft, motorSpeedFactorRight
);

volatile bool mpuInterrupt = false;

void dmpDataReady() {
    mpuInterrupt = true;
}
// Hàm điều khiển robot
void forward() {
    setpoint = originalSetpoint - movingAngleOffset; // Nghiêng về trước
}

void backward() {
    setpoint = originalSetpoint + movingAngleOffset; // Nghiêng về sau
}

void left() {
    motorController.turnLeft(100, false); // Quay trái với tốc độ 100
}

void right() {
    motorController.turnRight(100, false); // Quay phải với tốc độ 100
}

void stop() {
    setpoint = originalSetpoint; // Đặt lại góc mục tiêu
    motorController.stopMoving(); // Dừng động cơ
}

void setup() {

    #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
        Wire.begin();
        TWBR = 24; // 400kHz I2C clock (200kHz if CPU is 8MHz)
    #elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
        Fastwire::setup(400, true);
    #endif
    Serial.begin(9600);
    mpu.initialize();
    devStatus = mpu.dmpInitialize();

    // Set gyro offsets (adjust as needed)
    mpu.setXGyroOffset(0);
    mpu.setYGyroOffset(0);
    mpu.setZGyroOffset(0);
    mpu.setZAccelOffset(0); // Default: 1688 for test chip

    if (devStatus == 0) {
        mpu.setDMPEnabled(true);
        attachInterrupt(0, dmpDataReady, RISING);
        mpuIntStatus = mpu.getIntStatus();
        dmpReady = true;
        packetSize = mpu.dmpGetFIFOPacketSize();

        // Setup PID
        pid.SetMode(AUTOMATIC);
        pid.SetSampleTime(10);
        pid.SetOutputLimits(-255, 255);
    } else {
        Serial.print(F("DMP Initialization failed (code "));
        Serial.print(devStatus);
        Serial.println(F(")"));
    }
}

void loop() {
    if (!dmpReady) return;
    // Ưu tiên điều khiển dựa trên góc
        if (input > 174 && input < 176) {
            forward();
            Serial.println("Ưu tiên tiến");
        } else if (input >= 180 && input < 182) {
            backward(); 
            Serial.println("Ưu tiên lùi");
        } else {
            stop(); // Ưu tiên cân bằng khi ngoài khoảng góc
            Serial.println("Ưu tiên cân bằng");
        }

    // Kiểm tra lệnh từ WinForm qua Serial
    if (Serial.available() > 0) {
        char command = Serial.read();
        switch (command) {
            case 'F':
                forward();
                break;
            case 'B':
                backward();
                break;
            case 'L':
                left();
                break;
            case 'R':
                right();
                break;
            case 'S':
                stop();
                break;
        }
    }

    // Wait for MPU interrupt or extra packet(s)
    while (!mpuInterrupt && fifoCount < packetSize) {
        pid.Compute();
        motorController.move(output, MIN_ABS_SPEED);
    }

    mpuInterrupt = false;
    mpuIntStatus = mpu.getIntStatus();
    fifoCount = mpu.getFIFOCount();

    // FIFO overflow
    if ((mpuIntStatus & 0x10) || fifoCount == 1024) {
        mpu.resetFIFO();
        Serial.println(F("FIFO overflow!"));
    }
    // DMP data ready
    else if (mpuIntStatus & 0x02) {
        while (fifoCount < packetSize)
            fifoCount = mpu.getFIFOCount();

        mpu.getFIFOBytes(fifoBuffer, packetSize);
        fifoCount -= packetSize;

        mpu.dmpGetQuaternion(&q, fifoBuffer);
        mpu.dmpGetGravity(&gravity, &q);
        mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);

        input = ypr[1] * 180 / M_PI + 180;
        Serial.print("Angle: ");
        Serial.print(input);
      
    }
}

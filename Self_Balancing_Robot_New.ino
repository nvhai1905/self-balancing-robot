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
double originalSetpoint = 178.0;
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

// MOTOR CONTROLLER
int ENA = 11;
int IN1 = 7;
int IN2 = 6;
int IN3 = 5;
int IN4 = 4;
int ENB = 10;
LMotorController motorController(ENA, IN1, IN2, ENB, IN3, IN4, motorSpeedFactorLeft, motorSpeedFactorRight);

// Biến để lưu trạng thái điều khiển
int currentSpeed = 0;
bool isTurningLeft = false;
bool isTurningRight = false;

volatile bool mpuInterrupt = false;
void dmpDataReady()
{
    mpuInterrupt = true;
}

void setup()
{
    Serial.begin(115200);

    // Join I2C bus
    #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    Wire.begin();
    TWBR = 24; // 400kHz I2C clock (200kHz if CPU is 8MHz)
    #elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
    Fastwire::setup(400, true);
    #endif

    mpu.initialize();
    devStatus = mpu.dmpInitialize();

    // Supply your own gyro offsets here
    mpu.setXGyroOffset(0);
    mpu.setYGyroOffset(0);
    mpu.setZGyroOffset(0);
    mpu.setZAccelOffset(0);

    if (devStatus == 0)
    {
        mpu.setDMPEnabled(true);
        attachInterrupt(0, dmpDataReady, RISING);
        mpuIntStatus = mpu.getIntStatus();
        dmpReady = true;
        packetSize = mpu.dmpGetFIFOPacketSize();

        // Setup PID
        pid.SetMode(AUTOMATIC);
        pid.SetSampleTime(10);
        pid.SetOutputLimits(-255, 255);
    }
    else
    {
        Serial.print(F("DMP Initialization failed (code "));
        Serial.print(devStatus);
        Serial.println(F(")"));
    }
}

// Hàm điều khiển xe

void moveForward()
{
    setpoint = originalSetpoint - movingAngleOffset;
    isTurningLeft = false;
    isTurningRight = false;
    Serial.println("Moving forward");
}

void moveBackward()
{
    setpoint = originalSetpoint + movingAngleOffset;
    isTurningLeft = false;
    isTurningRight = false;
    Serial.println("Moving backward");
}

void turnLeft()
{
    isTurningLeft = true;
    isTurningRight = false;
    Serial.println("Turning left");
}

void turnRight()
{
    isTurningLeft = false;
    isTurningRight = true;
    Serial.println("Turning right");
}

void stopMotors()
{
    setpoint = originalSetpoint;
    isTurningLeft = false;
    isTurningRight = false;
    Serial.println("Stopping");
}

void loop()
{
    // Nhận lệnh từ Serial
    if (Serial.available() > 0)
    {
        String command = Serial.readStringUntil('\n');
        command.trim();

        if (command == "FWD")
        {
            moveForward();
        }
        else if (command == "BACK")
        {
            moveBackward();
        }
        else if (command == "LEFT")
        {
            turnLeft();
        }
        else if (command == "RIGHT")
        {
            turnRight();
        }
        else
        {
            stopMotors();
        }
    }

    if (!dmpReady) return;

    while (!mpuInterrupt && fifoCount < packetSize)
    {
        pid.Compute();
        currentSpeed = output;

        if (isTurningLeft)
        {
            int leftSpeed = currentSpeed * 0.3;
            int rightSpeed = currentSpeed * 0.7;
            motorController.move(currentSpeed - leftSpeed, MIN_ABS_SPEED);
        }
        else if (isTurningRight)
        {
            int leftSpeed = currentSpeed * 0.7;
            int rightSpeed = currentSpeed * 0.3;
            motorController.move(currentSpeed - rightSpeed, MIN_ABS_SPEED);
        }
        else
        {
            motorController.move(currentSpeed, MIN_ABS_SPEED);
        }
    }

    mpuInterrupt = false;
    mpuIntStatus = mpu.getIntStatus();
    fifoCount = mpu.getFIFOCount();

    if ((mpuIntStatus & 0x10) || fifoCount == 1024)
    {
        mpu.resetFIFO();
        Serial.println(F("FIFO overflow!"));
    }
    else if (mpuIntStatus & 0x02)
    {
        while (fifoCount < packetSize) fifoCount = mpu.getFIFOCount();
        mpu.getFIFOBytes(fifoBuffer, packetSize);
        fifoCount -= packetSize;

        mpu.dmpGetQuaternion(&q, fifoBuffer);
        mpu.dmpGetGravity(&gravity, &q);
        mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
        input = ypr[1] * 180 / M_PI + 180;

        // Gửi góc lệch qua Serial với định dạng "ANGLE:<giá trị>"
        Serial.print("ANGLE:");
        Serial.println(input, 2); // Gửi giá trị góc với 2 chữ số thập phân
    }
}
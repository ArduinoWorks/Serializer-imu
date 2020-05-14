#include "LSM9DS1.h"
#include <I2C.h>
#include <StandardCplusplus.h>
#include <string>
#include <sstream>

#define GET_VAR_NAME(Variable) (#Variable)

I2C Bus;
LSM9DS1 Sensor(Bus);

void setup()
{

    pinMode(LED_BUILTIN, OUTPUT);

    Serial.begin(1286400);

    Bus.begin();
    Bus.setSpeed(true);
    Bus.timeOut(10);

    Sensor.Set_CTRL_REG1_G(LSM9DS1::GyroODR::ODR_952, LSM9DS1::GyroFullscale::FS_2000);
    Sensor.Set_CTRL_REG2_G(LSM9DS1::GyroInterruptGeneration::None, LSM9DS1::GyroOutDataConfiguration::OutAfterLPF1);
    Sensor.Set_CTRL_REG3_G(LSM9DS1::GyroLowPowerMode::Disabled);
    Sensor.Set_ORIENT_CFG_G(LSM9DS1::GyroAxisSign::Negative, LSM9DS1::GyroAxisSign::Positive, LSM9DS1::GyroAxisSign::Positive, LSM9DS1::GyroAxisOrder::XYZ);

    Sensor.Set_REG5_XL(LSM9DS1::AccelDecimation::NoDecimation);
    Sensor.Set_REG6_XL(LSM9DS1::AccelODR::ODR_952, LSM9DS1::AccelFullscale::G_2, LSM9DS1::AccelAntiAliasingBandwidth::BW_408);
    Sensor.Set_REG7_XL(LSM9DS1::AccelHighResolutionMode::Enabled, LSM9DS1::AccelFilterCutoff::ODR_TO_9, LSM9DS1::AccelDataSelection::Bypassed);

    Sensor.Set_REG1_M(LSM9DS1::MagnetTempCompensation::Enabled, LSM9DS1::MagnetAxisOperativeMode::UltraHighPerfomance, LSM9DS1::MagnetODR::ODR_80, LSM9DS1::MagnetFastODRMode::Enabled);
    Sensor.Set_REG2_M(LSM9DS1::MagnetFullscale::GAUSS_16);
    Sensor.Set_REG3_M(LSM9DS1::MagnetLowPowerMode::Disabled, LSM9DS1::MagnetOperatingMode::ContinuousConversion);
    Sensor.Set_REG4_M(LSM9DS1::MagnetAxisOperativeMode::UltraHighPerfomance);
    Sensor.Set_REG5_M(LSM9DS1::MagnetDataUpdate::Continuous);
}

unsigned long oldtime = 0;
unsigned long newtime = 0;
unsigned long t = 0;
bool led = false;

void loop()
{
    t++;
    //delay(1000);

    if (t % 100 == 0)
    {
        pulse();
        newtime = micros();
        Serial.print("******************************************************t");
        Serial.println(((float)(newtime - oldtime)) / (1000 * t));
        t = 0;
        oldtime = micros();
    }

    Sensor.Update();

    Sensor.Accelerometer.X = 0x5A;
    Sensor.Accelerometer.Y = 0x5A;
    Sensor.Accelerometer.Z = 0x5A;
    Sensor.Gyroscope.X = 0x5A;
    Sensor.Gyroscope.Y = 0x5A;
    Sensor.Gyroscope.Z = 0x5A;
    Sensor.Magnetometer.X = 0x5A;
    Sensor.Magnetometer.Y = 0x5A;
    Sensor.Magnetometer.Z = 0x5A;

    Serial.write(0x3C);
    Serial.write(Sensor.Accelerometer.X >> 8);
    Serial.write(Sensor.Accelerometer.X & 0xFF);
    Serial.write("\t");
    Serial.write(Sensor.Accelerometer.Y >> 8);
    Serial.write(Sensor.Accelerometer.Y & 0xFF);
    Serial.write("\t");
    Serial.write(Sensor.Accelerometer.Z >> 8);
    Serial.write(Sensor.Accelerometer.Y & 0xFF);
    Serial.write("\t");
    Serial.write(Sensor.Gyroscope.X >> 8);
    Serial.write(Sensor.Gyroscope.X & 0xFF);
    Serial.write("\t");
    Serial.write(Sensor.Gyroscope.Y >> 8);
    Serial.write(Sensor.Gyroscope.Y & 0xFF);
    Serial.write("\t");
    Serial.write(Sensor.Gyroscope.Z >> 8);
    Serial.write(Sensor.Gyroscope.Z & 0xFF);
    Serial.write("\t");
    Serial.write(Sensor.Magnetometer.X >> 8);
    Serial.write(Sensor.Magnetometer.X & 0xFF);
    Serial.write("\t");
    Serial.write(Sensor.Magnetometer.Y >> 8);
    Serial.write(Sensor.Magnetometer.Y & 0xFF);
    Serial.write("\t");
    Serial.write(Sensor.Magnetometer.Z >> 8);
    Serial.write(Sensor.Magnetometer.Z & 0xFF);
    Serial.write(0x3E);
    Serial.write("\n");
    
}

void pulse()
{
    led = !led;
    switch (led)
    {
    case true:

        digitalWrite(LED_BUILTIN, HIGH);
        break;

    case false:

        digitalWrite(LED_BUILTIN, LOW);
        break;
    }
}
/*
 *
 *    Copyright (c) 2023 Project CHIP Authors
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */
/*Modified by NXP on 2026: added support for P3T1755 I2C temperature sensor on the FRDM-RW612*/

#include "TemperatureSensorManager.h"
#include "AppTask.h"

#include "fsl_p3t1755.h"
#include "fsl_i2c.h"
#include "RW612.h"
#include "pin_mux.h"
#include "fsl_common.h"
#include "fsl_io_mux.h"

using namespace chip;
using namespace ::chip::DeviceLayer;

constexpr float kMinTemperatureDelta          = 0.5; // 0.5 degree Celsius
static float mSimulatedTemperature            = 25.5;
double temperature;


#define EXAMPLE_I2C_MASTER_BASE		I2C2
#define I2C_MASTER_CLOCK_FREQUENCY CLOCK_GetFlexCommClkFreq(2U)
#define I2C_TIME_OUT_INDEX 100000000U
#define SENSOR_SLAVE_ADDR          0x48U
#define EXAMPLE_I2C_MASTER ((I2C_Type *)EXAMPLE_I2C_MASTER_BASE)

#define CCC_RSTDAA  0x06U
#define CCC_SETDASA 0x87U

#define I2C_BAUDRATE 100000U

volatile status_t g_completionStatus;
volatile bool g_MasterCompletionFlag;
i2c_master_handle_t g_i2c_m_handle;
p3t1755_handle_t p3t1755Handle;

TaskHandle_t xTempSensorTaskHandle = nullptr;


void TemperatureSensorManager::Notify_Read_Completed(intptr_t arg)
{
    if (xTempSensorTaskHandle != nullptr)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(xTempSensorTaskHandle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

TemperatureSensorManager TemperatureSensorManager::sTemperatureSensorManager;

status_t I2C_WriteSensor(uint8_t deviceAddress, uint32_t regAddress, uint8_t *regData, size_t dataSize)
{
    status_t result                  = kStatus_Success;
    i2c_master_transfer_t masterXfer = {0};
    uint32_t timeout                 = 0U;

    masterXfer.slaveAddress   = deviceAddress;
    masterXfer.direction      = kI2C_Write;
    masterXfer.subaddress     = regAddress;
    masterXfer.subaddressSize = 1;
    masterXfer.data           = regData;
    masterXfer.dataSize       = dataSize;
    masterXfer.flags          = kI2C_TransferDefaultFlag;

    g_MasterCompletionFlag = false;
    g_completionStatus     = kStatus_Success;
    result                 = I2C_MasterTransferNonBlocking(EXAMPLE_I2C_MASTER, &g_i2c_m_handle, &masterXfer);
    if (kStatus_Success != result)
    {
        return result;
    }

    while (!g_MasterCompletionFlag)
    {
    }

    return result;
}

status_t I2C_ReadSensor(uint8_t deviceAddress, uint32_t regAddress, uint8_t *regData, size_t dataSize)
{
    status_t result                  = kStatus_Success;
    i2c_master_transfer_t masterXfer = {0};

    masterXfer.slaveAddress   = deviceAddress;
    masterXfer.direction      = kI2C_Read;
    masterXfer.subaddress     = regAddress;
    masterXfer.subaddressSize = 1;
    masterXfer.data           = regData;
    masterXfer.dataSize       = dataSize;
    masterXfer.flags          = kI2C_TransferDefaultFlag;

    g_MasterCompletionFlag = false;
    g_completionStatus     = kStatus_Success;
    result = I2C_MasterTransferNonBlocking(EXAMPLE_I2C_MASTER, &g_i2c_m_handle, &masterXfer);
    
    while(!g_MasterCompletionFlag)
    {
        //wait
    }

    return result;
}


static void I2C_Done_Callback(I2C_Type *base, i2c_master_handle_t *handle, status_t status, void *userData)
{
    /* Signal transfer success when received success status. */
    g_completionStatus = status;
    if (status == kStatus_Success)
    {
        g_MasterCompletionFlag = true;
        
    }
    PlatformMgr().ScheduleWork(TemperatureSensorManager::Notify_Read_Completed, 0);
}


CHIP_ERROR TemperatureSensorManager::Init()
{
    // TODO: Initialize temperature sensor
    status_t result = kStatus_Success;
	i2c_master_config_t masterConfig;
	p3t1755_config_t p3t1755Config;

    /* Use 16 MHz clock for the FLEXCOMM2 */
	CLOCK_AttachClk(kSFRO_to_FLEXCOMM2);

    /* I2C PINS INIT */
    IO_MUX_SetPinMux(IO_MUX_FC2_I2C_16_17);

     /* Init I2C */
	I2C_MasterGetDefaultConfig(&masterConfig);
	masterConfig.baudRate_Bps = I2C_BAUDRATE;
	I2C_MasterInit(EXAMPLE_I2C_MASTER, &masterConfig, I2C_MASTER_CLOCK_FREQUENCY);

    I2C_MasterTransferCreateHandle(EXAMPLE_I2C_MASTER, &g_i2c_m_handle,(i2c_master_transfer_callback_t) I2C_Done_Callback, NULL);

    /* P3T1755 Temperature Sensor Config */
	p3t1755Config.writeTransfer = I2C_WriteSensor;
	p3t1755Config.readTransfer  = I2C_ReadSensor;
	p3t1755Config.sensorAddress = SENSOR_SLAVE_ADDR;
	P3T1755_Init(&p3t1755Handle, &p3t1755Config);

    ChipLogProgress(DeviceLayer, "******* Temperature Sensor Initialized *******");
    return CHIP_NO_ERROR;
}

void TemperatureSensorManager::TemperatureRead()
{
    status_t result = kStatus_Success;
 
    result = P3T1755_ReadTemperature(&p3t1755Handle, &temperature);
    if (result != kStatus_Success)
    {
        ChipLogProgress(DeviceLayer, "\r\nP3T1755 read temperature failed.\r\n");
    }
}

int16_t TemperatureSensorManager::GetMeasuredValue()
{
    
    // Per spec Application Clusters 2.3.4.1. : MeasuredValue = 100 x temperature [°C]

	sTemperatureSensorManager.mMeasuredTempCelsius = (int16_t) 100 * temperature;


    return sTemperatureSensorManager.mMeasuredTempCelsius;
}


/*
 *
 *    Copyright (c) 2021-2023 Project CHIP Authors
 *    Copyright (c) 2021 Google LLC.
 *    Copyright 2023-2024, 2026 NXP
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
/*Modified by NXP on 2026: added FreeRTOS task for periodic temperature sensor readings*/

#include "AppTask.h"
#include "CHIPDeviceManager.h"
#include "ICDUtil.h"
#include <app/InteractionModelEngine.h>
#include <app/util/attribute-storage.h>
#include "TemperatureSensorManager.h"
#include <app-common/zap-generated/attributes/Accessors.h>
#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Clusters.h>

#include "FreeRTOS.h"

#ifdef ENABLE_CHIP_SHELL
#include <lib/shell/Engine.h>

using namespace chip::Shell;
#endif /* ENABLE_CHIP_SHELL */

extern TaskHandle_t xTempSensorTaskHandle;

using namespace chip;
using namespace ::chip::DeviceLayer;
using namespace chip::app::Clusters;

CHIP_ERROR cliTemperatureMeasurement(int argc, char * argv[])
{
    ChipLogDetail(Shell, "Get Temperature Sensor Value!");
    TemperatureSensorApp::AppTask::GetDefaultInstance().TemperatureMeasurementHandler();
    return CHIP_NO_ERROR;
}

void TemperatureSensorApp::AppTask::Temp_Sensor_Periodic_Read(void *arg)
{

    xTempSensorTaskHandle = xTaskGetCurrentTaskHandle(); // Register this task for notification

    int16_t temperature = 0;
	while(1)
    {
        TemperatureSensorMgr().TemperatureRead();
        
        // Wait for notification from ISR (with timeout)
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5000)); // Wait up to 5000ms

        temperature = TemperatureSensorMgr().GetMeasuredValue();
    	ChipLogProgress(DeviceLayer, "######## TemperatureMeasurement::Set : %d", temperature);
        app::Clusters::TemperatureMeasurement::Attributes::MeasuredValue::Set(1, temperature);
        vTaskDelay(pdMS_TO_TICKS(5000)); // Delay between reads
    }
	
}

void TemperatureSensorApp::AppTask::PreInitMatterStack()
{
    ChipLogProgress(DeviceLayer, "Welcome to NXP Temperature Sensor Demo App");
}

void TemperatureSensorApp::AppTask::PostInitMatterStack()
{
	CHIP_ERROR err = CHIP_NO_ERROR;
    chip::app::InteractionModelEngine::GetInstance()->RegisterReadHandlerAppCallback(&chip::NXP::App::GetICDUtil());
	if (TemperatureSensorMgr().Init() != CHIP_NO_ERROR)
    {
        ChipLogError(DeviceLayer, "Init TemperatureSensorMgr failed: %s", ErrorStr(err));
    }
    xTaskCreate(Temp_Sensor_Periodic_Read, "temperature_sensor_task", configMINIMAL_STACK_SIZE * 2, NULL, 0 , NULL);


}

void TemperatureSensorApp::AppTask::AppMatter_RegisterCustomCliCommands()
{
#ifdef ENABLE_CHIP_SHELL
    /* Register application commands */
    static const shell_command_t kCommands[] = {
        { .cmd_func = cliTemperatureMeasurement, .cmd_name = "temperature", .cmd_help = "Get mocked temperature sensor value, no argument required" },
    };
    Engine::Root().RegisterCommands(kCommands, sizeof(kCommands) / sizeof(kCommands[0]));
#endif
}

// This returns an instance of this class.
TemperatureSensorApp::AppTask & TemperatureSensorApp::AppTask::GetDefaultInstance()
{
    static TemperatureSensorApp::AppTask sAppTask;
    return sAppTask;
}

chip::NXP::App::AppTaskBase & chip::NXP::App::GetAppTask()
{
    return TemperatureSensorApp::AppTask::GetDefaultInstance();
}

void TemperatureSensorApp::AppTask::TemperatureMeasurementHandler(void)
{
	PlatformMgr().ScheduleWork(TemperatureSensorApp::AppTask::TemperatureMeasurementInternal, 0);
}

void TemperatureSensorApp::AppTask::TemperatureMeasurementInternal(intptr_t arg)
{
	int16_t temperature = 0;
	
	temperature = TemperatureSensorMgr().GetMeasuredValue();
	ChipLogProgress(DeviceLayer, "######## TemperatureMeasurement::Set : %d", temperature);
	
    app::Clusters::TemperatureMeasurement::Attributes::MeasuredValue::Set(1, temperature);
}
/*
This code communicates with VICTRON CERBO GX style devices using CANBUS @ 500kbps and 11 bit addresses.

The code supports the VICTRON CAN BUS BMS style messages.
*/

#define USE_ESP_IDF_LOG 1
static constexpr const char* const TAG = "diybms-victron";

#include "victron_canbus.h"


//just send the hostname of the master controller

void victron_message_370_371_35e()
{
    CANframe candata(TWAI_FRAME_MAX_DLC, 0);  // initialize struct with dlc and identifier
    CANframe* ptrFrame = &candata;


    // message 370
    memset(&candata.data, 0, TWAI_FRAME_MAX_DLC);
    memcpy(&candata.data[0], &hostname[0], TWAI_FRAME_MAX_DLC);
    candata.identifier = 0x370;


    send_canbus_message(ptrFrame);

    // message 371
    memset(&candata.data, 0, TWAI_FRAME_MAX_DLC);
    memcpy(&candata.data[0], &hostname[8], TWAI_FRAME_MAX_DLC);
    candata.identifier = 0x371;


    send_canbus_message(ptrFrame);

    // message 35eCANframe
    memset(&candata.data, 0, TWAI_FRAME_MAX_DLC);
    memcpy(&candata.data[0], &hostname[0], 6);
    candata.identifier = 0x35e;

    send_canbus_message(ptrFrame);
}

// Firmware & Ah capacity
void victron_message_35f()
{
    CANframe candata(TWAI_FRAME_MAX_DLC, 0x35f);  // initialize struct with dlc and identifier
    CANframe* ptrFrame = &candata;

    //if no networked controllers than just copy over the local data
    if (mysettings.controllerNet == 1)
    {
        memcpy(&candata.data[0], &can.data[5][mysettings.controllerID][0], sizeof(uint16_t));          
        memcpy(&candata.data[2], &can.data[5][mysettings.controllerID][2], sizeof(uint16_t));      
        memcpy(&candata.data[4], &can.data[5][mysettings.controllerID][4], sizeof(uint16_t));        
    }


    else
    {
    // Wait for permission from Canbus_RX task to edit data array
    if (!xSemaphoreTake(can.dataMutex[5], pdMS_TO_TICKS(50)))
    {
      ESP_LOGE(TAG, "CANBUS RX/TX intertask notification timeout");
      return;
    }

        //uint16_t BatteryModel      // slice= 0
        //uint16_t Firmwareversion     // slice= 2
        //uint16_t OnlinecapacityinAh       // slice= 4

        // check if firmware versions match and report
        uint8_t mismatches = 0;
        for (int8_t i = 0; i < MAX_NUM_CONTROLLERS; i++)
        {
            if (can.controller_is_online[i] && can.controller_is_integrated[i])  
            {
            mismatches = mismatches + memcmp((uint16_t*)&can.data[5][i][2], (uint16_t*)&can.data[5][i + 1][2], sizeof(uint16_t));
            }
        }

        if (mismatches != 0)
        {
            // should we raise an alarm here??
            // firmware_version_mismatch = true;

            // if firmware versions don't match just display 1's or something...
            memset(&candata.data[2], 1, sizeof(uint16_t));
        }

        else
        {
            // firmware_version_mismatch = false;

            memcpy(&candata.data[2], &can.data[5][mysettings.controllerID][2], sizeof(uint16_t));
        }

        // calculate total Ah based on #of controllers
        uint16_t Total_Weighted_Ah = 0;
        for (int8_t i = 0; i < MAX_NUM_CONTROLLERS; i++)
        {
            if (can.controller_is_online[i] && can.controller_is_integrated[i])  
            {
            Total_Weighted_Ah = Total_Weighted_Ah + .01 * (*(uint16_t*)&can.data[4][i][0]) * (*(uint16_t*)&can.data[5][i][4]);  // .01 * SOC(%) x Online capacity
            }
        }

        memcpy(&candata.data[4], &Total_Weighted_Ah, sizeof(uint16_t));

    }
        // Return permission to Canbus_RX task 
        xSemaphoreGive(can.dataMutex[5]); 

        send_canbus_message(ptrFrame);

}

// Min/Max Cell V & T I.D.'s
void victron_message_373_374_375_376_377()
{
    CANframe candata(TWAI_FRAME_MAX_DLC, 0);  // initialize struct with dlc and identifier
    CANframe* ptrFrame = &candata;


    if (mysettings.controllerNet == 1)
    {
        // message 373 //
        candata.identifier = 0x373;
        memset(&candata.data, 0, TWAI_FRAME_MAX_DLC);
        memcpy(&candata.data, &can.data[8][mysettings.controllerID][0], TWAI_FRAME_MAX_DLC);

        send_canbus_message(ptrFrame);

        // message 374 //
        candata.identifier = 0x374;
        memset(&candata.data, 0, TWAI_FRAME_MAX_DLC);
        memcpy(&candata.data, &can.data[9][mysettings.controllerID][0], TWAI_FRAME_MAX_DLC);

        send_canbus_message(ptrFrame);

        // message 375 //
        candata.identifier = 0x375;
        memset(&candata.data, 0, TWAI_FRAME_MAX_DLC);
        memcpy(&candata.data, &can.data[10][mysettings.controllerID][0], TWAI_FRAME_MAX_DLC);

        send_canbus_message(ptrFrame);

        // message 376  //
        candata.identifier = 0x376;
        memset(&candata.data, 0, TWAI_FRAME_MAX_DLC);
        memcpy(&candata.data, &can.data[11][mysettings.controllerID][0], TWAI_FRAME_MAX_DLC);

        send_canbus_message(ptrFrame);

        // message 377 //
        candata.identifier = 0x377;
        memset(&candata.data, 0, TWAI_FRAME_MAX_DLC);
        memcpy(&candata.data, &can.data[12][mysettings.controllerID][0], TWAI_FRAME_MAX_DLC);

        send_canbus_message(ptrFrame);    }

    // aggregate DVCC data from networked controllers 
    else
    {

        uint16_t min_cell_v;  // corresponding slice 0_1 incan.data[8][i]
        uint16_t max_cell_v;  // corresponding slice 2_3 incan.data[8][i]
        uint16_t min_cell_t;   // corresponding slice 4_5 incan.data[8][i]
        uint16_t max_cell_t;   // corresponding slice 6_7 incan.data[8][i]

        uint8_t MinCellV_ID[TWAI_FRAME_MAX_DLC];
        uint8_t MaxCellV_ID[TWAI_FRAME_MAX_DLC];
        uint8_t MinCellT_ID[TWAI_FRAME_MAX_DLC];
        uint8_t MaxCellT_ID[TWAI_FRAME_MAX_DLC];

    // Wait for permission from Canbus_RX task to edit data array
    if (
        !(xSemaphoreTake(can.dataMutex[8], pdMS_TO_TICKS(50)) &&
        xSemaphoreTake(can.dataMutex[9], pdMS_TO_TICKS(50)) &&
        xSemaphoreTake(can.dataMutex[10], pdMS_TO_TICKS(50)) &&
        xSemaphoreTake(can.dataMutex[11], pdMS_TO_TICKS(50)) &&
        xSemaphoreTake(can.dataMutex[12], pdMS_TO_TICKS(50)))
        )
    {
      ESP_LOGE(TAG, "CANBUS RX/TX intertask notification timeout")  ;

      xSemaphoreGive(can.dataMutex[8]);
      xSemaphoreGive(can.dataMutex[9]); 
      xSemaphoreGive(can.dataMutex[10]); 
      xSemaphoreGive(can.dataMutex[11]);
      xSemaphoreGive(can.dataMutex[12]);  
      return; 
    } 

        min_cell_v = *(uint16_t*)&can.data[8][mysettings.controllerID][0];
        min_cell_t = *(uint16_t*)&can.data[8][mysettings.controllerID][4];
        max_cell_v = *(uint16_t*)&can.data[8][mysettings.controllerID][2];
        max_cell_t = *(uint16_t*)&can.data[8][mysettings.controllerID][6];


        for (int8_t i = 0; i < MAX_NUM_CONTROLLERS; i++)
        {
            if (can.controller_is_online[i] && can.controller_is_integrated[i])  
            {
                // find minimums
                if ((*(uint16_t*)&can.data[8][i][0] <= min_cell_v))  
                {
                    min_cell_v = *(uint16_t*)&can.data[8][i][0];
                    memcpy(&MinCellV_ID, &can.data[9][i], sizeof(MinCellV_ID));
                }
                if ((*(uint16_t*)&can.data[8][i][4] <= min_cell_t))
                {
                    min_cell_t = *(uint16_t*)&can.data[8][i][4];
                    memcpy(&MinCellT_ID, &can.data[11][i], sizeof(MinCellT_ID));
                }

                // find maximums
                if ((*(uint16_t*)&can.data[8][i][2] >= max_cell_v))
                {
                    max_cell_v = *(uint16_t*)&can.data[8][i][2];
                    memcpy(&MaxCellV_ID, &can.data[10][i], sizeof(MaxCellV_ID));
                }
                if ((*(uint16_t*)&can.data[8][i][6] >= max_cell_t))
                {
                    max_cell_t = *(uint16_t*)&can.data[8][i][6];
                    memcpy(&MaxCellT_ID, &can.data[12][i], sizeof(MaxCellT_ID));
                }
            }
        }

        xSemaphoreGive(can.dataMutex[8]);
        xSemaphoreGive(can.dataMutex[9]); 
        xSemaphoreGive(can.dataMutex[10]); 
        xSemaphoreGive(can.dataMutex[11]);
        xSemaphoreGive(can.dataMutex[12]);  

        // message 373  (Min/Max Cell V & T)
        memset(&candata.data, 0, TWAI_FRAME_MAX_DLC);
        candata.identifier = 0x373;
        memcpy(&candata.data[0], &min_cell_v, sizeof(min_cell_v));
        memcpy(&candata.data[2], &max_cell_v, sizeof(max_cell_v));
        memcpy(&candata.data[4], &min_cell_t, sizeof(min_cell_t));
        memcpy(&candata.data[6], &max_cell_t, sizeof(max_cell_t));

        send_canbus_message(ptrFrame);

        // message 374  (Min Cell V I.D.)
        memset(&candata.data, 0, TWAI_FRAME_MAX_DLC);
        candata.identifier = 0x374;
        memcpy(&candata.data, &MinCellV_ID, sizeof(MinCellV_ID));

        send_canbus_message(ptrFrame);

        // message 375  (Max Cell V I.D.)
        memset(&candata.data, 0, TWAI_FRAME_MAX_DLC);
        memcpy(&candata.data, &MaxCellV_ID, sizeof(MaxCellV_ID));
        candata.identifier = 0x375;

        send_canbus_message(ptrFrame);

        // message 376  (Min Cell T I.D.)
        memset(&candata.data, 0, TWAI_FRAME_MAX_DLC);
        memcpy(&candata.data, &MinCellT_ID, sizeof(MinCellT_ID));
        candata.identifier = 0x376;

        send_canbus_message(ptrFrame);

        // message 377  (Max Cell T I.D.)
        memset(&candata.data, 0, TWAI_FRAME_MAX_DLC);
        memcpy(&candata.data, &MaxCellT_ID, sizeof(MaxCellT_ID));
        candata.identifier = 0x377;

        send_canbus_message(ptrFrame);
    }
}

// CVL & CCL implementation
/*
See ControllerCAN section for detailed description of DVCC parameters for each controller

For aggregation:

For the maxchargevoltage reported to the inverter we will just use the minimum maxchargevoltage reported by any controller.

For the maxchargecurrent reported to the inverter, we will use the minimum maxchargecurrent reported by any controller multiplied by the number of conrollers online.

*/
void  victron_message_351()
{
    CANframe candata(TWAI_FRAME_MAX_DLC, 0x351);  // initialize struct with dlc and identifier
    CANframe* ptrFrame = &candata;

    uint16_t chargevoltagelimit = 0;
    int16_t maxchargecurrent = 0;
    int16_t maxdischargecurrent = 0;
    uint16_t dischargevoltage = 0;
    uint8_t integrated_count = 0;

    // if no controllers are networked then just send the local values. It won't matter what controllerID is selected on the config page
    if (mysettings.controllerNet == 1)
    {
        memcpy(&candata.data, &can.data[0][mysettings.controllerID][0], TWAI_FRAME_MAX_DLC);
        memcpy(&chargevoltagelimit, &can.data[0][mysettings.controllerID][0], sizeof(chargevoltagelimit));
        memcpy(&maxchargecurrent, &can.data[0][mysettings.controllerID][2], sizeof(maxchargecurrent));
        memcpy(&maxdischargecurrent, &can.data[0][mysettings.controllerID][4], sizeof(maxdischargecurrent));
        memcpy(&dischargevoltage, &can.data[0][mysettings.controllerID][6], sizeof(dischargevoltage));
    }

    // aggregate DVCC data from networked controllers and use the minimum for each parameter
    else
    {       
        // Wait for permission from Canbus_RX task to edit data array
        if (!xSemaphoreTake(can.dataMutex[0], pdMS_TO_TICKS(50)))
        {
        ESP_LOGE(TAG, "CANBUS RX/TX intertask notification timeout")  ;
        return; 
        }

        chargevoltagelimit = *(uint16_t*)&can.data[0][mysettings.controllerID][0];
        maxchargecurrent = *(int16_t*)&can.data[0][mysettings.controllerID][2];
        maxdischargecurrent = *(int16_t*)&can.data[0][mysettings.controllerID][4];
        dischargevoltage = *(uint16_t*)&can.data[0][mysettings.controllerID][6];

        for (int8_t i = 0; i < MAX_NUM_CONTROLLERS; i++)
        {
            //only include online controllers (must be present on CANBUS and have one|both Charge/Discharge Allowed)
            if (can.controller_is_online[i] && can.controller_is_integrated[i])  
            {
                integrated_count++;
                
                // compare and use minimum
                if (*(uint16_t*)&can.data[0][i][0] < chargevoltagelimit)
                {
                    chargevoltagelimit = *(uint16_t*)&can.data[0][i][0];
                }

                // use lowest reported value from controllers multiplied by the number of controllers.
                if (*(int16_t*)&can.data[0][i][2] < maxchargecurrent)
                {
                    maxchargecurrent = *(int16_t*)&can.data[0][i][2];
                }

                // use lowest reported value from controllers multiplied by the number of controllers
                if (*(int16_t*)&can.data[0][i][4] < maxdischargecurrent) 
                {
                    maxdischargecurrent = *(int16_t*)&can.data[0][i][4];
                }

                // use minimum
                if (*(uint16_t*)&can.data[0][i][6] < dischargevoltage)  
                {
                    dischargevoltage = *(uint16_t*)&can.data[0][i][6];
                }
            }
        }

        if (integrated_count > 0)
        {
            maxchargecurrent = maxchargecurrent * integrated_count;
            maxdischargecurrent = maxdischargecurrent * integrated_count;
        }

    }


        // Return permission to Canbus_RX task 
        xSemaphoreGive(can.dataMutex[0]);     

        memcpy(&candata.data[0], &chargevoltagelimit, sizeof(chargevoltagelimit));                 
        memcpy(&candata.data[2], &maxchargecurrent, sizeof(maxchargecurrent));
        memcpy(&candata.data[4], &maxdischargecurrent, sizeof(maxdischargecurrent));
        memcpy(&candata.data[6], &dischargevoltage, sizeof(dischargevoltage));      

        ESP_LOGI(TAG, "Charge Voltage Limit = %d",chargevoltagelimit);
        ESP_LOGI(TAG, "Max Charge Current = %d",maxchargecurrent);
        ESP_LOGI(TAG, "Max Discharge Current = %d",maxdischargecurrent);
        ESP_LOGI(TAG, "Discharge Voltage Limit = %d",dischargevoltage);     

        send_canbus_message(ptrFrame);
}

// SOC & SOH
void victron_message_355()
{
    CANframe candata(4, 0x355);  // initialize struct with dlc and identifier
    CANframe* ptrFrame = &candata;

    if (mysettings.controllerNet == 1)
    {
        memcpy(&candata.data, &can.data[4][mysettings.controllerID][0], candata.dlc);
    }

    else
    { 
    // Wait for permission from Canbus_RX task to edit data array
    if (!(xSemaphoreTake(can.dataMutex[4], pdMS_TO_TICKS(50)) &&
        xSemaphoreTake(can.dataMutex[5], pdMS_TO_TICKS(50))))
    {
      ESP_LOGE(TAG, "CANBUS RX/TX intertask notification timeout")  ;

      xSemaphoreGive(can.dataMutex[4]);
      xSemaphoreGive(can.dataMutex[5]);  
      return; 
    }
        
        //SOC (weighted average based on nominal Ah of each controller)
        //SOH (display the minmimum health value of all online packs which could be more useful than an averaged health value)
        uint16_t Total_Ah = 0;
        uint32_t Total_Weighted_Ah = 0;
        uint16_t Weighted_SOC = 0;
        uint16_t SOH = *(uint16_t*)&can.data[4][mysettings.controllerID][2];  //start with this controllers SOH

        for (int8_t i = 0; i < MAX_NUM_CONTROLLERS; i++)
        {
            if (can.controller_is_online[i] && can.controller_is_integrated[i])
            {
            Total_Ah = Total_Ah + *(uint16_t*)&can.data[5][i][4];
            Total_Weighted_Ah = Total_Weighted_Ah + (*(uint16_t*)&can.data[4][i][0]) * (*(uint16_t*)&can.data[5][i][4]);  // SOC(%) x Online capacity

            // use minimum
            if (*(uint16_t*)&can.data[4][i][2] < SOH)
            {
                SOH = *(uint16_t*)&can.data[4][i][2];
            }
            }

        }
       
        // Return permission to Canbus_RX task 
        xSemaphoreGive(can.dataMutex[4]);
        xSemaphoreGive(can.dataMutex[5]);  

        if (Total_Ah != 0)  //avoid divide by zero (we won't have useable values during CAN initialization)
        {
            Weighted_SOC = Total_Weighted_Ah / Total_Ah;
        }

        memcpy(&candata.data[0], &Weighted_SOC, sizeof(Weighted_SOC));
        memcpy(&candata.data[2], &SOH, sizeof(SOH));
    }
  
    send_canbus_message(ptrFrame);
}

// Battery V, I, T
void victron_message_356()
{
    CANframe candata(6, 0x356);  // initialize struct with dlc and identifier
    CANframe* ptrFrame = &candata;

    if (mysettings.controllerNet == 1)
    {
        memcpy(&candata.data, &can.data[6][mysettings.controllerID][0], candata.dlc);
    }

    else
    
    {
         int16_t voltage = 0;
         int16_t current = 0;
         int16_t temperature = 0;
         int8_t integrated_count = 0;

        // Wait for permission from Canbus_RX task to edit data array
        if (!xSemaphoreTake(can.dataMutex[6], pdMS_TO_TICKS(50)))
        {
        ESP_LOGE(TAG, "CANBUS RX/TX intertask notification timeout")  ;
        return; 
        }

        for (int8_t i = 0; i < MAX_NUM_CONTROLLERS; i++)
        {
            if (can.controller_is_online[i] && can.controller_is_integrated[i])  // only use values from online controllers
            {
                integrated_count++;

                voltage = voltage + *(int16_t*)&can.data[6][i][0];
                current = current + *(int16_t*)&can.data[6][i][2];
                temperature = temperature + *(int16_t*)&can.data[6][i][4];
            }
        }

        // DEBUGGING PURPOSES ONLY
        ESP_LOGE(TAG, "total voltage = %d AND integrated_count =%d", voltage, integrated_count);


        
        // Return permission to Canbus_RX task 
        xSemaphoreGive(can.dataMutex[6]); 

        if (integrated_count > 0)
        {
        voltage = voltage / integrated_count;
        temperature = temperature / integrated_count;
        }

        memcpy(&candata.data[0], &voltage, sizeof(voltage));
        memcpy(&candata.data[2], &current, sizeof(current));
        memcpy(&candata.data[4], &temperature, sizeof(temperature));
    }

    send_canbus_message(ptrFrame);

}

// # modules OK
void victron_message_372()
{
    CANframe candata(TWAI_FRAME_MAX_DLC, 0x372);  // initialize struct with dlc and identifier
    CANframe* ptrFrame = &candata;

    if (mysettings.controllerNet == 1)
    {
        memcpy(&candata.data, &can.data[3][mysettings.controllerID][0], candata.dlc);
    }

    else
    {
    // Wait for permission from Canbus_RX task to edit data array
    if (!xSemaphoreTake(can.dataMutex[3], pdMS_TO_TICKS(50)))
    {
      ESP_LOGE(TAG, "CANBUS RX/TX intertask notification timeout")  ;
      return; 
    }

        uint16_t online_count = 0;
        // uint16_t numberofmodulesblockingcharge = 0;
       // uint16_t numberofmodulesblockingdischarge = 0;  
        uint16_t offline_count = 0;


        // # Online Modules

        for (int8_t i = 0; i < MAX_NUM_CONTROLLERS; i++)
        {
            if (can.controller_is_online[i] && can.controller_is_integrated[i])  // only use 0 values from online controllers
            {
            online_count = online_count + *(uint16_t*)&can.data[3][i][0];
            offline_count = offline_count + *(uint16_t*)&can.data[3][i][6];
            }
        }
 
        // Return permission to Canbus_RX task 
        xSemaphoreGive(can.dataMutex[3]); 
        
        memcpy(&candata.data[0], &online_count, sizeof(online_count));
        memcpy(&candata.data[6], &offline_count, sizeof(offline_count));

    }

    send_canbus_message(ptrFrame);

}

// Alarms
void victron_message_35a()
{

    CANframe candata(TWAI_FRAME_MAX_DLC, 0x35a);  // initialize struct with dlc and identifier
    CANframe* ptrFrame = &candata;

    uint8_t bitmask, byte;

    memset(&candata.data, 0, TWAI_FRAME_MAX_DLC);

        if (mysettings.controllerNet == 1)
    {
        memcpy(&candata.data, &can.data[1][mysettings.controllerID][0], candata.dlc);
    }

        else  //aggregate
    {

    // Wait for permission from Canbus_RX task to edit data array
    if (!xSemaphoreTake(can.dataMutex[1], pdMS_TO_TICKS(50)))
    {
      ESP_LOGE(TAG, "CANBUS RX/TX intertask notification timeout")  ;
      return; 
    }

        /*
        Alarms will be reported in the following manner:
        1) all connected controllers must report OK for an OK to be reported to the inverter
        2) If all controllers report Not Supported than that will be reported to the inverter.
        3) Any other combination will report an alarm to the inverter.

        Alarm = 01
        OK = 10
        NSUP = 11

                        Truth table for reporting alarms

  Controller 1:     |  01  |  01  |  10  |  10  |  11  |
                    |______|______|______|______|______|
  Controller 2:     |  10  |  11  |  11  |  10  |  11  |
                    |______|______|______|______|______|
     Report Out:    |  01  |  01  |  01  |  10  |  11  |



        */



        for (int8_t i = 0; i < TWAI_FRAME_MAX_DLC; i++)     // loop through all 8 bytes
        {
            for (int8_t j = 0; j < 8; j += 2)    // loop through each 'crumb'
            {
                //  set the bitmask so we can compare the same crumb across every controller
                bitmask = 3 << j;    // e.g. for j=2 then B0000 1100   

                for (int8_t k = 0; k < MAX_NUM_CONTROLLERS; k++)    // loop through the controllers 
                {
                    if (can.controller_is_online[k])  // only use online controllers
                    {
                    byte =can.data[1][k][i];     //set "byte" equal to the byte found in k controller
                        
                        switch ((byte & bitmask) >> j)  // Check k controller crumb. This will compute to a 0, 1, 2, or 3
                        {
        
                        case B00000001:     // ALARM

                            candata.data[i] &= ~(3 << j);  // zero the crumb
                            candata.data[i] |= (1 << j);   //set the crumb as an ALARM and we can break from this word
                            break;
                        /*
                        case B00000000:     // NSUP
                            
                            if ((candata.data[i] & bitmask) >> j == B00000010)  // if the existing crumb is an OK change it to ALARM since it's a mismatch
                            {
                            candata.data[i] &= ~(3 << j);  // zero the crumb
                            candata.data[i] |= (1 << j);   //set the crumb as an ALARM and we can break from this word
                            break;
                            }

                            candata.data[i] |= (3 << j);   //set the crumb as NSUP and break from this word
                            break;

                        case B00000011:     // NSUP

                            if ((candata.data[i] & bitmask) >> j == B00000010)  // if the existing crumb is an OK change it to ALARM since it's a mismatch
                            {
                            candata.data[i] &= ~(3 << j);  // zero the crumb
                            candata.data[i] |= (1 << j);   //set the crumb as an ALARM and we can break from this word
                            break;
                            }

                            candata.data[i] |= (3 << j);   //set the crumb as NSUP and break from this word
                            break;
                        */
                        case B00000010:     // OK
                            
                            if ((candata.data[i] & bitmask) >> j != B00000001)  // only if the existing crumb is not an Alarm do we mark an OK
                            {
                            candata.data[i] &= ~(3 << j);  // zero the crumb
                            candata.data[i] |= (2 << j);   //set the crumb as OK
                            }
                        }  

                    }
                }

            }
        }   

    }   

        // Return permission to Canbus_RX task 
        xSemaphoreGive(can.dataMutex[1]); 

        send_canbus_message(ptrFrame);
}

// Installed Capacity
void victron_message_379()
{
    CANframe candata(2, 0x379);  // initialize struct with dlc and identifier
    CANframe* ptrFrame = &candata;

    // calculate total Ah based on #of controllers
    uint16_t Online_Ah = 0;

    // Wait for permission from Canbus_RX task to edit data array
    if (!xSemaphoreTake(can.dataMutex[5], pdMS_TO_TICKS(50)))
    {
      ESP_LOGE(TAG, "CANBUS RX/TX intertask notification timeout")  ;
      return; 
    }

    for (int8_t i = 0; i < MAX_NUM_CONTROLLERS; i++)
    {
        if (can.controller_is_online[i] && can.controller_is_integrated[i])  
        {
        Online_Ah = Online_Ah + *(uint16_t*)&can.data[5][i][4];
        }
    }
 
    // Return permission to Canbus_RX task 
    xSemaphoreGive(can.dataMutex[5]); 

    memcpy(&candata.data[0], &Online_Ah, sizeof(uint16_t));

    send_canbus_message(ptrFrame);
}
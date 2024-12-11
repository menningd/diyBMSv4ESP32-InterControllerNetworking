/*

This code formats local controller data into "industry standard" CAN messages for the purpose of reporting to various inverter manufacturers. It also establishes Inter-Controller communication of these messages to be aggregated and reported to an inverter

Note!! Data will be serialized for transmission in little endian order. It will be received by other controllers and stored as such in a array of single byte type. So, those parameters
need cast back to the original data type/size before any aggregation math.
*/


#define USE_ESP_IDF_LOG 1
static constexpr const char *const TAG = "diybms-ControllerCAN";

#include "ControllerCAN.h"

const uint32_t ControllerCAN::id[MAX_CAN_PARAMETERS][MAX_NUM_CONTROLLERS] = {
    /*                                                                                                                                                                                     __INDUSTRY STANDARD ID'S__    */
    /*  0       DVCC*/                    {0x258 /*600*/   ,0x259 /*601*/   ,0x25a /*602*/   ,0x25b /*603*/   ,0x25c /*604*/   ,0x25d /*605*/   ,0x25e /*606*/   ,0x25f /*607*/   },         /*__0x351 (849)__*/
    /*  1       ALARMS*/                  {0x26c /*620*/   ,0x26d /*621*/   ,0x26e /*622*/   ,0x26f /*623*/   ,0x270 /*624*/   ,0x271 /*625*/   ,0x272 /*626*/   ,0x273 /*627*/   },         /*__0x35a (858)__*/
    /*  2       DIYBMS MSGS*/             {0x280 /*640*/   ,0x281 /*641*/   ,0x282 /*642*/   ,0x283 /*643*/   ,0x284 /*644*/   ,0x285 /*645*/   ,0x286 /*646*/   ,0x287 /*647*/   },
    /*  3       #MODULES OK*/             {0x294 /*660*/   ,0x295 /*661*/   ,0x296 /*662*/   ,0x297 /*663*/   ,0x298 /*664*/   ,0x299 /*665*/   ,0x29a /*666*/   ,0x29b /*667*/   },         /*__0X372 (882)__*/
    /*  4       SOC/SOH*/                 {0x3e8 /*1000*/  ,0x3e9 /*1001*/  ,0x3ea /*1002*/  ,0x3eb /*1003*/  ,0x3ec /*1004*/  ,0x3ed /*1005*/  ,0x3ee /*1006*/  ,0x3ef /*1007*/  },         /*__0x355 (853)__*/
    /*  5       CAP & FIRMWARE*/          {0x3fc /*1020*/  ,0x3fd /*1021*/  ,0x3fe /*1022*/  ,0x3ff /*1023*/  ,0x400 /*1004*/  ,0x401 /*1005*/  ,0x402 /*1006*/  ,0x403 /*1007*/  },         /*__0x35f (863)__*/
    /*  6       V-I-T*/                   {0x410 /*1040*/  ,0x411 /*1041*/  ,0x412 /*1042*/  ,0x413 /*1043*/  ,0x414 /*1044*/  ,0x415 /*1045*/  ,0x416 /*1046*/  ,0x417 /*1047*/  },         /*__0x356 (854)__*/
    /*  7       HOSTNAME*/                {0x424 /*1060*/  ,0x425 /*1061*/  ,0x426 /*1062*/  ,0x427 /*1063*/  ,0x428 /*1064*/  ,0x429 /*1065*/  ,0x42a /*1066*/  ,0x42b /*1067*/  },         /*__0x35e (862)__*/
    /*  8       MIN_MAX CELL V_T*/        {0x438 /*1080*/  ,0x439 /*1081*/  ,0x43a /*1082*/  ,0x43b /*1083*/  ,0x43c /*1084*/  ,0x43d /*1085*/  ,0x43e /*1086*/  ,0x43f /*1087*/  },         /*__0x373 (883)__*/
    /*  9       MIN CELL V I.D.*/         {0x44c /*1100*/  ,0x44d /*1101*/  ,0x44e /*1102*/  ,0x44f /*1103*/  ,0x450 /*1104*/  ,0x451 /*1105*/  ,0x452 /*1106*/  ,0x453 /*1107*/  },         /*__0x374 (884)__*/
    /*  10      MAX CELL V I.D.*/         {0x460 /*1120*/  ,0x461 /*1121*/  ,0x462 /*1122*/  ,0x463 /*1123*/  ,0x464 /*1124*/  ,0x465 /*1125*/  ,0x466 /*1126*/  ,0x467 /*1127*/  },         /*__0x375 (885)__*/
    /*  11      MIN CELL T I.D.*/         {0x474 /*1140*/  ,0x475 /*1141*/  ,0x476 /*1142*/  ,0x477 /*1143*/  ,0x478 /*1144*/  ,0x479 /*1145*/  ,0x47a /*1146*/  ,0x47b /*1147*/  },         /*__0x376 (886)__*/
    /*  12      MAX CELL T I.D.*/         {0x488 /*1160*/  ,0x489 /*1161*/  ,0x48a /*1162*/  ,0x48b /*1163*/  ,0x48c /*1164*/  ,0x48d /*1165*/  ,0x48e /*1166*/  ,0x48f /*1167*/  }          /*__0x377 (887)__*/

};


//return whether a controller has a valid heartbeat by checking the bitmssgs timestamp array
bool ControllerCAN::controller_heartbeat(uint8_t controllerAddress)
{
 if ((esp_timer_get_time() - DIYBMS_TIMESTAMP[controllerAddress]) < (HEARTBEAT_PERIOD*1000))
  {
    return true;
  }
  return false;
}

// clear values
void ControllerCAN::clearvalues()
{
   online_controller_count = 1; 
   master = 0;
      // Zero Data Array 
    memset(&data, 0, sizeof(data));
    memset(&DIYBMS_TIMESTAMP, 0, sizeof(DIYBMS_TIMESTAMP));
}

// Checks controller network status and returns: 0=OK  1=controller offline  2=controller network configuration error
// Updates online controller count
uint8_t ControllerCAN::controllerNetwork_status()
{
  uint8_t returnvalue = OK;
  uint8_t communicating_controllers = 0;
  uint8_t isolated_controllers = 0;
  uint8_t addressbitmask = 0;
  uint8_t high_availability = mysettings.highAvailable;
  uint8_t networked_controllers = 0;

  for (uint8_t i = 0; i < MAX_NUM_CONTROLLERS; i++)
  {
      if (controller_heartbeat(i)) // only poll online controllers 
        {communicating_controllers++;

        // count how many have Isolated themselves (DIYBMS_MSGS frame, byte 2)
        if (data[2][i][2] == 1)
        {
          isolated_controllers++;
        }

        // checksum for controller address overlap
        addressbitmask &= data[2][i][0]; // this should should never be elevated above 0 or there is an overlap
        
        // check that local high_availability setting matches the network
        /*
        if (mysettings.highAvailable != data[2][i][3])
        {
        ESP_LOGE(TAG, "Local 'High Availability' settings do not match with networked ControllerID %d",i);
        returnvalue = 2;
        } 
        // check that # networked controllers setting matches the network
        if (mysettings.controllerNet != data[2][i][4])
        {
        ESP_LOGE(TAG, "Local '# of Networked Controllers' settings do not match with networked ControllerID %d",i);
        returnvalue = 2; 
        }
        */ 
      }

  }

  if (addressbitmask != 0)
  {
    ESP_LOGE(TAG, "Controller address conflict");
    returnvalue = 2;
  }



   // checksum for connected controllers 
  if (communicating_controllers > mysettings.controllerNet)
  {
      ESP_LOGE(TAG, "Controller network count discrepancy. #Online Controllers=%d/%d (more than expected total).",online_controller_count,mysettings.controllerNet);
      returnvalue = 2; 
      online_controller_count = communicating_controllers;
  }
  else if (communicating_controllers < mysettings.controllerNet && returnvalue !=2)  // controller offline (but don't overwrite a higher level alert)
  {
      returnvalue = 1; 
      online_controller_count = communicating_controllers - isolated_controllers; 
      ESP_LOGE(TAG, "A controller is offline. #Online Controllers=%d/%d",online_controller_count,mysettings.controllerNet);
  }
  else  //normal operation
  {
      online_controller_count = communicating_controllers - isolated_controllers;
  }

  return returnvalue;

 }

void ControllerCAN::who_is_master()  // Decide which controller is master ( == the lowest integer-addressed controller number with a valid heartbeat)
{   

      for (uint8_t i = 0; i < MAX_NUM_CONTROLLERS; i++)
      {
        if (controller_heartbeat(i) == true)
        {
            master=i;
            
            ESP_LOGI(TAG, "Current master controller ID: %d", master);
            break; 
        }
      }
}

void ControllerCAN::SetBankAndModuleText(char *buffer, uint8_t cellid)       //function used for cell i.d.'s
{

  uint8_t bank = cellid / mysettings.totalNumberOfSeriesModules;
  uint8_t module = cellid - (bank * mysettings.totalNumberOfSeriesModules);

  // Clear all 8 bytes
  memset(buffer, 0, 8);

    if (mysettings.controllerNet != 1) 
    {
      snprintf(buffer, 8, "%db%dm%d", mysettings.controllerID, bank, module);      
    }                                                                   
    else
      snprintf(buffer, 8, "b%d m%d", bank, module);

}

// These reconnect functions check if Reconnection is permitted after a pack has been isolated
bool SOC_Permit_Reconnect()
{
  uint16_t local_SOC = *(uint16_t*)&data[4][mysettings.controllerID][0];

  if (abs(Aggregate_SOC - local_SOC) < Rules::hysteresisvalue[Rule::ReconnectSOC])
  {
    return true;
  }
  return false;
}
bool Voltage_Permit_Reconnect()
{
  int16_t local_Voltage = *(int16_t*)&data[6][mysettings.controllerID][0];

  if (abs(Aggregate_Voltage - local_Voltage) < Rules::hysteresisvalue[Rule::ReconnectVoltage])
  {
    return true;
  }
  return false;
}







////////////////////////////////////////////
// Controller-to-Controller CAN Messages////
////////////////////////////////////////////
void ControllerCAN::c2c_DVCC()    //DVCC settings
{
    CANframe candata;
    memset(&candata.data, 0, sizeof(candata.data));

    // CVL - 0.1V scale
    uint16_t chargevoltagelimit;
    // CCL - 0.1A scale
    int16_t maxchargecurrent;
    // DCL - 0.1A scale
    int16_t maxdischargecurrent;
    // Not currently used by Victron
    // 0.1V scale
    uint16_t dischargevoltage = mysettings.dischargevolt;


    // THESE DEFAULTS (do nothing) are dependant on the particular inverter 
      uint16_t default_charge_voltage;         
      int16_t default_charge_current_limit;   
      int16_t default_discharge_current_limit; 

      if (mysettings.protocol == ProtocolEmulation::CANBUS_VICTRON)
      {
        // Don't use zero for voltage - this indicates to Victron an over voltage situation, and Victron gear attempts to dump
        // the whole battery contents!  (feedback from end users)
        default_charge_voltage = rules.lowestBankVoltage / 100;   
        default_charge_current_limit = 0;    
        default_discharge_current_limit = 0; 
      }
      else if ((mysettings.protocol == ProtocolEmulation::CANBUS_PYLONTECH) && (mysettings.canbusinverter == CanBusInverter::INVERTER_DEYE))
      {
        // FOR DEYE INVERTERS APPLY DIFFERENT LOGIC TO PREVENT "W31" ERRORS
        // ISSUE #216
        default_charge_voltage = rules.lowestBankVoltage / 100;
        default_charge_current_limit = 0;
        default_discharge_current_limit = 0;
      }
      else if ((mysettings.protocol == ProtocolEmulation::CANBUS_PYLONTECH) && (mysettings.canbusinverter == CanBusInverter::INVERTER_GENERIC))
      { // If we pass ZERO's to SOFAR inverter it appears to ignore them
        // so send 0.1V and 0.1Amps instead to indicate "stop"
        default_charge_voltage = 1;         // 0.1V
        default_charge_current_limit = 1;    // 0.1A
        default_discharge_current_limit = 1; // 0.1A
      }


  // Charge settings...
  if (rules.IsChargeAllowed(&mysettings))
  {
    if (rules.numberOfBalancingModules > 0 && mysettings.stopchargebalance == true)
    {
      // Balancing, stop charge
    chargevoltagelimit = default_charge_voltage;
    maxchargecurrent = default_charge_current_limit;
    }
    else
    {
      // Default - normal behaviour
      chargevoltagelimit = rules.DynamicChargeVoltage();
      maxchargecurrent = rules.DynamicChargeCurrent();
    }
  }
  else
  {
    chargevoltagelimit = default_charge_voltage;
    maxchargecurrent = default_charge_current_limit;
  }

  // Discharge settings....
  if (rules.IsDischargeAllowed(&mysettings))
  {
    maxdischargecurrent = mysettings.dischargecurrent;
  }
  else
  {
    maxdischargecurrent = default_discharge_current_limit;
  }
     
      candata.dlc = 8; 
      memcpy(&candata.data[0],&chargevoltagelimit,sizeof(chargevoltagelimit));        // this 'word' will be serialized in little endian order
      memcpy(&candata.data[2],&maxchargecurrent,sizeof(maxchargecurrent));            // this 'word' will be serialized in little endian order
      memcpy(&candata.data[4],&maxdischargecurrent,sizeof(maxdischargecurrent));     // this 'word' will be serialized in little endian order
      memcpy(&candata.data[6],&dischargevoltage,sizeof(dischargevoltage));           // this 'word' will be serialized in little endian order
      candata.identifier = id[0][mysettings.controllerID];                            

    
      memcpy(&data[0][mysettings.controllerID][0], &candata.data, candata.dlc);       //copy calculated values to array


    if (mysettings.controllerNet != 1)
    {
        // send to tx routine , block 50ms 
        if (xQueueSendToBack(CANtx_q_handle, &candata, pdMS_TO_TICKS(50)) != pdPASS)
        {
            ESP_LOGE(TAG, "CAN tx Q Full");
        }
    }

}

// diyBMS will use Victron CAN alarm structure to report alarms. These will need reorganized durring aggregation for other inverters  as necessary
void ControllerCAN::c2c_ALARMS()      //Inverter Alarms 
{

  CANframe candata;
  memset(&candata.data, 0, sizeof(candata.data));

  const uint8_t BIT01_ALARM = B00000001;
  const uint8_t BIT23_ALARM = B00000100;
  const uint8_t BIT45_ALARM = B00010000;
  const uint8_t BIT67_ALARM = B01000000;

  const uint8_t BIT01_OK = B00000010;
  const uint8_t BIT23_OK = B00001000;
  const uint8_t BIT45_OK = B00100000;
  const uint8_t BIT67_OK = B10000000;

  //const uint8_t BIT01_NOTSUP = B00000011;
  //const uint8_t BIT23_NOTSUP = B00001100;
  //const uint8_t BIT45_NOTSUP = B00110000;
  //const uint8_t BIT67_NOTSUP = B11000000;

  if (_controller_state == ControllerState::Running)
  {
    // BYTE 0
    //(bit 0+1) General alarm (not implemented)
    //(bit 2+3) Battery high voltage alarm
    candata.data[0] |= ((rules.ruleOutcome(Rule::BankOverVoltage) | rules.ruleOutcome(Rule::CurrentMonitorOverVoltage)) ? BIT23_ALARM : BIT23_OK);
    
    //(bit 4+5) Battery low voltage alarm
    candata.data[0] |= ((rules.ruleOutcome(Rule::BankUnderVoltage) | rules.ruleOutcome(Rule::CurrentMonitorUnderVoltage)) ? BIT45_ALARM : BIT45_OK);

    //(bit 6+7) Battery high temperature alarm
    if (rules.moduleHasExternalTempSensor)
    {
      candata.data[0] |= (rules.ruleOutcome(Rule::ModuleOverTemperatureExternal) ? BIT67_ALARM : BIT67_OK);
    }

    // BYTE 1
    // 1 (bit 0+1) Battery low temperature alarm
    if (rules.moduleHasExternalTempSensor)
    {
      candata.data[1] |= (rules.ruleOutcome(Rule::ModuleUnderTemperatureExternal) ? BIT01_ALARM : BIT01_OK);
    }
    // 1 (bit 2+3) Battery high temperature charge alarm
    // byte1 |= BIT23_NOTSUP;
    // 1 (bit 4+5) Battery low temperature charge alarm
    // byte1 |= BIT45_NOTSUP;
    // 1 (bit 6+7) Battery high current alarm
    // byte1 |= BIT67_NOTSUP;
  }

  // 2 (bit 0+1) Battery high charge current alarm
  // byte2 |= BIT01_NOTSUP;
  // 2 (bit 2+3) Contactor Alarm (not implemented)
  // byte2 |= BIT23_NOTSUP;
  // 2 (bit 4+5) Short circuit Alarm (not implemented)
  // byte2 |= BIT45_NOTSUP;

  // ESP_LOGI(TAG, "Rule BMSError=%u, EmergencyStop=%u", rules.ruleOutcome[Rule::BMSError], rules.ruleOutcome[Rule::EmergencyStop]);

  // 2 (bit 6+7) BMS internal alarm
  candata.data[2] |= ((rules.ruleOutcome(Rule::BMSError) | rules.ruleOutcome(Rule::EmergencyStop)) ? BIT67_ALARM : BIT67_OK);
  // 3 (bit 0+1) Cell imbalance alarm
  // byte3 |= BIT01_NOTSUP;
  // 3 (bit 2+3) Reserved
  // 3 (bit 4+5) Reserved
  // 3 (bit 6+7) Reserved
  //  memset(&q_message[4], byte3, 1);

  // 4 (bit 0+1) General warning (not implemented)
  // byte4 |= BIT01_NOTSUP;
  // 4 (bit 2+3) Battery low voltage warning
    // dischargevolt=490, lowestbankvoltage=48992 (scale down 100)
    if (rules.lowestBankVoltage / 100 < mysettings.dischargevolt)
    {
      candata.data[4] |= BIT23_ALARM;
    }
  // 4 (bit 4+5) Battery high voltage warning
      if (rules.highestBankVoltage / 100 > mysettings.chargevolt)
    {
      candata.data[4] |= BIT45_ALARM;
    }

  // 4 (bit 6+7) Battery high temperature warning
    if (rules.moduleHasExternalTempSensor && rules.highestExternalTemp > mysettings.chargetemphigh)
    {
      candata.data[4] |= BIT67_ALARM;
    }
  // memset(&q_message[5], byte4, 1);

  // 5 (bit 0+1) Battery low temperature warning
    if (rules.moduleHasExternalTempSensor && rules.lowestExternalTemp < mysettings.chargetemplow)
    {
      candata.data[5] |= BIT01_ALARM;
    }
  // 5 (bit 2+3) Battery high temperature charge warning
  // byte5 |= BIT23_NOTSUP;
  // 5 (bit 4+5) Battery low temperature charge warning
  // byte5 |= BIT45_NOTSUP;
  // 5 (bit 6+7) Battery high current warning
  // byte5 |= BIT67_NOTSUP;
    //memset(&q_message[6], byte5, 1);

  // 6 (bit 0+1) Battery high charge current warning
  // byte6 |= BIT01_NOTSUP;
  // 6 (bit 2+3) Contactor warning (not implemented)
  // byte6 |= BIT23_NOTSUP;
  // 6 (bit 4+5) Short circuit warning (not implemented)
  // byte6 |= BIT45_NOTSUP;
  // 6 (bit 6+7) BMS internal warning
   candata.data[6] |= ((rules.ruleOutcome(Rule::BMSError) || rules.ruleOutcome(Rule::EmergencyStop)) ? BIT67_ALARM : 0);
   candata.data[6] |= ((_controller_state != ControllerState::Running) ? BIT67_ALARM : 0);


  // ESP_LOGI(TAG, "numberOfBalancingModules=%u", rules.numberOfBalancingModules);

  // 7 (bit 0+1) Cell imbalance warning
  // byte7 |= (rules.numberOfBalancingModules > 0 ? BIT01_ALARM : BIT01_OK);

  // 7 (bit 2+3) System status (online/offline) [1]
  candata.data[7] |= ((_controller_state != ControllerState::Running) ? BIT23_ALARM : BIT23_OK);


    candata.dlc = 8;                                            
    candata.identifier = id[1][mysettings.controllerID];                                    

    memcpy(&data[1][mysettings.controllerID][0],&candata.data,candata.dlc);                        //copy calculated values to array

    if (mysettings.controllerNet != 1)
    {
        // send to tx routine , block 50ms 
        if (xQueueSendToBack(CANtx_q_handle, &candata, pdMS_TO_TICKS(50)) != pdPASS)
        {
            ESP_LOGE(TAG, "CAN Q Full");
        }
    }
}

void ControllerCAN::c2c_DIYBMS_MSGS()      // diyBMS messaging/alarms
{
  CANframe candata;
  memset(&candata.data, 0, sizeof(candata.data));


// byte 0 - Broadcast the Controller ID
// 
// Bit position determines which controller number is set within configuration
  //CNTRL0 = B00000001  
  //CNTRL1 = B00000010   
  //CNTRL2 = B00000100
  //CNTRL3 = B00001000
  //CNTRL4 = B00010000
  //CNTRL5 = B00100000
  //CNTRL6 = B01000000
  //CNTRL7 = B10000000    
  candata.data[0] = 0x1 << mysettings.controllerID;   
 
// byte 1 - Is Charge/Discharge Allowed?
  candata.data[1] = 0;
  
    if (rules.IsChargeAllowed(&mysettings))
    {
        candata.data[1] = candata.data[1] | B10000000;
    }

    if (rules.IsDischargeAllowed(&mysettings))
    {
        candata.data[1] = candata.data[1] | B01000000;
    }

// byte 2 - Is controller Isolated?
  candata.data[2] = ControllerIsolated;
// byte 3 - HighAvailability setting
  candata.data[3] = mysettings.highAvailable;
// byte 4 - # Networked Controllers
  candata.data[4] = mysettings.controllerNet;
// byte 5 - reserved for future use 
// byte 6 - reserved for future use
// byte 7 - reserved for future use 

  candata.dlc = 8;    
  candata.identifier = id[2][mysettings.controllerID];

  memcpy(&data[2][mysettings.controllerID][0],&candata.data,candata.dlc);     

        if (mysettings.controllerNet != 1)
        {
       // This is a high priority so should we send it to front of tx queue? 
        if (xQueueSendToFront(CANtx_q_handle, &candata, pdMS_TO_TICKS(250)) != pdPASS)
        {
            ESP_LOGE(TAG, "CAN tx Q Full");
        }
        }
}

void ControllerCAN::c2c_MODULES()       // # of modules O.K. 
{
  CANframe candata;
  memset(&candata.data, 0, sizeof(candata.data));
  
    uint16_t numberofmodulesok = TotalNumberOfCells() - rules.invalidModuleCount;
    // uint16_t numberofmodulesblockingcharge = 0;
    // uint16_t numberofmodulesblockingdischarge = 0;
    uint16_t numberofmodulesoffline = rules.invalidModuleCount;


    candata.dlc = 8; 
    memcpy(&candata.data[0],&numberofmodulesok,sizeof(numberofmodulesok));
    memcpy(&candata.data[6],&numberofmodulesoffline,sizeof(numberofmodulesoffline));         
    candata.identifier = id[3][mysettings.controllerID];                                   

    memcpy(&data[3][mysettings.controllerID][0],&candata.data,candata.dlc);                        //copy calculated values to array
 

    if (mysettings.controllerNet != 1)
    {
        // send to tx routine , block 50ms 
        if (xQueueSendToBack(CANtx_q_handle, &candata, pdMS_TO_TICKS(50)) != pdPASS)
        {
            ESP_LOGE(TAG, "CAN Q Full");
        }
    }
}

void ControllerCAN::c2c_SOC()      // SOC
{

    CANframe candata;
    memset(candata.data, 0, sizeof(candata.data));

    uint16_t stateofchargevalue;
    uint16_t stateofhealthvalue;
    // uint16_t highresolutionsoc;


  if (_controller_state == ControllerState::Running && mysettings.currentMonitoringEnabled && currentMonitor.validReadings && (mysettings.currentMonitoringDevice == CurrentMonitorDevice::DIYBMS_CURRENT_MON_MODBUS || mysettings.currentMonitoringDevice == CurrentMonitorDevice::DIYBMS_CURRENT_MON_INTERNAL))
  {

    stateofchargevalue = rules.StateOfChargeWithRulesApplied(&mysettings, currentMonitor.stateofcharge);
    stateofhealthvalue = (uint16_t)(trunc(mysettings.soh_percent));
   
    candata.dlc = 4;                                                    
    memcpy(&candata.data[0],&stateofchargevalue,sizeof(stateofchargevalue));  
    memcpy(&candata.data[2],&stateofchargevalue,sizeof(stateofhealthvalue));          
    candata.identifier = id[4][mysettings.controllerID];                                 

    memcpy(&data[4][mysettings.controllerID][0],&candata.data[0],candata.dlc);                   //copy calculated values to array

    if (mysettings.controllerNet != 1)
    {

        // We must do some aggregation here even if the controller is Isolated so that we can establish when a reconnection is permitted
        //SOC (weighted average based on nominal Ah of each controller)
        uint16_t Total_Ah = 0;
        uint32_t Total_Weighted_Ah = 0;
        uint16_t Weighted_SOC = 0;

        for (int8_t i = 0; i < MAX_NUM_CONTROLLERS; i++)
        {
            if (controller_heartbeat(i) && (data[2][i][2] == 1)) // only use values from online controllers that have not Isolated themselves
            {
            Total_Ah = Total_Ah + *(uint16_t*)&data[5][i][4];
            Total_Weighted_Ah = Total_Weighted_Ah + (*(uint16_t*)&data[4][i][0]) * (*(uint16_t*)&data[5][i][4]);  //SOC x Online capacity
            }
        }
        if (Total_Ah)  //avoid divide by zero (we won't have useable values during CAN initialization)
        {          
            // store this value for use in rules
            AggregateSOC = Total_Weighted_Ah / Total_Ah;
        }    

        // send to tx routine , block 50ms       
        //ESP_LOGI(TAG,"SOC sent over ControllerCAN = %d",stateofchargevalue); //for debug purposes only
        if (xQueueSendToBack(CANtx_q_handle, &candata, pdMS_TO_TICKS(50)) != pdPASS)
        {
            ESP_LOGE(TAG, "CAN Q Full");
        }

    }
  }
}

void ControllerCAN::c2c_CAP()   // Online capacity and firmware version
{
    CANframe candata;
    memset(&candata.data, 0, sizeof(candata.data));

    uint16_t BatteryModel;
    uint16_t Firmwareversion;
    uint16_t OnlinecapacityinAh;

  // Not used by Victron
  BatteryModel = 0;

  // Need to swap bytes for this to make sense.
  Firmwareversion = ((uint16_t)COMPILE_WEEK_NUMBER_BYTE << 8) | COMPILE_YEAR_BYTE;

  OnlinecapacityinAh = mysettings.nominalbatcap;


      
    
      candata.dlc=6;                                                          
      memcpy(&candata.data[0],&BatteryModel,sizeof(BatteryModel));                     
      memcpy(&candata.data[2],&Firmwareversion,sizeof(Firmwareversion));                
      memcpy(&candata.data[4],&OnlinecapacityinAh,sizeof(OnlinecapacityinAh));  
      candata.identifier = id[5][mysettings.controllerID];        

    
      memcpy(&data[5][mysettings.controllerID][0],&candata.data[0],candata.dlc);                  //copy local values to array

    if (mysettings.controllerNet != 1)
    {
        // send to tx routine , block 50ms 
        if (xQueueSendToBack(CANtx_q_handle, &candata, pdMS_TO_TICKS(50)) != pdPASS)
        {
            ESP_LOGE(TAG, "CAN Q Full");
        }
    }
}

void ControllerCAN::c2c_VIT()   //Battery voltage, current, and temperature
{
    CANframe candata;
    memset(&candata.data, 0, sizeof(candata.data));


    int16_t voltage; 
    int16_t current;
    int16_t temperature;  

  // Use highest bank voltage calculated by controller and modules
  // Scale 0.01V
   voltage = rules.highestBankVoltage / 10;

  // If current shunt is installed, use the voltage from that as it should be more accurate
  if (mysettings.currentMonitoringEnabled && currentMonitor.validReadings)
  {
   voltage = currentMonitor.modbus.voltage * 100.0;
  }

  
  current = 0;
  // If current shunt is installed, use it
  if (mysettings.currentMonitoringEnabled && currentMonitor.validReadings)
  {
    // Scale 0.1A
  current = currentMonitor.modbus.current * 10;
  }


  // Temperature 0.1C using external temperature sensor
  if (rules.moduleHasExternalTempSensor)
  {
   temperature = (int16_t)rules.highestExternalTemp * (int16_t)10;
  }
  else
  {
    // No external temp sensors
   temperature = 0;
  }


      candata.dlc = 6;
      memcpy(&candata.data[0],&voltage,sizeof(voltage));      // this 'word' will be serialized in little endian order
      memcpy(&candata.data[2],&current,sizeof(current));      // this 'word' will be serialized in little endian order   
      memcpy(&candata.data[4],&temperature,sizeof(temperature));   // this 'word' will be serialized in little endian order
      candata.identifier = id[6][mysettings.controllerID];     

    
      memcpy(&data[6][mysettings.controllerID][0],&candata.data,candata.dlc);       //copy calculated values to local array


     // send to tx routine , block 50ms 
      if (mysettings.controllerNet != 1)
      {

        // We must do some aggregation here even if the controller is Isolated so that we can establish when a reconnection is permitted
        voltage = 0;

        for (int8_t i = 0; i < MAX_NUM_CONTROLLERS; i++)
        {
            if (controller_heartbeat(i) && (data[2][i][2] == 1))  // only use values from online controllers that have not Isolated themselves (DIYBMS_MSGS frame, byte 2)
            {
                voltage = voltage + *(int16_t*)&data[6][i][0];
            }
        }
        AggregateVoltage = voltage / online_controller_count;


          if (xQueueSendToBack(CANtx_q_handle, &candata, pdMS_TO_TICKS(50)) != pdPASS)
        {
            ESP_LOGE(TAG, "CAN Q Full");
        }
      }



}

void ControllerCAN::c2c_HOST()     //unique part of hostname
{
    CANframe candata;
    memset(&candata.data, 0, sizeof(candata.data));


    candata.dlc = 8;                                                
    memcpy(&candata.data[0],&hostname[7],sizeof(candata.data));          
    candata.identifier = id[7][mysettings.controllerID];           
  
    memcpy(&data[7][mysettings.controllerID][0],&candata.data,candata.dlc);       //copy calculated values to array

    if (mysettings.controllerNet != 1)
    {
        // send to tx routine , block 50ms 
        if (xQueueSendToBack(CANtx_q_handle, &candata, pdMS_TO_TICKS(50)) != pdPASS)
        {
            ESP_LOGE(TAG, "CAN Q Full");
        }
    }
}

void ControllerCAN::c2c_MINMAX_CELL_V_T()    // Min/Max Cell V & T
{
    CANframe candata;
    memset(&candata.data, 0, sizeof(candata.data));

    uint16_t mincellvoltage;
    uint16_t maxcellvoltage;
    uint16_t lowestcelltemperature;
    uint16_t highestcelltemperature;
 
      lowestcelltemperature = 273 + rules.lowestExternalTemp;
      highestcelltemperature = 273 + rules.highestExternalTemp;
      maxcellvoltage = rules.highestCellVoltage;
      mincellvoltage = rules.lowestCellVoltage;


      candata.dlc = 8;                                                 
      memcpy(&candata.data[0],&mincellvoltage,sizeof(mincellvoltage));               
      memcpy(&candata.data[2],&maxcellvoltage,sizeof(maxcellvoltage));           
      memcpy(&candata.data[4],&lowestcelltemperature,sizeof(lowestcelltemperature));  
      memcpy(&candata.data[6],&highestcelltemperature,sizeof(highestcelltemperature)); 
      candata.identifier = id[8][mysettings.controllerID];                                     
    
      memcpy(&data[8][mysettings.controllerID][0],&candata.data,candata.dlc);       //copy calculated values to array


    if (mysettings.controllerNet != 1)
    {
        // send to tx routine , block 50ms 
        if (xQueueSendToBack(CANtx_q_handle, &candata, pdMS_TO_TICKS(50)) != pdPASS)
        {
            ESP_LOGE(TAG, "CAN Q Full");
        }
    }

}

void ControllerCAN::c2c_CELL_IDS()   // Min/Max Cell V & T addresses
{
    CANframe candata;
    candata.dlc = 8;
    
    char text[8];

    // Min. cell voltage id string [1]
  if (rules.address_LowestCellVoltage < maximum_controller_cell_modules)
  {
    memset(&candata.data, 0, sizeof(candata.data));
    SetBankAndModuleText(text, rules.address_LowestCellVoltage);

    memcpy(&candata.data[0],&text,sizeof(text));   
    candata.identifier = id[9][mysettings.controllerID]; 

    memcpy(&data[9][mysettings.controllerID][0],&candata.data,candata.dlc);       //copy calculated values to array


     // send to tx routine , block 50ms 
    if (mysettings.controllerNet != 1 && (xQueueSendToBack(CANtx_q_handle, &candata, pdMS_TO_TICKS(50)) != pdPASS))
    {
        ESP_LOGE(TAG, "CAN Q Full");
    }
  }
    // Max. cell voltage id string [1]
  if (rules.address_HighestCellVoltage < maximum_controller_cell_modules)
  {
    memset(&candata.data, 0, sizeof(candata.data));
    SetBankAndModuleText(text, rules.address_HighestCellVoltage);

    memcpy(&candata.data[0],&text,sizeof(text));   
    candata.identifier = id[10][mysettings.controllerID]; 

    memcpy(&data[10][mysettings.controllerID][0],&candata.data,candata.dlc);       //copy calculated values to array

     // send to tx routine , block 50ms 
    if (mysettings.controllerNet != 1 && (xQueueSendToBack(CANtx_q_handle, &candata, pdMS_TO_TICKS(50)) != pdPASS))
    {
        ESP_LOGE(TAG, "CAN Q Full");
    }
  }
    // Min. cell temp id string [1]
  if (rules.address_lowestExternalTemp < maximum_controller_cell_modules)
  {
    memset(&candata.data, 0, sizeof(candata.data));
    SetBankAndModuleText(text, rules.address_lowestExternalTemp);

    memcpy(&candata.data[0],&text,sizeof(text));   
    candata.identifier = id[11][mysettings.controllerID]; 

    memcpy(&data[11][mysettings.controllerID][0],&candata.data,candata.dlc);       //copy calculated values to array

     // send to tx routine , block 50ms 
    if (mysettings.controllerNet != 1 && (xQueueSendToBack(CANtx_q_handle, &candata, pdMS_TO_TICKS(50)) != pdPASS))
    {
        ESP_LOGE(TAG, "CAN Q Full");
    }
  }
  // Max. cell temp id string [1]
  if (rules.address_highestExternalTemp < maximum_controller_cell_modules)
  {
    memset(&candata.data, 0, sizeof(candata.data));
    SetBankAndModuleText(text, rules.address_highestExternalTemp);

    memcpy(&candata.data[0],&text,sizeof(text));   
    candata.identifier = id[12][mysettings.controllerID]; 

    memcpy(&data[12][mysettings.controllerID][0],&candata.data,candata.dlc);       //copy calculated values to array

     // send to tx routine , block 50ms 
    if (mysettings.controllerNet != 1 && (xQueueSendToBack(CANtx_q_handle, &candata, pdMS_TO_TICKS(50)) != pdPASS))
    {
        ESP_LOGE(TAG, "CAN Q Full");
    }
  }
}










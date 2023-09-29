/*
Copyright 2022 D. Kloser
  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2 of the License.
  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
  You should have received a copy of the GNU General Public License along with this program. If not, see https://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
*/
/*       Weather Exchange ESP32
//       1) This is v1.0.5913 on 2022/11/09
//       2) This is the PCB 1.1 and 1.2 Weather Station with Firmware version 1.0.5x, confirmed working with the ESP32 (LOLIN32 or Dev Board)
//       3) Both "Client" and "Server" are now in one source file and on the same hardware in the case of the ESP32 (normally)
//         Monteino MEGA with "Server 2.3 ESP8266" - settings OLED 1, LORA 1, RF69 1, RTClib 1, RAIN_INT 1, SPEED_INT 1, not tested since 2019/05/05 (probably broken)
//         RocketScream SAMD21 with RF69 to server ESP8266 - "RF69_reliable_datagram_server_data_2.3.ino" (probably broken now)
//         Tested with the SAMD21 and PCB 1.0 (modified for SAMD21 with Adapter board) (probably broken now)
//       4) Macro Sections can be activate and deactivated by changing the below defines to 0 or 1 respectively, but need to be careful as it may break things.
//  SRAM Storage: 1: RAIN counter (4b)
//                2: Sensor data packet x ?
//                3: INA_energy_total mAh
//                4: head, tail is_full/empty boolean pointer of buffer
//                   and some others (see code)
*/
/*   Done: 1) Implemented Client Side LORA: DATA, ACK, ERR, POLL; and PING, PONG, TIME;
           2) Added Encryption (LORA RFM69 only)
           3) Cleaned up code
           4) Using RH_RF69::GFSK_Rb9_6Fd19_2 for better SNR (LORA)
           5) Added Wind Vane (resistor network) and Wind Speed (Interrupt driven)
           6) Collect real data and feed into packet data
           7) Read noise value with rssiRead() for LORA
           8) Calibrate BH1750 lux value with Plastic filter housing
           9) The first RSSI value is zero (no packet was sent yet PING/PONG packet to check for server?).
          10) ESP32 - Working: ADS1115, HDC1080, BMP280|BMP180, DS3231 (RTClib), OLED|SSD1306, ENCODER + Button, 
          11) RTC time update packet (0x07), based on NTP format (only from client to server at the moment - needs some more work)
          12) CRC16 instead of CRC32 (saves 2 bytes of data, while increasing collision potential)
          13) Peripheral detection - else erraneous data received (done for ADC1115, HDC1080, BMP280, BH1750), Requires supporting Server to filter "error" numbers
          14) Last Data Packet "age" timer, displayed on OLED
          15) Client able to use WiFi for direct https/http POST
          16) Included Wifi rssi and include in POST (not yet in LORA packet)
          17) Added uRTClib for DS3232 Alarming
          18) Sleeping after screensaver timeout + interactive timeout "HW_SLEEP 1" option
          19) OLED Display setup results - BMP280 - OK, RTC - ERROR, etc
          20) Safe millis() use for timing with wrap around
          21) AT24Cxx EEPROM and DS3232 SRAM FIFO circular Data records Buffer ( put() - when no server connectivity); Send data in bulk when server connectivity available ( pop() )
          22) Forced reboot after xxx millis direct after http_send() to update NTP and catch any http client problems which were manifesting (outdated now 1.05)
          23) Integrated SI7021 RH% Temperature Chip use: HW_SI7021 == 1
          24) Report Wind Direction only for non-zero wind speeds (should be optional) (reverted back to report zero wind 1.05)
          25) Wind gust reported as max wind measured in periodSample
          26) Critical Data saved to SRAM: rain_counter, ina_energy_total, tail, head buffer_if_full on changes through write_dsram();
          27) Added HW Watchdog capability on pin 13 HW_WATCHDOG option
          28) Removed the RAIN and WIND Interrupt serial print (only useful for debugging) - now a macro option
          29) Added sane value chacking for I2C sensors (BMP280, BH1080, BH1750) in poll_ routines. Solves the bogous 124.99C problems when hot swapping HDC1080 chips.
          30) Chip detection for BH1080 to avoid duplicate address ping with SI7021 (re-check this really works properly 1.05)
          31) Resolved issue on lost data on rebooting and poor network connectivity due to watchdog reboot loop (delay in booting triggering watchdog reboot event). Watchdog is now properly fed.
          32) There is some heating of the DS3232 chip under the ESP32 processor chip - resolved on PCB 1.2 by moving the chip away.
          33) Improved the speed_int routines for better speed measurements (some calibration tests will be nice / useful).
          34) Fixed an issue with the int_SPEED function whereby wind speed int counts timing includes possible blocking http call and thus lowers the speed_int count bringing down the average wind speed for any reporting period which includes int blocking http system calls.
          35) 1.03 Reviewed Wind Gust Code for Sanity and corrected a timing issue during http_send
          36) 1.04 Added 3 window Median filtering for ADC0-3 values of ADS1115 sensor (works well)
          37) 1.04 Startup Screen Values taken from macros (compile date, version number, hw version, site name, customer name)
          38) 1.04 I2C ping DS_ADDRESS prior to polling to avoid crashing if DS323x is not present.
          39) 1.04 Removed depreciated HW_OLED code, Refactored some debug messages & code
          40) 1.04 Added SW_DEBUG_HEAP option to monitor memory usage (to resolve memory leak)
          41) 1.04 Majour send_http() rewite using WifiClientSecure + HTTPClient; Using http "keep-alive" while cycling main loop between sends; Only attempt to POST data if Wifi is connected (working well now in 1.05)
          42) 1.04 keep track of server_time from post and go_set_ntp on overly different times
          43) 1.04 Reboot if (esp_get_free_heap_size() < 120000) || (esp_get_minimum_free_heap_size() < 100000) || (heap_caps_get_largest_free_block(0) < 100000) to avoid running out of memory
          44) 1.04 Imnplemented a lastPublishMillis millis() timer as a failsafe in case the RTC Interrupt is unavailable for whatever reason: periodPublish + 10% (works well only in 1.05)
          45) 1.04 Call setClock() to synchronize clocks every 12h as a failsafe (only if internet is working and no buffer data is being sent)
          46) 1.04 Reboot System if no successful data connection for 11h (last_packet_millis counter) check_timers()
          47) 1.04 Reboot 10*300s before millis() rollover as a precaution - macro option
          48) 1.04 Remove redundant send_local() function (previously used for debugging)
          49) 1.04 Realtime data only submitted "in realtime" (heap space, etc in assemble_data() )
          50) 1.04 uint16_t reboot_counter & packet_counter in SDRAM (for debugging)
          51) 1.04 lastPeriodPublish timestamp in SDRAM to avoid writing the same packet twice (eg during reboot) with upgrade procedure (needs some more testing)
          52) 1.04 Added OTA firware update - https://iotappstory.com/ (working properly only in 1.0.56)
          53) 1.05 Added macro to Changed RTC alarm to match every minute and not reset the alarm period to reduce chance of missed interrupts when CONF_MODULUS_S 60
          54) 1.05 Added rtc_int_counter to POST data for debugging
          55) 1.05 Fixed Interrupt routines to work with IAS OTA firmware updates
          56) 1.05 Added Library References & Removed private config data from main source file
          57) 1.05 Adjusted ds_temperature/100 in line with the new new uRTCLIB 6.2.7 format of the temp() function
          58) 1.056 Fixed bug with IAS OTA updates caused by improperly handled interrupts during updates
          59) 1.057 Removed redundant DS3231 lib code use uRTClib instead
          60) 1.057 Added SHT35D Temperature & Humidity Sensor Support, use HW_SHT35 == 1 macro to enable
          61) 1.057 Added median filtering to BMP280 sensor for both pressure and temperature
          62) 1.057 Added RS485/Modbus Solar Radiation Sensor TBQ "HW_SOLAR_TBQ == 1" macro to enable
          63) 1.058 Removed adc3 (wind direction) from median filtering as it does not make sense
          64) 1.058 Changed check_publish_data() criteria to avoid overwriting previous data set: (lastPeriodPublishTimestamp + periodPublish/1000) <= timestamp
          65) 1.059 Extensively tested and fixed the fallback timer lastPublishMillis and adjust_ms (now works as expected)
          66) 1.059 Introduced SW_TOGGLE_DS_INT for debugging the DS interrupt fallback timer
          67) 1.0510 Fixed HW_SOLAR bug in init_solar() code
          68) 1.0511 Added CONF_INFLUX_PORT option
          69) 1.0511 Improved OTA update progress output on Console (whole 10% and dots for each %) and OLED (counting %)
          70) 1.0511 Added median filtering to BH1750 (illumination), SI7021 (temperature & humidity), HDC1080 (temperature & humidity), SHT35 (temperature & humidity), INA219 (voltage & current) and DS3232 (temperature), SOLAR|TBQ sensors
          71) 1.0512 Added SW_WIFI_MULTI macro option to use WifiMulti.h for the use of a fallback acces spoint and hopfully better IAS inter operability in the future. (currently you need to disable the Wifi connection in the IAS library)
          72) 1.0512 Added SW_UPDATE_SPIFFS_CERT macro option to update to update IAS CaCert to the latest root certificate on SPIFFS /cert/iasRootCa.cer (as of 21/Nov/2020) - this is only required once and should be disabled for normal use. certificate is updated prior to contacting IAS.
          73) 1.0512 Increased timeouts to HTTP_CONNECT_TIMEOUT 9000 (ms); HTTP_TIMEOUT 8000 (ms); SSL_HANDSHAKE_TIMEOUT 7 (seconds) due to problems with aiven.io host changes (SSL handshake took longer than previously)
          
    Issues resolved:
           1) Resolved in 1.0511 - Use median filter for all sensor values to catch outliers (done for ADS1115, BMP280, BH1750, HDC1080, SHT35, INA219, DS3232)
           2) Resolved in 1.0511 - IAS Message before actually rebooting (had a problem where reboot message appeared, but the actual reboot occured some hours later
           3) Wakeup on RTC or Button (requires hardware rewiring - inverting the RTC interrupt - done in PCB v1.2)
           4) Use non blocking: RS485 / Modbus library, HTTP(s) / POST (on second core or different library)
           5) Plaintext passwords in serial debug text
           6) Board reboots if low heap <120,000b is detected. Memory leak: Unbable to setup TLS/SSL connection with low Free internal heap before TLS 102180 (hanging) vs 257064 (after boot - working) Workaround: Reboot if free heap becomes too low? Sane value seems to be >100K
              https://github.com/mobizt/Firebase-ESP32/issues/57
           7) Resolved in 1.0512 - Connectivity problems with aiven.io after "upgrade" resulting in longer SSL handshakes and therefore timeouts.
          
    To Do: 1) Temporarily save POST data in SRAM and either delete after successful post or write to EEPROM on failure (in case of shudown during post).
           2) Clean up macros and test if working for different settings & functionality > 1.05
           3) 
           4) Option to Power off solar sensor "at night" (= during night hours, when "online" and time is within say 60s of server time). Check for "darkness" and start "off" timer to switch back on (based on élapsed tim and time of day). This saves about 10mA.
           5) Check: "EEPROM Init...SRAM Init...OK Len: 0 Rain: 29 Energy: 0 Head: 376 Tail: 376 Records: 0 Is Full: 0 Is Empty: 1 CRC: 62076 OK!" "Is Empty:" when buffer is NOT EMPTY.
           6) Integrate & test LORA receiving code from remote client
           7) Implement tiny SRAM FIFO to save EEPROM write cycles
           8) Boot Screen showing EEPROM info
           9) Bootup hardware test mode with OLED and Serial information
           10) Wind direction case select structure
           11) SD Card logging functionality
           12) Alarm Flags 8 bit for: Sensors, Time Sync, System Voltage, Connectivity, General
           13) local AP WiFi data download to laptop or phone via local webUI
           14) Disable Wifi unless sending, disable one core, wakeup for 1) timer, 2) External interrupts - RTC alarm, rotary encoder interrupt, rain, wind interrupt
           15) Use HDC1080 heater when RH > 95% - needs some further research
           16) Check for data in buffer on reboot, to avoid duplicating the last data set. Compare timestamps. 
           17) Differentiate between HDC1080 and SI7021 modules to avoid bogous readings since they are both on the same 0x40 address
           18) Save site data on EEPROM instead of hardcoding allowing EEPROM swap
           19) Before posting data check that timestamp is different to the last one
           20) *** Lost packet on some reboots ***
           21) *** Double packets in 5min period - due to ESP and RTC time difference of 1s ***
           22) Option to select wind beahviour always count (includes zero int periods) or only count when int available.
           23) Look at dates fo certifcates and their expiry dates, think of a workaround
           24) Split Source file into multiple files?
           25) *** Workaround for multiple RTC interrpuots (300 in 5min), caused by some unknown I2C interference. Reprogram RTC alarm should fix it ***
           26) Better handle the POST results codes instead of just 0 and !=0.
           27) Think about possible rate limit for rain interrupt to catch interference (had a situation of 70 interrupts in a 5min period at Kwacibi 16/02/2020 16:00-16:05)
           28) Delay timer function mechanism to delay processes for startup - a non-blocking delay().
           29) Check I2C sensors failure / functionality. Reboot (or cycle sensor) if failure for more than some period of time 1h? Set Alarm flag.
           30) Test TLS/SSL memory limits to lower reboot thresholds
           31) 
           32) 
           
// Issues: 1) Double Interrupts for RAIN with ESP32. There is a problem with ESP32 interrupt edge detection, which wrongly detected rising AND falling edge - need to count two for one as a workaround. 
           3) Wifi Noise level unavailable?
           4) packet counter timer after sleep is reset to zero due to millis reset (use RTC epoch)
           5) mAh calculation not working beyond sleep due to millis reset (use RTC epoch)
           6) time warp: RTC warps by 5h into the future for some unknown reason (may be addressed with the lastPeriodPublish failsafe check)
           7) EEPROM status is_full is_empty on reboot when data available; Data count incorrect.
           8) 
           9) 
           10) ADC1115 measurements include occasional outliers - filter measurements (probably median filter with 3 windows) https://github.com/daPhoosa/MedianFilter https://github.com/flrs/HampelFilter, https://github.com/denyssene/SimpleKalmanFilter
           11) See 41 & 44 above: NTP Synchonization Conditions: Time Jumps between RTC, ESP32, Millis Clock; Cyclic timer 12 or 24h (if internet connectivity is OK, no locally buffered packets available)
           12) It seems that RTC interrupts can be lost in certain situations. Use matching every minute moulus=60 for multiple interrupts during reporting period
           13) PCB 1.2 When REMOTE_I2C (pin 13) is LOW it does not fully switch off the remote I2C board and the P82B175 chip (there remains a 1.8V between EN and GND, when there should be 3.3V, causing i2c bus disruption). Workaround: ensure the PIN 13 os ALWAYS HIGH, or solder EN to GND (to disable the feature) or a pullup to VCC
           14) Unable to hot plug SSD1306 display (yet)
           15) On wifi failure (any send failure) the fallback timer becomes active sending a duplicate packet as is already in the EEPROM (which is later sent and overwrites the previous packet)
           16) On post failure (but internet connectivity is available) IAS is not called (due to wrongly setting the no connectivity flag). Need different failure modes.

// Notes:

Suspected I2C Interference is messing with the RTC programming (time, alarms). The backup timer has fixed this, but introduced a nrew problem with missing packjets on reboot and double packets when ESP and RTC time differes by 1s
Available heap has caused issues in being able to send SSL data

Roadmap to 1.0.6 - stable release
  DONE 1.0511 Median filter of the solar radiation & solar illumination & INA219 current
  Missing packets on some reboots with 5min reporting intervals
  Double packets when RTC / ESP times differ
  Wrongly trigger "PUBLISH First" on reboot with EEPROM packets data stored
  Some limited time delay if possible on "PUBLISH First" to get a minimal wind measurement - check https://avdweb.nl/arduino/timing/virtualdelay
    https://www.instructables.com/id/Coding-Timers-and-Delays-in-Arduino/
    https://www.element14.com/community/community/arduino/arduino-tutorials/blog/2014/06/05/a-non-blocking-delay
    https://github.com/contrem/arduino-timer
    https://github.com/avandalen/VirtualDelay


*/ 
/* REQUIREMENTS */

/* ESP32 Board
   Arduino IDE 1.8.12         https://www.arduino.cc/en/Main/Software
   Arduino ESP32 Addon Boards https://dl.espressif.com/dl/package_esp32_index.json
   General Libraries:
     Adafruit Unified Sensor Library https://github.com/adafruit/Adafruit_Sensor
     CRC16 Library https://github.com/hideakitai/CRCx
   Hardware Specific Libraries - See below comments  
*/

/* HARDWARE CONFIGURATION OPTIONS */
#define HW_ADS1115           1    // HW ADS1115 4 x 16bit ADC                  I2C @ 0x48   (for Supply Voltage, Wind Speed, Wind Direction and 1 Spare Channel)
                                  //   Requires: Adafruit ADS1X15 1.1.0 https://github.com/adafruit/Adafruit_ADS1X15
  #define HW_WIND_DIR        1    // HW WH1080 WIND VANE based on resistor network & reed switches (depends on ADC1115) 
#define HW_ENCODER           1    // HW ROTARY ENCODER with Push Button
                                  //   Requires: Encoder Library https://github.com/igorantolic/ai-esp32-rotary-encoder
#define HW_INA219            1    // HW INA219 Voltage & Current Sensor       I2C @ 0x41
                                  // Requires: ArduinoINA219 Library https://github.com/flav1972/ArduinoINA219
#define HW_HDC1080           0    // HW HDC1080 Temperature & Humidity Sensor  I2C @ 0x40 (only one device per address)
                                  //   Requires: ClosedCube HDC1080 1.3.2 https://github.com/closedcube/ClosedCube_HDC1080_Arduino
#define HW_SI7021            1    // HW SI7021 Temperature & Humidity Sensor  I2C @ 0x40 (only one device per address)
                                  //   Requires: Adafruit SI7021 1.2.5 https://github.com/adafruit/Adafruit_Si7021
#define HW_SHT35             0    // HW SHT35 Temperature & Humidity Sensors  I2C @ 0x44
                                  //   Requires: ClosedCube SHT31D 1.5.1 https://github.com/closedcube/ClosedCube_SHT31D_Arduino
#define HW_BH1750            1    // HW BH1750 Illumination Sensor            I2C @ 0x
                                  //   Requires: BH1750 Library https://github.com/claws/BH1750
#define HW_BMP280            1    // HW BMP280 Barometer & Thermometer Sensor I2C @
                                  //   Requires: Adafruit BMP280 2.0.1 https://github.com/adafruit/Adafruit_BMP280_Library
#define HW_BMP180            0    // HW BMP180 Barometer & Thermometer Sensor I2C @  
#define HW_DS3231            0    // HW DS3231 Real Time Clock DS3231.h       I2C @ 0x68 (only one device per address) (not for SAMD21)
#define HW_RTClib            0    // HW DS3231 Real Time Clock using RTClib   I2C @ 0x68 (only one device per address)
#define HW_uRTClib           1    // HW DS3232 Real Time Clock using uRTClib  I2C @ 0x68 (only one device per address) (including Alarms & DSRAM -> this is the preferred option for the DS3232)
                                  //   Requires: uRTCLIB 6.2.7 https://github.com/Naguissa/uRTCLib
  #define SW_DS_INT_ENABLE   1    //   Optionally ignore DS323X generated interrupts in software (to testing fault conditions)
#define HW_SLEEP             0    // CONFIG ESP32 Based Sleeping (probably needs some cleaning up)
#define HW_SSD1306           1    // HW SSD1306 OLED Display                   I2C @
                                  //   Requires: Adafruit SSD1306 2.2.1 https://github.com/adafruit/Adafruit_SSD1306
#define HW_RAIN_INT          1    // HW WH1080 Rain tipping Bucket Interrupt counter
#define HW_SPEED_INT         1    // HW WH1080 Wind Anemometer Interrupt counter  
#define HW_SPEED_ADC         0    // HW Wind Anemometer Voltage based (requires: HW_ADS1115 == 1)
#define HW_LORA              0    // HW LORA based communication (overall framework)
                                  //   Requires: RadioHead Library http://www.airspayce.com/mikem/arduino/RadioHead/
  #define HW_RF96            0    //   HW RFM96 Wireless Communication Chip
  #define HW_RF69            0    //   HW RFM69 Wireless Communication Chip
#define HW_WIFI              1    // HW WIFI based on ESP32 Chip Only
  #define SW_HTTP_REUSE      1    //   Re-use the http(s) communication to the influxdb server
#define HW_SERIAL            1    // HW Serial Console Enables most of the Normal Output
#define HW_SERIAL_COM        0    // HW SERIAL based communication (probably mostly broken now and incomplete) dependant on?
  #define HW_SI4466          0    //   HW SI4466 Wireless Chip (probably mostly broken now and incomplete)
#define HW_SDCARD            0    // HW SDCARD based logging (mostly broken now and incomplete)
#define HW_SOIL              0    // HW SOIL Humidity Sensor wired to RS485 MODBUS communication
                                  //   Requires: ModbusMaster 2.0.1 https://github.com/4-20ma/ModbusMaster
#define HW_SOLAR             0    // HW SOLAR Insolunation Sensor wired to RS485 MODBUS communication
                                  //   Requires: esp32ModbusRTU Library https://github.com/bertmelis/esp32ModbusRTU
#define HW_SOLAR_TBQ         1    // HW_SOLAR_TBQ New Insolunation Sensor wired to RS485 MODBUS communication
                                  // Requires: esp32ModbusRTU Library https://github.com/bertmelis/esp32ModbusRTU
#define HW_PCF8574           1    // HW PCF8574 Chip Port Multiplexer
                                  //   Requires: PCF8574 Library https://github.com/xreef/PCF8574_library
#define HW_PCF8574JM         0    // HW Port Multiplexer (or this, using different library)
#define HW_EEPROM            1    // HW AT24Cxx based EEPROM for storing data sets after failed POST
                                  //   Requires: AT24Cx Library https://github.com/cyberp/AT24Cx
#define HW_WATCHDOG          1    // HW WATCHDOG
  #define HB_PIN            15    //   CONFIG Heartbeat PIN to feed the HW_WATCHDOG
#define HW_REMOTE_I2C        1    // HW REMOTE I2C Sensor Board Power
  #define REMOTE_I2C_PIN    13    //   CONFIG Pin Number to control the Power to the remote Board

/* ENABLE DEBUG INFORMATION */
#define SW_DEBUG_I2C         0    // needs some cleaning up
#define SW_DEBUG_SSD1306     0    // needs some cleaning up
#define SW_DEBUG_BUFFER      0
#define SW_DEBUG_SPEED_INT   0
#define SW_DEBUG_RAIN        0
#define SW_DEBUG_HEAP        1    // Sends heap statistics via WiFi to influxdb via Post to monitor for memory leaks
#define SW_DEBUG_DS          0
#define SW_DEBUG_HTTP        0
#define SW_DEBUG_SRAM        0
#define SW_DEBUG_BMP280      0
#define SW_DEBUG_HW_WATCHDOG 0
#define SW_DEBUG_SI          0
#define SW_DEBUG_PUBLISH     0    // Debug fallback publish timer if RTC is not working as expected

/* SOFTWARE CONFIGURATION OPTIONS*/
#define SW_TEST

/* SITE SPECIFIC CONFIG HERE */
#include <config_sites.h>                          // Optional external sites config file to keep private data

#define SW_VERSION                        "1.05"   // The Software Version Number
#define SW_SERIAL_SPEED                  115200    // Serial Console Speed in bps (8N1 Assumed)
#define SW_MILLIS_ROLLOVER_REBOOT             1    // Reboot before the millis() rollover just to be safe
#define SW_TOGGLE_DS_INT                      1    // Toggle DS interrupts on and off with the pushbutton for testing
#define SW_MEDIUM_FILTER                      1    // Median filtering values in ADS1115; Requires: MedianFilterLib 1.0.0 https://github.com/luisllamasbinaburo/Arduino-MedianFilter
  #define CONF_MEDIAN_WINDOW                  5    // Windows Size of the median filter
#define SW_IAS_OTA                            1    // InternetAppStory OTA Support, requires a good amount of extra ram and expects the ESP32 EEPROM to be correctly initialized by configuration mode (use IAS loader sketch)
                                                   // Requires: IOTAppstory-ESP 2.1.0-RC2 https://github.com/iotappstory/ESP-Library (Make wifi connectivity conditional in begin() in IOTAppstory.cpp)
#define SW_WIFI_MULTI                         1    // Use "WifiMulti" for fallback AP options and possibly better interoperability with IAS (at some point, but currently still interferes), if set to 0 normal Wifi will be used.
#define SW_UPDATE_SPIFFS_CERT                 1    // Routine which checks the "/cert/iasRootCa.cer" file in SPIFFS and updates it to a newer version (this is not normally required)
                                                   
#define CONF_IAS_CALL_HOME_INTERVAL   7200*1000    // CONFIG 2h in ms
#define WIND_FACTOR                         2.4    // CONFIG 2.4km/h per Hz or (2.4 / 3.6) = 0.66667 m/s per Hz
#define CONF_MILLIS_REBOOT_MS        4291967295    // CONFIG This is 10*300s before the rollover
#define CONF_INTERACTIVE_TIMEOUT          10000    // CONFIG in ms how long to stay in interactive mode until collecting multiple samples and then going to sleep (requires HW_SLEEP above))
#define CONF_MULTIPLE_SAMPLES                20    // CONFIG how many samples to collect before going back to sleep or publishing the data collected (only in non-interactive mode)
#define CONF_NTP_SERVER_1         "pool.ntp.org"   // CONFIG the NTP servers to synchronize time with
#define CONF_NTP_SERVER_2        "time.nist.gov"
#ifndef CONF_INFLUX_SERVER
  #define CONF_INFLUX_SERVER   "www.example.com"   // CONFIG the influx host port 8086 assumed and requires server side SSL support
#endif
#ifndef CONF_INFLUX_PORT
  #define CONF_INFLUX_PORT                  8086   // CONFIG the influx host port 8086 assumed and requires server side SSL support
#endif

/* SYSTEM TIMING SAMPLING AND PUBLISHING */
#if defined(SW_TEST)||defined(SW_BLA1)||defined(SW_BLA2)||defined(SW_BLA3)||defined(SW_BLA4)
  #define CONF_PERIOD_SAMPLE_MS            3000    // in ms, every how often a sample is taken from the sensors (should really not be lower than 1000ms better 3000ms)
  #define CONF_PERIOD_PUBLISH_MS         300000    // in ms, every how often data is averaged and sent to the server (should be a multiple of MODULUS_S)
  #define CONF_MODULUS_S                     75    // in s, RTC alarming period (a multiple of these make up PERIOD_PUBLISH_MS
  #undef  SW_TOGGLE_DS_INT
#else
  #define CONF_PERIOD_SAMPLE_MS            3000
  #define CONF_PERIOD_PUBLISH_MS         300000    
  #define CONF_MODULUS_S                     75    
#endif

#define I2C_FREQUENCY                    50000L    // 100kHz works, but slower is better for noise immunity on remote senor board
#define HW_LED_D2                             1    // Green Status LED
#if defined (ARDUINO_ARCH_SAMD)
  #define HW_SAMTEMP                          0
#endif
#if defined(__AVR_ATmega1284P__)                   // Moteino Board
  #define RAIN_INT_PIN                       10
  #define SPEED_INT_PIN                      11
#endif

  #define CLIENT_ADDRESS                      3    // Test client
  #define SERVER_ADDRESS                      1    // New Test server
  #define SD_CS                               4
  #define RFM96_PIN_CS                        4
  #define RFM96_PIN_IRQ                       2
  #define RFM96_PIN_RESET                     8

#if defined(ESP32)
  #include <esp_task_wdt.h>
  #include "esp_attr.h"
  #include "esp_wifi_types.h"
  #define WDT_TIMEOUT_SEC                   140
  #define RAIN_INT_PIN                       26
  #define SPEED_INT_PIN                      25
  #define RTC_INT_PIN                        27
  #define RFM96_PIN_CS                        5
  #define RFM96_PIN_IRQ                      33
  #define RFM96_PIN_RESET                     2
  #define LED_PIN_OK                          2   // The Green LED PIN

  /* FALLBACK VALUES */
  #ifndef WIFI_SSID
    #define WIFI_SSID              "my_wifi_ssid" 
  #endif
  #ifndef WIFI_PWD
    #define WIFI_PWD               "my_wifi_password"
  #endif
  #ifndef INFLUX_HOST
    #define INFLUX_HOST     String("example_site")          //this is the site= for the influxdb
  #endif
  #ifndef INFLUX_DB
    #define INFLUX_DB       String("my_influx_db_name")
  #endif
  #ifndef INFLUX_USER
    #define INFLUX_USER     String("my_influx_user")
  #endif
  #ifndef INFLUX_PASS
    #define INFLUX_PASS     String("my_secret_password")
  #endif
  #ifndef SW_SITE_NAME
    #define SW_SITE_NAME           " -  UNKNOWN"            // EXACTLY 11 CHARACTERS
  #endif
  #ifndef SW_CUSTOMER_NAME
    #define SW_CUSTOMER_NAME       "XXXXXXXXXXXXXXXXXXXXX"  // EXACTLY 21 CHARACTERS
  #endif
  #ifndef HW_VERSION_MAJOR
    #define HW_VERSION_MAJOR             1
  #endif    
  #ifndef HW_VERSION_MINOR
    #define HW_VERSION_MINOR             2
  #endif
  #ifndef SW_EMAIL
    #define SW_EMAIL "Email: me@example.com\n"  // 21 Characters excluding \n for OLED Boot Screen
  #endif
  #ifndef SW_PHONE
    #define SW_PHONE "Phone:+123 XXX YYYYYY\n"  // 21 Characters excluding \n for OLED Boot Screen
  #endif

  #if (HW_VERSION_MAJOR == 1) && (HW_VERSION_MINOR == 2) // has a hardware potentiometer to adjust VSYS ADC ratio
    #define ADC2_RATIO (( 0.0011666666667 )) //multiplier is: 0.0001875 / (10k / (47k + 9k)) = 0.001068750000   
  #endif
  #ifndef ADC2_RATIO 
    #define ADC2_RATIO (( 0.0011666666667 ))
  #endif
    
  #define HTTP_CONNECT_TIMEOUT   9000   // Time to establish communication with the server (not the SSL connection)
  #define HTTP_TIMEOUT           8000   // Time to get a response from the server to a http request
  #define SSL_HANDSHAKE_TIMEOUT     7   // Time to setup the TLS/SSL connection overhead in seconds
  #define NTP_TIMEOUT            4000   // Time to wait for NTP data in ms
  #define WIFI_TIMEOUT             30   // Time to wait for WIFI connection: 30 x 500ms = 15s
  #define USE_SERIAL           Serial   // The Name of the Serial Console
  
  #if HW_ENCODER == 1
    #define ENCODER_USE_INTERRUPTS
    #define ENCODER_PIN_A          36   //ENCODER PORT A
    #define ENCODER_PIN_B          39   //ENCODER PORT B
    #define ENCODER_PIN_C          34   //BUTTON  PORT C
  #endif
#endif //(ESP32)

/* RFM69 ONLY*/
#if HW_RF69 == 1
  #define RF_MODEM_CONFIG           RH_RF69::FSK_Rb2Fd5
  #include <conf_lora.h>            // Optional external config file for primarily encryption key
  #if ndef ENCRYPTION_KEY
    #define ENCRYPTION_KEY         {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15}
  #endif
#endif

/* I2C DEVICE ADDRESSES */
#define PCF8574_ADDRESS        (0x20)   // PCF8574 8 PORT IO Expander
#define BH1750_ADDRESS         (0x23)   //  BH1750 Illumination Sensor
#define SSD1306_ADDRESS        (0x3C)   // SSD1306 OLED
#define HDC1080_ADDRESS        (0x40)   // HDC1080 Humidity & Temperature Sensor
#define SI_ADDRESS             (0x40)   //  SI7021 Humidity & Temperature Sensor
#define INA219_ADDRESS         (0x41)   //  INA219 Voltage & Current Sensor
#define SHT35_ADDRESS          (0x44)   //   SHT35 Humidity & Temperature Sensor
#define ADS_ADDRESS            (0x48)   // ADS1115 ADC Converter
#define EEPROM_ADDRESS         (0x50)   //  EEPROM Non Volatile Memory
#define DS_ADDRESS             (0x68)   //  DS3232 Realtime Clock
#define BMP280_ADDRESS         (0x76)   //  BMP280 Atmospheric Pressure & Temperature Sensor, macro not used (as it does not work for some reason, probably a typo)

#if defined(ARDUINO_ARCH_SAMD)
  //#define Serial USE_SERIAL
  #define USE_SERIAL SerialUSB
  #include <RTCZero.h>
  RTCZero zrtc;
  #define RAIN_INT_PIN             7
  #define SPEED_INT_PIN            8
  #define RFM96_PIN_CS             5
  #define RFM96_PIN_IRQ            2
  #define RFM96_PIN_RESET         12
  #if HW_ENCODER == 1
    #define ENCODER_PIN_A         11  //Encoder A
    #define ENCODER_PIN_B          9  //Encoder B
    #define ENCODER_PIN_C         10  //Button C
  #endif
#endif
#if HW_SOLAR == 1
  #define CONF_SOLAR_PREFIX String("Sun:")
#elif HW_SOLAR_TBQ == 1
  #define CONF_SOLAR_PREFIX String("TBQ:")
#endif
/* ACTUAL PROGRAM START*/
#include <Arduino.h>
#include <Wire.h>
#include <CRCx.h>
#define CRC crcx

/* FUNCTIONS DEFINITIONS */
  void poll_SOIL();
  void get_ADS1115();
  void init_SSD1306();
  void update_SSD1306();
  void get_BH1750();
  void poll_BH1750();
  boolean init_BMP280();
  boolean poll_BMP280(boolean verb=false);
  void get_BMP280();
  void poll_HDC1080();
  void get_HDC1080();
  void poll_INA219();
  void init_pcf8574();
  //boolean adjust_time();  // adjusts the adjust_ms to fit halfway between 2 reporting periods
  #if SW_TOGGLE_DS_INT == 1
    void disableRTCAlarm(uint32_t epoch);
  #endif
  boolean init_http(uint16_t wifi_connect_timeout=5000);
  uint32_t header_date(String date_string);
  boolean  poll_RTC(boolean verb=false);

#if defined (ESP32)
  RTC_DATA_ATTR uint32_t bootCount = 0;
  RTC_DATA_ATTR uint32_t loopMillis, lastSampleMillis, lastUpdateMillis, lastPublishMillis, lastNtpMillis=0, lastPostMillis, lastCallHomeTime=0, callHomeInterval=CONF_IAS_CALL_HOME_INTERVAL;
  RTC_DATA_ATTR uint16_t periodSample=CONF_PERIOD_SAMPLE_MS, multiple=CONF_MULTIPLE_SAMPLES, sampleCounter=0; uint32_t periodPublish=CONF_PERIOD_PUBLISH_MS, interactiveTimeout=CONF_INTERACTIVE_TIMEOUT; static uint16_t modulus = CONF_MODULUS_S; // round unix time to nearest minute
  RTC_DATA_ATTR boolean lcd_enable=true, last_lcd_enable=true, mode_interactive=true; uint32_t last_screensaver_activity=0, screenOffMillis=-1;
  RTC_DATA_ATTR boolean reboot=false, collect_sample=true, publish_data=false, update_data=false, interactive_mode=true, go_set_ntp=true, go_send_buffer=false;
  //reboot: was this a reboot event?, collect_sample: collect sensor data in every loop; publish_data: pulish data after collecting sensor data;
  //update_data: switch to get_xxx and publish date in the final loop; interactive_mode: no sleeping and normal processing
  uint32_t last_packet_millis=0;
  RTC_DATA_ATTR int8_t rssi_wifi=127, i_ntp_counter=6;
#else
  uint32_t bootCount = 0;
  uint32_t loopMillis, lastSampleMillis, lastUpdateMillis, lastPublishMillis;
  uint16_t periodSample=CONF_PERIOD_SAMPLE_MS, multiple=CONF_MULTIPLE_SAMPLES, sampleCounter=0; uint32_t periodPublish=CONF_PERIOD_PUBLISH_MS, interactiveTimeout=CONF_INTERACTIVE_TIMEOUT; static uint16_t modulus = CONF_MODULUS_S; // round unix time to nearest minute
  boolean lcd_enable=true, last_lcd_enable=true, mode_interactive=true; uint32_t last_screensaver_activity=0, screenOffMillis=-1;
  boolean collect_sample=true, publish_data=false, update_data=false, interactive_mode=true, interactive_mode=true, go_set_ntp=true, go_send_buffer=false;
  #if HW_SLEEP == 1
    boolean reboot=false;
  #else
    boolean reboot=true;
  #endif
  uint32_t last_packet_millis=0;
  int8_t rssi_wifi=127;
#endif

#if SW_UPDATE_SPIFFS_CERT == 1
  // a one time helper routine to update the root certificate of IAS, not ususally required
  #include "FS.h"
  #include "SPIFFS.h"
  #include <CRCx.h>
  #define CRC crcx
  #define FORMAT_SPIFFS_IF_FAILED false

  void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Init CACERT...SPIFF Files: %s\r\n", dirname);
    File root = fs.open(dirname);
    if(!root){
      Serial.println("- failed to open directory");
      return;
    }
    if(!root.isDirectory()){
      Serial.println(" - not a directory");
      return;
    }
    File file = root.openNextFile();
    while(file){
      if(file.isDirectory()){
        Serial.print("  DIR : ");
        Serial.println(file.name());
        if(levels){
          listDir(fs, file.name(), levels -1);
        }
      } else {
        Serial.print("  FILE: ");
        Serial.print(file.name());
        Serial.print("\tSIZE: ");
        Serial.println(file.size());
      }
        file = root.openNextFile();
    }
  }
  bool checkCaCert(fs::FS &fs, const char * path){
    const char ROOT_CA[] = \
    "-----BEGIN CERTIFICATE-----\n" \
    "MIIEMjCCAxqgAwIBAgIBATANBgkqhkiG9w0BAQUFADB7MQswCQYDVQQGEwJHQjEb\n" \
    "MBkGA1UECAwSR3JlYXRlciBNYW5jaGVzdGVyMRAwDgYDVQQHDAdTYWxmb3JkMRow\n" \
    "GAYDVQQKDBFDb21vZG8gQ0EgTGltaXRlZDEhMB8GA1UEAwwYQUFBIENlcnRpZmlj\n" \
    "YXRlIFNlcnZpY2VzMB4XDTA0MDEwMTAwMDAwMFoXDTI4MTIzMTIzNTk1OVowezEL\n" \
    "MAkGA1UEBhMCR0IxGzAZBgNVBAgMEkdyZWF0ZXIgTWFuY2hlc3RlcjEQMA4GA1UE\n" \
    "BwwHU2FsZm9yZDEaMBgGA1UECgwRQ29tb2RvIENBIExpbWl0ZWQxITAfBgNVBAMM\n" \
    "GEFBQSBDZXJ0aWZpY2F0ZSBTZXJ2aWNlczCCASIwDQYJKoZIhvcNAQEBBQADggEP\n" \
    "ADCCAQoCggEBAL5AnfRu4ep2hxxNRUSOvkbIgwadwSr+GB+O5AL686tdUIoWMQua\n" \
    "BtDFcCLNSS1UY8y2bmhGC1Pqy0wkwLxyTurxFa70VJoSCsN6sjNg4tqJVfMiWPPe\n" \
    "3M/vg4aijJRPn2jymJBGhCfHdr/jzDUsi14HZGWCwEiwqJH5YZ92IFCokcdmtet4\n" \
    "YgNW8IoaE+oxox6gmf049vYnMlhvB/VruPsUK6+3qszWY19zjNoFmag4qMsXeDZR\n" \
    "rOme9Hg6jc8P2ULimAyrL58OAd7vn5lJ8S3frHRNG5i1R8XlKdH5kBjHYpy+g8cm\n" \
    "ez6KJcfA3Z3mNWgQIJ2P2N7Sw4ScDV7oL8kCAwEAAaOBwDCBvTAdBgNVHQ4EFgQU\n" \
    "oBEKIz6W8Qfs4q8p74Klf9AwpLQwDgYDVR0PAQH/BAQDAgEGMA8GA1UdEwEB/wQF\n" \
    "MAMBAf8wewYDVR0fBHQwcjA4oDagNIYyaHR0cDovL2NybC5jb21vZG9jYS5jb20v\n" \
    "QUFBQ2VydGlmaWNhdGVTZXJ2aWNlcy5jcmwwNqA0oDKGMGh0dHA6Ly9jcmwuY29t\n" \
    "b2RvLm5ldC9BQUFDZXJ0aWZpY2F0ZVNlcnZpY2VzLmNybDANBgkqhkiG9w0BAQUF\n" \
    "AAOCAQEACFb8AvCb6P+k+tZ7xkSAzk/ExfYAWMymtrwUSWgEdujm7l3sAg9g1o1Q\n" \
    "GE8mTgHj5rCl7r+8dFRBv/38ErjHT1r0iWAFf2C3BUrz9vHCv8S5dIa2LX1rzNLz\n" \
    "Rt0vxuBqw8M0Ayx9lt1awg6nCpnBBYurDC/zXDrPbDdVCYfeU0BsWO/8tqtlbgT2\n" \
    "G9w84FoVxp7Z8VlIMCFlA2zs6SFz7JsDoeA3raAVGI/6ugLOpyypEBMs1OUIJqsi\n" \
    "l2D4kF501KKaU73yqWjgom7C12yxow+ev+to51byrvLjKzg6CYG1a4XXvi3tPxq3\n" \
    "smPi9WIsgtRqAEFQ8TmDn5XpNpaYbg==\n" \
    "-----END CERTIFICATE-----\n";
    uint16_t i = 0;
    Serial.printf("  Reading: %s", path);
    File file = fs.open(path);
    if(!file || file.isDirectory()){
      Serial.println("- failed to open!");
      deleteFile(SPIFFS, path);
      Serial.println("  Writing NEW Cert: "+String(path));
      return writeFile(SPIFFS, path, ROOT_CA);
    }
    const uint32_t fsize=file.size();
    const uint32_t limit = fsize +1;
    byte message[fsize+1];      
    Serial.print(" Size: "+String(fsize));  
    //Serial.println("- read from file:");
    while(file.available()){
      message[i]=file.read();
      i++;
    }
    i=0;
    uint16_t file_crc=CRC::crc16(message, fsize);
    Serial.print(" CRC: "); Serial.print(file_crc); 
    if (file_crc != 63278){
      Serial.println(" Old Cert!");
      deleteFile(SPIFFS, path);
      return writeFile(SPIFFS, path, ROOT_CA);
    }
    else {
      Serial.println(" New Cert!");
      return true;
    }
  }
  bool writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("  Writing: %s", path);  
    File file = fs.open(path, FILE_WRITE);
    if(!file){
      Serial.println("- failed to open");
      return false;
    }
    if(file.print(message)){
      Serial.println(" - Written!");
      return true;
    } else {
      Serial.println(" - Failed!");
      return false;
    }
  }
  void deleteFile(fs::FS &fs, const char * path){
    Serial.printf("  Deleting: %s", path);
    if(fs.remove(path)){
      Serial.println(" - Deleted!");
    } else {
      Serial.println(" - Failed!");
    }
  }
  void update_spiffs_cert(){
    if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){
      Serial.println("SPIFFS Mount Failed");
      return;
    } /*
    const char root_old[]= \
    "-----BEGIN CERTIFICATE-----\n" \
    "MIIF2DCCA8CgAwIBAgIQTKr5yttjb+Af907YWwOGnTANBgkqhkiG9w0BAQwFADCB\n" \
    "hTELMAkGA1UEBhMCR0IxGzAZBgNVBAgTEkdyZWF0ZXIgTWFuY2hlc3RlcjEQMA4G\n" \
    "A1UEBxMHU2FsZm9yZDEaMBgGA1UEChMRQ09NT0RPIENBIExpbWl0ZWQxKzApBgNV\n" \
    "BAMTIkNPTU9ETyBSU0EgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMTAwMTE5\n" \
    "MDAwMDAwWhcNMzgwMTE4MjM1OTU5WjCBhTELMAkGA1UEBhMCR0IxGzAZBgNVBAgT\n" \
    "EkdyZWF0ZXIgTWFuY2hlc3RlcjEQMA4GA1UEBxMHU2FsZm9yZDEaMBgGA1UEChMR\n" \
    "Q09NT0RPIENBIExpbWl0ZWQxKzApBgNVBAMTIkNPTU9ETyBSU0EgQ2VydGlmaWNh\n" \
    "dGlvbiBBdXRob3JpdHkwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIKAoICAQCR\n" \
    "6FSS0gpWsawNJN3Fz0RndJkrN6N9I3AAcbxT38T6KhKPS38QVr2fcHK3YX/JSw8X\n" \
    "pz3jsARh7v8Rl8f0hj4K+j5c+ZPmNHrZFGvnnLOFoIJ6dq9xkNfs/Q36nGz637CC\n" \
    "9BR++b7Epi9Pf5l/tfxnQ3K9DADWietrLNPtj5gcFKt+5eNu/Nio5JIk2kNrYrhV\n" \
    "/erBvGy2i/MOjZrkm2xpmfh4SDBF1a3hDTxFYPwyllEnvGfDyi62a+pGx8cgoLEf\n" \
    "Zd5ICLqkTqnyg0Y3hOvozIFIQ2dOciqbXL1MGyiKXCJ7tKuY2e7gUYPDCUZObT6Z\n" \
    "+pUX2nwzV0E8jVHtC7ZcryxjGt9XyD+86V3Em69FmeKjWiS0uqlWPc9vqv9JWL7w\n" \
    "qP/0uK3pN/u6uPQLOvnoQ0IeidiEyxPx2bvhiWC4jChWrBQdnArncevPDt09qZah\n" \
    "SL0896+1DSJMwBGB7FY79tOi4lu3sgQiUpWAk2nojkxl8ZEDLXB0AuqLZxUpaVIC\n" \
    "u9ffUGpVRr+goyhhf3DQw6KqLCGqR84onAZFdr+CGCe01a60y1Dma/RMhnEw6abf\n" \
    "Fobg2P9A3fvQQoh/ozM6LlweQRGBY84YcWsr7KaKtzFcOmpH4MN5WdYgGq/yapiq\n" \
    "crxXStJLnbsQ/LBMQeXtHT1eKJ2czL+zUdqnR+WEUwIDAQABo0IwQDAdBgNVHQ4E\n" \
    "FgQUu69+Aj36pvE8hI6t7jiY7NkyMtQwDgYDVR0PAQH/BAQDAgEGMA8GA1UdEwEB\n" \
    "/wQFMAMBAf8wDQYJKoZIhvcNAQEMBQADggIBAArx1UaEt65Ru2yyTUEUAJNMnMvl\n" \
    "wFTPoCWOAvn9sKIN9SCYPBMtrFaisNZ+EZLpLrqeLppysb0ZRGxhNaKatBYSaVqM\n" \
    "4dc+pBroLwP0rmEdEBsqpIt6xf4FpuHA1sj+nq6PK7o9mfjYcwlYRm6mnPTXJ9OV\n" \
    "2jeDchzTc+CiR5kDOF3VSXkAKRzH7JsgHAckaVd4sjn8OoSgtZx8jb8uk2Intzna\n" \
    "FxiuvTwJaP+EmzzV1gsD41eeFPfR60/IvYcjt7ZJQ3mFXLrrkguhxuhoqEwWsRqZ\n" \
    "CuhTLJK7oQkYdQxlqHvLI7cawiiFwxv/0Cti76R7CZGYZ4wUAc1oBmpjIXUDgIiK\n" \
    "boHGhfKppC3n9KUkEEeDys30jXlYsQab5xoq2Z0B15R97QNKyvDb6KkBPvVWmcke\n" \
    "jkk9u+UJueBPSZI9FoJAzMxZxuY67RIuaTxslbH9qh17f4a+Hg4yRvv7E491f0yL\n" \
    "S0Zj/gA0QHDBw7mh3aZw4gSzQbzpgJHqZJx64SIDqZxubw5lT2yHh17zbqD5daWb\n" \
    "QOhTsiedSrnAdyGN/4fy3ryM7xfft0kL0fJuMAsaDk527RH89elWsn2/x20Kk4yl\n" \
    "0MC2Hb46TpSi125sC8KKfPog88Tk5c0NqMuRkrF8hey1FGlmDoLnzc7ILaZRfyHB\n" \
    "NVOFBkpdn627G190\n" \ 
    "-----END CERTIFICATE-----";      */
    listDir(SPIFFS, "/", 0);
    //deleteFile(SPIFFS, "/test.txt");
    //writeFile(SPIFFS, "/cert/iasRootCa.cer", root_old);
    if (!checkCaCert(SPIFFS, "/cert/iasRootCa.cer"));{
      //checkCaCert(SPIFFS, "/cert/iasRootCa.cer"); 
    }
    Serial.println( "...CACERT END" );
  }
#endif

  uint32_t compiledEpoch=0;
    struct data_packet {
      byte len;
      byte sender;
      byte receiver;
      byte type;
      byte noise;
      byte rssi;
      int16_t temperature_outside;
      int16_t temperature_inside;
      int16_t temperature_soil;
      uint16_t system_voltage;
      uint16_t relative_humidity;
      uint16_t rainfall_counter;
      uint16_t solar_illuminance;
      uint16_t solar_radiation;
      uint16_t atmospheric_pressure;
      uint16_t soil_moisture;
      uint16_t wind_speed;
      uint16_t wind_gust;
      uint16_t wind_direction;
      uint32_t system_energy;
      uint32_t unix_time;
      byte state;
      byte dummy;
      uint16_t checksum;
    };
  struct ack_packet {
    byte len;
    byte sender;
    byte receiver;
    byte type;
    uint16_t checksum;
  };
  struct gen_packet {
    byte len;
    byte sender;
    byte receiver;
    byte type;
  };
    struct time_packet {
    byte len;
    byte sender;
    byte receiver;
    byte type;
    uint32_t unix_time[4];
    uint16_t checksum;
  };
  union uni_definition {
    data_packet packet;
    ack_packet ack;
    gen_packet gen;
    time_packet tpk;
    byte bytes[sizeof(data_packet)];
    uint8_t stream[sizeof(data_packet)];
  };
  union uni_definition tx;
  union uni_definition rx;
  uint32_t last_millis = 0;
  uint8_t last_packet_type=0;

#if HW_EEPROM == 1
  boolean ds_ram_is_good = false, buffer_is_full = false;
  uint16_t head = -1, tail = -1, buffer_size = 0;
  uint16_t head_address = 0, tail_address = 0;
  uint16_t record_size = sizeof(data_packet);
  uint16_t max_records = 31744 / record_size; // 906 x 36bytes
  uint16_t eeprom_offset = 64;  //bytes
  uint16_t BUFFER_SIZE=max_records;
#endif

#if HW_WATCHDOG == 1
  boolean watchdog=HIGH;
  
  void init_watchdog(){
    pinMode(HB_PIN, OUTPUT);
    digitalWrite(HB_PIN, watchdog);
    Serial.println("Init WATCHDOG.OK Pin "+String(HB_PIN));
  }
  void call_watchdog(){
    watchdog=!watchdog;
    #if SW_DEBUG_HW_WATCHDOG == 1 && HW_SERIAL == 1
      Serial.println("  DEBUG Toggle Pin "+String(HB_PIN)+" to "+String(watchdog)+" call_watchdog()");
    #endif
    digitalWrite(HB_PIN, watchdog);
  }
#endif

#ifdef WDT_TIMEOUT_SEC
  void wdt_setup(void){
    USE_SERIAL.print("Init WDT......");
    esp_task_wdt_init(WDT_TIMEOUT_SEC, true);
    esp_err_t err = esp_task_wdt_add(NULL);
    switch (err) {
      case ESP_OK:
        USE_SERIAL.println("OK " + String(WDT_TIMEOUT_SEC)+"s");
        break;
      case ESP_ERR_INVALID_ARG:
        USE_SERIAL.println("ERROR Invalid Argument");
        break;
      case ESP_ERR_NO_MEM:
        USE_SERIAL.println("ERROR insufficent memory");
        break;
      case ESP_ERR_INVALID_STATE:
        USE_SERIAL.println("ERROR: not initialized");
        break;
      default:
        USE_SERIAL.print("ERROR: " + String(err));
      break;
    }
  }
#endif

  boolean ping_i2c(uint8_t address){
    uint8_t error;
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
      if (error == 0) {
        return true;
      } else {
        return false;
      }
  }

#if HW_REMOTE_I2C == 1
  boolean remote_i2c_online=false;
  void init_remote_i2c(){
    pinMode(REMOTE_I2C_PIN, OUTPUT);
    digitalWrite(REMOTE_I2C_PIN, HIGH);
    remote_i2c_online=true;
  }
  void shutdown_remote_i2c(){
    digitalWrite(13, LOW);
    remote_i2c_online=false;
  }
#endif

#if HW_SSD1306 == 1
  #include <SPI.h>
  #include <Wire.h>
  //#include <Adafruit_GFX.h>
  #include <Adafruit_SSD1306.h>
  #define SCREEN_WIDTH 128 // OLED display width, in pixels
  #define SCREEN_HEIGHT 64 // OLED display height, in pixels
  #define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
  Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET, I2C_FREQUENCY, I2C_FREQUENCY);
  extern uint8_t SmallFont[]; extern uint8_t TinyFont[];
  uint32_t lastLCDMillis=0; uint16_t periodLCDupdate=1000; boolean screensaver_enable=true; uint32_t screensaver_timeout=120000;
  uint8_t display_line=0; String display_string[8];

  void boot_display_SSD1306(String message, boolean newline=true, boolean replace_line=false){
      if(!newline){
        display_string[display_line]=message;
      } else { 
        display_string[display_line]=display_string[display_line] + message;
      }
      if ( replace_line ) {
        display_string[display_line]= message;
      }
      display.clearDisplay();
      #if SW_DEBUG_I2C == 1
        Serial.println("I2C Clock: "+String(Wire.getClock()));
      #endif
      display.setTextSize(1); // Draw 2X-scale text
      display.setTextColor(WHITE);
      for (int8_t i=0; i<=display_line; i++){ 
        display.setCursor(0, i*8);
        display.print(display_string[i]);
        //USE_SERIAL.println("Line: "+(String)display_line+" String["+(String)i+"]: "+display_string[i]);
      }
      display.display();
      if(newline){display_line = display_line + 1;
        if(display_line==8)
        {
          display_string[0]=display_string[1]; display_string[1]=display_string[2]; //move lines up by 1 position to create a scrolling effect
          display_string[2]=display_string[3]; display_string[3]=display_string[4]; 
          display_string[4]=display_string[5]; display_string[5]=display_string[6]; 
          display_string[6]=display_string[7]; 
          display_line=7;
        }
      }
  }
  void init_SSD1306(){
    #if HW_REMOTE_I2C == 1
      if(!remote_i2c_online){init_remote_i2c();}
    #endif
    #if HW_SERIAL == 1
      USE_SERIAL.print("Init SSD1306..");
    #endif
      if(!display.begin(SSD1306_SWITCHCAPVCC, SSD1306_ADDRESS)) { // Address 0x3C or 0x3D for 128x64 - this needs to be reset after sleep
      USE_SERIAL.println("ERROR hardware init_SSD1306()");
       }
    if(ping_i2c(SSD1306_ADDRESS)){
      if(reboot){
        //display.display(); // display the logo
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.setCursor(0, 0);
        display.print(String(SW_CUSTOMER_NAME)+"\n");
        display.print("---------------------\n");
        display.print("WX Station"+String(SW_SITE_NAME)+"\n");
        display.print("SW v"+String(SW_VERSION)+"      HW v"+String(HW_VERSION_MAJOR)+"."+String(HW_VERSION_MINOR)+"\n");
        display.print("from      "+String(__DATE__)+"\n");
        display.print(String(SW_EMAIL));
        display.print(String(SW_PHONE));
        display.print("Starting...       :-)");
        display.display();
        delay(2000);
      }
      if(lcd_enable){ boot_display_SSD1306("SSD1306...", false); }
      #if HW_SERIAL == 1
        USE_SERIAL.println("OK");
      #endif
      if(lcd_enable){ boot_display_SSD1306("OK"); }
      }
      #if HW_SERIAL == 1
      else {
        USE_SERIAL.println("ERROR ping 0x"+String(SSD1306_ADDRESS,HEX)+" init_SSD1306()");
        //No need to display anything on OLED as it is not available ;-)
      }
      #endif
  }
#endif
#if SW_IAS_OTA == 1
  #define COMPDATE __DATE__ __TIME__
  #define MODEBUTTON 34                      // Button pin on the esp for selecting modes. D3 for the Wemos!
  #define DEBUG_LVL 3
  #include <IOTAppStory.h>                  // IOTAppStory.com library
  IOTAppStory IAS(COMPDATE, MODEBUTTON);    // Initialize IOTAppStory
  String deviceName = "ias";
  String chipId;
  uint8_t perc=0;
  boolean ias_initialized=false;
  boolean init_IAS(){
    if (!ias_initialized){
      USE_SERIAL.print("Init IAS......");
      if ( init_http(5000) ) {  // only do if NOT initialized AND wifi is CONNECTED
      //chipId      = String(ESP_GETCHIPID);
      //chipId      = "-"+chipId.substring(chipId.length()-3);
      //deviceName += chipId;
      #if HW_SSD1306 == 1
        if(lcd_enable){ boot_display_SSD1306("IAS...", false); }
      #endif
  
      IAS.preSetDeviceName(INFLUX_HOST);          // preset deviceName this is also your MDNS responder: http://deviceName-123.local
      //IAS.preSetWifi(WIFI_SSID, WIFI_PWD);      // does not work for some reason
      IAS.preSetAutoConfig(false);                // Don't enter Config mode on WiFi failure
      //IAS.preSetAppName(F("test"));             // preset appName | The appName & appVersion get updated when you receive OTA updates. As this is your first app we will set it manually.
      //IAS.preSetAppVersion(F("1.0.3"));         // preset appVersion
      IAS.preSetAutoUpdate(false);                // Setting to true will make the device do an update-check immediately after calling begin(). The default is true
        #if HW_LED_D2 == 1
          pinMode(LED_PIN_OK, OUTPUT);
        #endif
      IAS.onFirmwareUpdateProgress([](int written, int total){
        #if HW_SPEED_INT == 1
          call_SPEED();
        #endif
        #if HW_RAIN_INT == 1
          call_RAIN();
        #endif
        #if HW_ENCODER == 1
          call_ENCODER();
        #endif
        if ( perc != written / (total / 100) ){
          perc = written / (total / 100);
          #if HW_SSD1306 == 1
            String progress = "IAS... "+String(perc)+"%";
            boot_display_SSD1306(progress, false, true);
          #endif
          if ( ( perc % 10 ) == 0 ) {
            Serial.print(String(perc) + "%");
          }
          else {
            Serial.print(".");
          }
          #if HW_LED_D2 == 1
            digitalWrite(LED_PIN_OK, !digitalRead(LED_PIN_OK));
          #endif
          #if HW_WATCHDOG == 1
            call_watchdog();
          #endif
          #ifdef WDT_TIMEOUT_SEC
            esp_task_wdt_reset();
          #endif
        }
      });
      IAS.onFirmwareUpdateSuccess([](){
        #if HW_LED_D2 == 1
          digitalWrite(LED_PIN_OK, LOW);
        #endif
      });
      IAS.onFirstBoot([]() {
        //IAS.eraseEEPROM('F');                 // Optional! What to do with EEPROM on First boot of the app? 'F' Fully erase | 'P' Partial erase
        //IAS.begin();
        #if HW_WATCHDOG == 1
          call_watchdog();
        #endif
        USE_SERIAL.print("Call Home..."); IAS.callHome(false); USE_SERIAL.println("OK");
      });
      
      IAS.begin();
      USE_SERIAL.print("OK ");
      ias_initialized=true; // check for this flag before calling home, in case it has not yet been initialized, due to lacking wifi
      if ( digitalRead(MODEBUTTON) ){ USE_SERIAL.print("Call Home..."); IAS.callHome(false); USE_SERIAL.println("OK");} else { //no spiffs update
      USE_SERIAL.println("Pin "+String(MODEBUTTON)+" Initialized:"+String(ias_initialized));}
      #if HW_SSD1306 == 1
        if(lcd_enable){ boot_display_SSD1306("OK"); }
      #endif
      return true;
      } else { USE_SERIAL.println("NO WIFI!"); return false; }
    } else { return true; }
    USE_SERIAL.println("PROBLEM!"); 
    return false;
  }
#endif
#if HW_LED_D2 == 1
  void init_LED(){
    pinMode(LED_PIN_OK, OUTPUT);
  }
#endif

#if HW_PCF8574JM == 1
  #define PORT0 0
  #define PORT1 1
  #define PORT2 2
  #define PORT3 3
  #define PORT4 4
  #define PORT5 5
  #define PORT6 6
  #define PORT7 7
  #include <Wire.h>    // Required for I2C communication
  #include <jm_PCF8574.h>
  jm_PCF8574 pcf8574; // I2C address fixed later by begin(...)

  void init_pcf8574(){
    USE_SERIAL.print("Init PCF8574.."); 
    #if HW_SSD1306 == 1
      if(lcd_enable){ boot_display_SSD1306("PCF8574...", false); }
    #endif
    if(ping_i2c(PCF8574_ADDRESS)){
      pcf8574.begin(PCF8574_ADDRESS);
      pcf8574.pinMode(PORT2, OUTPUT); //I2C
      pcf8574.pinMode(PORT3, OUTPUT); //BUZZER + LED
      pcf8574.pinMode(PORT4, OUTPUT); //RS485 Module
      pcf8574.pinMode(PORT5, OUTPUT); //RS485 Output (soil module)
      pcf8574.pinMode(PORT6, OUTPUT); //Wind Speed Supply
      pcf8574.pinMode(PORT7, OUTPUT); //Wind Direction Supply
      pcf8574.digitalWrite(PORT2, LOW); // turn everything off at the start
      pcf8574.digitalWrite(PORT3, HIGH);
      pcf8574.digitalWrite(PORT4, LOW);
      pcf8574.digitalWrite(PORT5, LOW);
      pcf8574.digitalWrite(PORT6, LOW);
      pcf8574.digitalWrite(PORT7, LOW);
     //switch all on and delay for startup of sensors
      pcf8574.digitalWrite(PORT2, HIGH);
      pcf8574.digitalWrite(PORT4, HIGH);
      pcf8574.digitalWrite(PORT5, HIGH);
      delay(1000);
    }
    #if HW_SSD1306 == 1
      if(lcd_enable){ boot_display_SSD1306("OK"); }
    #endif
    USE_SERIAL.print("OK P0-P7:"); 
    for (uint8_t port=0; port <= 7; port++){
      USE_SERIAL.print("P"+String(port)+" "+String(pcf8574.digitalRead(port))+" ");
    }
    USE_SERIAL.println(""); 
  }
#endif
#if HW_PCF8574 == 1
  #define PORT0 0
  #define PORT1 1
  #define PORT2 2
  #define PORT3 3
  #define PORT4 4
  #define PORT5 5
  #define PORT6 6
  #define PORT7 7
  #include <Wire.h>    // Required for I2C communication
  #include "PCF8574.h" // Required for PCF8574
  PCF8574 pcf8574(PCF8574_ADDRESS);
  void init_pcf8574(){
    USE_SERIAL.print("Init PCF8574.."); 
    #if HW_SSD1306 == 1
      if(lcd_enable){ boot_display_SSD1306("PCF8574...", false); }
    #endif
    if(ping_i2c(PCF8574_ADDRESS)){
      pcf8574.pinMode(PORT2, OUTPUT); //I2C in HW v1.1 (but not working as switching interferes with the i2c bus)
      pcf8574.pinMode(PORT3, OUTPUT); //BUZZER + LED
      pcf8574.pinMode(PORT4, OUTPUT); //RS485 Module
      pcf8574.pinMode(PORT5, OUTPUT); //RS485 Output (soil module)
      pcf8574.pinMode(PORT6, OUTPUT); //Wind Speed Supply
      pcf8574.pinMode(PORT7, OUTPUT); //Wind Direction Supply
      pcf8574.begin();
      USE_SERIAL.print("OK P0-7: "); 
      /* pcf8574.digitalWrite(PORT1, HIGH);*/ 
      /* pcf8574.digitalWrite(PORT2, LOW); */ USE_SERIAL.print("??"); // turn everything off at the start
      #if (HW_VERSION_MAJOR == 1) && (HW_VERSION_MINOR == 1)
        pcf8574.digitalWrite(PORT3, HIGH);  //BUZZER ON for a short "Chirp" on reboot
      #else
      pcf8574.digitalWrite(PORT3, LOW);  //BUZZER ON
      #endif
      #if HW_SOLAR == 1 || HW_SOLAR_TBQ == 1 || HW_SOIL == 1
        pcf8574.digitalWrite(PORT4, HIGH); USE_SERIAL.print("1");
        pcf8574.digitalWrite(PORT5, HIGH); USE_SERIAL.print("1");
      #else
        pcf8574.digitalWrite(PORT4, LOW); USE_SERIAL.print("0");
        pcf8574.digitalWrite(PORT5, LOW); USE_SERIAL.print("0");
      #endif
      pcf8574.digitalWrite(PORT6, LOW);
      pcf8574.digitalWrite(PORT7, LOW);
      //switch all on and delay for startup of sensors
      
      #if (HW_VERSION_MAJOR == 1) && (HW_VERSION_MINOR == 1)
        pcf8574.digitalWrite(PORT2, HIGH); USE_SERIAL.print("1");
        pcf8574.digitalWrite(PORT3, LOW); USE_SERIAL.print("0"); //BUZZER OFF
      #else
      pcf8574.digitalWrite(PORT2, LOW); USE_SERIAL.print("0");
      pcf8574.digitalWrite(PORT3, HIGH); USE_SERIAL.print("1"); //BUZZER OFF
      #endif
      USE_SERIAL.print("00");
      //delay(1000); // to let hardware stabilize (may not be strictly required)
      #if HW_SSD1306 == 1
        if(lcd_enable){ boot_display_SSD1306("OK"); }
      #endif
      //USE_SERIAL.print("OK P0-7: "); 
      /* Reading port values does not seem to work - needs more testing
      PCF8574::DigitalInput di = pcf8574.digitalReadAll();
      Serial.print(di.p0);
      Serial.print(di.p1);
      Serial.print(di.p2);
      Serial.print(di.p3);
      Serial.print(di.p4);
      Serial.print(di.p5);
      Serial.print(di.p6);
      Serial.println(di.p7);

      /* for (uint8_t port=0; port <= 7; port++){
        USE_SERIAL.print(String(pcf8574.digitalRead(port))+" ");
      }*/
      USE_SERIAL.println(""); 
    } else {
      #if HW_SERIAL == 1
        USE_SERIAL.println("ERROR ping 0x"+String(PCF8574_ADDRESS, HEX)+" init_pcf8574()");
      #endif
      #if HW_SSD1306 == 1
        if(lcd_enable){ boot_display_SSD1306("ERROR 0x"+String(PCF8574_ADDRESS, HEX)); }
      #endif
    }
  }
#endif
#if (HW_SOLAR == 1) || (HW_SOLAR_TBQ == 1)
  #include <Arduino.h>
  #include <Wire.h>    // Required for I2C communication
  #if HW_SOLAR == 1
    #define TIMEOUT_MS 500  // does not work, needs to be set in the library for now
  #elif HW_SOLAR_TBQ == 1
    #define TIMEOUT_MS 100
  #endif
  #include <esp32ModbusRTU.h>
  esp32ModbusRTU modbus(&Serial2, NULL);  // use Serial1 and pin 16 as RTS  
  uint16_t solar_radiation=0, solar_radiation_avg=0;
  uint32_t solar_radiation_total=0;
  boolean solar_last_error=true, solar_working=false;
  #if SW_MEDIUM_FILTER == 1 // sometimes the sensor values have outliers and this is noticable through dips in the average values. Simple fix to median filter, but may require further investigation to the root cause.
    #include "MedianFilterLib.h"
    MedianFilter<uint16_t> med_solar_radiation(CONF_MEDIAN_WINDOW);
    int16_t solar_radiation_count=-CONF_MEDIAN_WINDOW+1; //-3+1=-2 // count only the 3rd value (when counter=0) if filtering
  #else
    int16_t solar_radiation_count=0; // otherwise act normally
  #endif
  
/*void handleData(uint8_t serverAddress, esp32Modbus::FunctionCode fc, uint8_t* data, size_t length) {
    Serial.printf("id 0x%02x fc 0x%02x len %u: 0x", serverAddress, fc, length);
    for (size_t i = 0; i < length; ++i) {
      Serial.printf("%02X", data[i]);
    }
    solar_radiation=data[0]<<8|data[1]; //combine 2 bytes to a uint16_t
    Serial.println(" Result: "+String(solar_radiation)+" W/m2");
}*/
  void init_solar(){
    Serial.print("Init Solar....");
    #if (HW_PCF8574 == 1) || (HW_PCF8574JM == 1)
      pcf8574.pinMode(PORT4, OUTPUT);
      pcf8574.pinMode(PORT5, OUTPUT);
      pcf8574.begin();
      pcf8574.digitalWrite(PORT4, HIGH);
      pcf8574.digitalWrite(PORT5, HIGH);
    #endif

    Serial2.begin(9600, SERIAL_8N1);  // Modbus connection
    modbus.onData([](uint8_t serverAddress, esp32Modbus::FunctionCode fc, uint8_t* data, size_t length) {
      #if HW_SOLAR_TBQ == 1
      if (fc==0x03 && length==2){
        #if SW_MEDIUM_FILTER == 1 
          solar_radiation = med_solar_radiation.AddValue((uint16_t)data[0]<<8|data[1]);
        #else
          solar_radiation=data[0]<<8|data[1];
        #endif
        if ( solar_radiation_count >= 0 ) {
          solar_radiation_total=solar_radiation_total+solar_radiation;
        }
        solar_radiation_count++; 
        solar_working=true;
      }
      #elif HW_SOLAR == 1
        #if SW_MEDIUM_FILTER == 1 
          solar_radiation = med_solar_radiation.AddValue((uint16_t)data[0]<<8|data[1]);
        #else
          solar_radiation=data[0]<<8|data[1];
        #endif
        if ( solar_radiation_count >= 0 ) {
          solar_radiation_total=solar_radiation_total+solar_radiation;
        }
        solar_radiation_count++; 
        solar_working=true;
        Serial.printf("Solar Debug: id 0x%02x fc 0x%02x len %u: 0x", serverAddress, fc, length);
      #endif
      //Serial.println(" Result: "+String(solar_radiation)+"W/m2 ");
    });
    modbus.onError([](esp32Modbus::Error error){
      solar_working=false;
    });
    modbus.begin();  
    Serial.print("OK ");
    poll_solar();
    Serial.println("");
  }
  void poll_solar(){
    #if HW_SOLAR == 1
    modbus.readHoldingRegisters(1, 1, 1); //address 1, address 0x01, 1 register
    #elif HW_SOLAR_TBQ == 1
      modbus.readHoldingRegisters(1, 0, 1); //address 1, address 0x00, 1 register
    #endif
    if (solar_working){
      Serial.print(CONF_SOLAR_PREFIX+String(solar_radiation)+"W/m2 ");    
    } else {
      Serial.print(CONF_SOLAR_PREFIX+"x ");
    }
  }
  void get_solar(){
    solar_radiation_avg=uint16_t(0.5+(solar_radiation_total*10)/float(solar_radiation_count));
    solar_radiation_count=0; solar_radiation_total=0;
    #if HW_SERIAL == 1
      USE_SERIAL.print(CONF_SOLAR_PREFIX+String(solar_radiation_avg/10.0,1)+"W/m2 ");
    #endif     
  }
#endif // HW_SOLAR || HW_SOLAR_TBQ
#if HW_SAMTEMP == 1 // there is a problem with the second lot of readings.... check if you want to use this
  #include <TemperatureZero.h>
  TemperatureZero TempZero = TemperatureZero();
  int16_t sam_temperature=0, sam_temperature_total=0, sam_temperature_avg=0; uint16_t sam_count=0;
  void init_SAMTEMP(){
    TempZero.init();
  }
  void poll_SAMTEMP(){
    sam_temperature = TempZero.readInternalTemperature()*100+0.5; 
    sam_temperature_total = sam_temperature_total + sam_temperature;
    sam_count=sam_count+1; 
    #if HW_SERIAL == 1
      USE_SERIAL.print("SAM: "); USE_SERIAL.print(sam_temperature_avg/100.); USE_SERIAL.print("C ");
    #endif 
  }
  void get_SAMTEMP(){
    if (sam_count == 0){poll_SAMTEMP();}
    sam_temperature_avg=sam_temperature_total / sam_count;
    sam_count=0; sam_temperature_total=0;
    #if HW_SERIAL == 1
      USE_SERIAL.print("SAM:"); USE_SERIAL.print(sam_temperature_avg/100.); USE_SERIAL.print("C ");
    #endif     
  }
#endif
#if HW_SDCARD == 1
  #include <SPI.h>
  #include <SD.h>
  #define SD_FILE_CSV "/wx.csv"
#endif

#if HW_SOIL == 1
#if defined(ESP32)
  #define Serial1 Serial2
#endif
  #include <ModbusMaster.h>
  ModbusMaster node;
  uint16_t soil_moisture=0, soil_moisture_avg=0, soil_temperature=0, soil_temperature_avg=0;
#if defined(ESP32)
  RTC_DATA_ATTR int16_t soil_temperature_total=0, soil_moisture_total=0, soil_counter=0;
  RTC_DATA_ATTR uint32_t last_soil_failure_millis=50000;
#else
  int16_t soil_temperature_total=0, soil_moisture_total=0, soil_counter=0;
  uint32_t last_soil_failure_millis=50000;
#endif
  static const uint16_t ku16MBResponseTimeout = 200; // Modbus Timeout [milliseconds]

  void init_SOIL(){
    #if HW_SSD1306 == 1
      if(lcd_enable){ boot_display_SSD1306("SOIL RS485...", false); }
    #endif
    USE_SERIAL.print("SOIL RS485 Init...");
    #if (HW_PCF8574 == 1) || (HW_PCF8574JM == 1)
      pcf8574.pinMode(PORT4, OUTPUT);
      pcf8574.pinMode(PORT5, OUTPUT);
      pcf8574.begin();
      pcf8574.digitalWrite(PORT4, HIGH);
      pcf8574.digitalWrite(PORT5, HIGH);
    #endif
    Serial1.begin(19200, SERIAL_8N2); // use Serial (port 0); initialize Modbus communication baud rate
    node.begin(1, Serial1); // communicate with Modbus slave ID 1 over Serial (port 1) 
    USE_SERIAL.println("OK");
    #if HW_SSD1306 == 1
      if(lcd_enable){ boot_display_SSD1306("OK"); }
    #endif
    }
  
  void poll_SOIL(){
    static uint8_t i;
    uint8_t j, result;
    uint16_t soil_data[2];
    result = node.readInputRegisters(0, 2); // Read soil moisture and soil temperature = 2 bytes
    if (result == node.ku8MBSuccess ){
      for (j = 0; j < 2; j++) { soil_data[j] = node.getResponseBuffer(j); }
      soil_moisture=soil_data[0]; soil_temperature=soil_data[1];
      soil_moisture_total=soil_moisture_total+soil_moisture;
      soil_temperature_total=soil_temperature_total+soil_temperature;
      soil_counter=soil_counter+1;
    } else {
        last_soil_failure_millis=loopMillis;
        //soil_moisture_total=0; soil_temperature_total=0; soil_counter=1; soil_moisture=0; soil_temperature=0;
      }
      #if HW_SERIAL == 1
        USE_SERIAL.print("Soil: "); USE_SERIAL.print(float(soil_moisture*0.169491525-35.59322), 1); USE_SERIAL.print("% "); USE_SERIAL.print(soil_temperature/10., 1); USE_SERIAL.print("C ");
      #endif 
  }
  void get_SOIL(){
    if(soil_counter==0){poll_SOIL();}
    if (soil_counter!=0){
      soil_moisture_avg=uint16_t(0.5+100*((soil_moisture_total/soil_counter)*0.169491525-35.59322));
      soil_temperature_avg=int16_t(0.5+10*(soil_temperature_total/soil_counter)); //already uses *10 internally (10 x 10 = 100)
      soil_counter=0; soil_temperature_total=0; soil_moisture_total=0;
    } else {
      //USE_SERIAL.println("GET SOIL Failed!");
      soil_moisture_avg=-1; soil_temperature_avg=32767; soil_counter=0; soil_temperature_total=0; soil_moisture_total=0;
    }
    #if HW_SERIAL == 1
      USE_SERIAL.print("Soil:"); USE_SERIAL.print(soil_moisture_avg/100., 2); USE_SERIAL.print("% "); USE_SERIAL.print(soil_temperature_avg/100., 2); USE_SERIAL.print("C ");
    #endif 
  }
#endif

#if HW_WIND_DIR == 1
  float windVaneCosTotal = 0.0;
  float windVaneSinTotal = 0.0;
  unsigned int windVaneReadingCount = 0;

#endif

#if HW_EEPROM == 1
  struct record_t_old{
    uint8_t len;
    boolean buffer_is_full;
    uint16_t head;
    uint32_t rain_counter;
    uint32_t ina_energy_total;
    uint16_t tail;
    uint16_t crc;
  };

  struct record_t{
    uint8_t len;
    uint8_t ver;
    uint8_t store;
    boolean buffer_is_full;
    uint16_t status_flags;
    uint16_t head;
    uint16_t packet_counter;          // counter for last successfully published packet for debugging purposes
    uint16_t reboot_counter;          // incremented on reboot for debugging purposes
    uint32_t rain_counter;
    uint32_t ina_energy_total;
    uint32_t periodPublishTimestamp;  //epoch timestamp to show last successfully published packet
    uint16_t tail;
    uint16_t crc;
  };
  union uni_t {
    record_t rec;
    record_t_old rec_old;
    byte bytes[sizeof(record_t)];
  };
  union uni_t sram;
#endif
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
#if HW_RAIN_INT == 1
  volatile uint32_t rain_counter_last = 0;
  volatile uint32_t rain_counter = 0; // 0.2794 mm per impulse
  volatile uint32_t rain_event_millis = 0;
  volatile boolean rain_event = false;
  void init_RAIN(){
    #if HW_SERIAL == 1
      USE_SERIAL.print("Init RAIN INT.");
    #endif
    pinMode(RAIN_INT_PIN, INPUT); 
    #if defined(__AVR_ATmega1284P__)
      attachInterrupt(0,rain_int,FALLING); // digital pin 2 // down
    #elif defined (ESP32)
      attachInterrupt(RAIN_INT_PIN,rain_int,RISING); // digital pin 2 // down
    #else
      attachInterrupt(digitalPinToInterrupt(RAIN_INT_PIN),rain_int,FALLING); // digital pin 2 // down
    #endif
    #if HW_SERIAL == 1
      USE_SERIAL.println("OK");
    #endif
  }
  int rain_period=0;
  IRAM_ATTR void rain_int(){
    portENTER_CRITICAL_ISR(&mux);
     rain_period = millis() - rain_event_millis;
     if (rain_period > 0 ) { //first part of pulse noise is <1ms
       rain_event_millis = millis();
         //if (rain_period < 150 ) { //2nd part of pulse pulse period is 80-100ms in experimental measurements
          rain_event=true;
          #if (HW_SERIAL == 1) && (SW_DEBUG_RAIN == 1)
            USE_SERIAL.print("Rain INTERRUPT! "); USE_SERIAL.println(rain_counter);
          #endif 
         //}
      }
      //USE_SERIAL.print("Rain INTERRUPT 1 "); USE_SERIAL.println(rain_counter);
    portEXIT_CRITICAL_ISR(&mux);
  }
  void call_RAIN(){ // this needs to be called every loop in case an interrupt has occured
      if (rain_event){
      rain_counter = rain_counter + 1;
      rain_event = false;
      sram.rec.rain_counter=rain_counter;
      write_dsram();
      #if HW_SERIAL == 1
      #endif
      }
  }
  void get_RAIN(){
    #if HW_SERIAL == 1
      USE_SERIAL.print("Rain:"); USE_SERIAL.print(rain_counter); USE_SERIAL.print(" ");
    #endif
  }
#endif
#if HW_SPEED_INT == 1
  /* This works by counting impulses from the wind anemometer. By timing the impulses a frequency is calculated, which is proportional and converted to a speed.
   * Two time periods are available, the "first_speed_event_millis" the start of a (longer) speed measuring period and the "last_speed_gust_event_millis" which measures consecutive 3s periods for gusts. */
  volatile boolean speed_event = false;
  volatile uint16_t speed_counter = 0;
  volatile uint32_t speed_event_millis = 0;
  uint32_t last_speed_event_millis = 0, last_speed_gust_event_millis = 0, last_avg_speed_gust_event_millis = 0, first_speed_event_millis = 0;
  uint32_t speed_gust_delta = 0;
  uint16_t speed_gust_counter = 0;
  uint16_t speed_avg = -1, speed_gust_max = -1, speed_gust_max_avg=0, speed_gust_now=0;
  uint16_t speed_counter_total = 0;


  void init_SPEED(){
    #if HW_SERIAL == 1
      USE_SERIAL.print("Init WIND INT.");
    #endif
      pinMode(SPEED_INT_PIN, INPUT); 
    #if defined(__AVR_ATmega1284P__)
      attachInterrupt(1,speed_int,FALLING);
    #elif defined(ESP32)
      attachInterrupt(digitalPinToInterrupt(SPEED_INT_PIN),speed_int,FALLING);
    #else
      attachInterrupt(digitalPinToInterrupt(SPEED_INT_PIN),speed_int,FALLING);
    #endif
    #if HW_SERIAL == 1
      USE_SERIAL.println("OK");
    #endif
  }
IRAM_ATTR void speed_int(){
  portENTER_CRITICAL_ISR(&mux);
    speed_counter++;
    if ( speed_counter % 2 == 0) {speed_event_millis=millis(); speed_event = true;} // speed_event_millis only needed on even counts, as 2 impulses per rotation, to avoid inconsistencies due to magnet offsets (untested theory only)
  portEXIT_CRITICAL_ISR(&mux);
  }
  void call_SPEED(){ // this needs to be called every loop in case an interrupt has occured
      if ( speed_counter < 2){ speed_gust_max=0; first_speed_event_millis = speed_event_millis; } // reset from 65535 as there is an active interrupt source from the wind anemometer
      if (speed_event){  //only true on even impulses to measure whole rotations in case the magnets are slightly offset.
      portENTER_CRITICAL_ISR(&mux);   
        speed_event = false; 
      portEXIT_CRITICAL_ISR(&mux);
        //if ( speed_counter % 2 == 0) { //use only multiple turns -> 2 impulses per turn
          speed_gust_delta = speed_event_millis - last_speed_gust_event_millis;
          speed_counter_total = speed_counter;
          last_speed_event_millis = speed_event_millis;
          #if (HW_SERIAL == 1) && (SW_DEBUG_SPEED_INT == 1)
            USE_SERIAL.print("   DEBUG call_SPEED() Last Gust: Delta (ms): "); USE_SERIAL.print(speed_gust_delta); USE_SERIAL.print(" Counter "); USE_SERIAL.println(speed_counter);
          #endif
          if ( speed_gust_delta > 3000 ) { //calculate wind gust only after 3s averaging window
            #if (HW_SERIAL == 1) && (SW_DEBUG_SPEED_INT == 1)
              USE_SERIAL.print("    DEBUG call_SPEED() Last Gust: Delta (ms): "); USE_SERIAL.print(speed_gust_delta); USE_SERIAL.print(" Counter "); USE_SERIAL.print(speed_counter); USE_SERIAL.print(" Counters: "); USE_SERIAL.print(speed_counter); USE_SERIAL.print(" "); USE_SERIAL.print(speed_gust_counter); USE_SERIAL.print(" Deltas: "); USE_SERIAL.println(speed_gust_delta );
            #endif
              if (speed_gust_delta == 0){speed_gust_now=65535;} else {speed_gust_now = uint16_t(0.5 + (speed_counter - speed_gust_counter) / (speed_gust_delta / 1000.)*WIND_FACTOR*100);}
              #if (HW_SERIAL == 1) && (SW_DEBUG_SPEED_INT == 1)
                USE_SERIAL.println("     DEBUG call_SPEED() Counts:"+String(speed_counter - speed_gust_counter)+" gust_delta:"+String(speed_gust_delta)+" Gust Now:"+String(speed_gust_now/100.)+"km/h "+String((speed_counter - speed_gust_counter)/float(speed_gust_delta/1000.))+"Hz");
              #endif
              last_speed_gust_event_millis = speed_event_millis;
              speed_gust_counter = speed_counter;
                if ( speed_gust_max < speed_gust_now ){ speed_gust_max=speed_gust_now; 
                  #if (HW_SERIAL == 1) && (SW_DEBUG_SPEED_INT == 1)
                    USE_SERIAL.print("      DEBUG call_SPEED() Gust Max (km/h): "); USE_SERIAL.println(speed_gust_max);
                  #endif
                }
          }
        //}
        #if (HW_SERIAL == 1) && (SW_DEBUG_SPEED_INT == 1)
          USE_SERIAL.print("   DEBUG call_SPEED() Even INT! mod:"); USE_SERIAL.println(speed_counter%2);
        #endif
      }
  }
  void poll_SPEED(){
    if ( (last_speed_event_millis - first_speed_event_millis) == 0){speed_avg=-1;} else{ speed_avg = uint16_t(0.5 + speed_counter_total / float((loopMillis - first_speed_event_millis) / 1000.)*WIND_FACTOR*100); }
    if (speed_avg > speed_gust_max) { speed_gust_max=speed_avg;}
    #if HW_SERIAL == 1
      USE_SERIAL.print("Wind:"); USE_SERIAL.print(speed_avg/100.); USE_SERIAL.print(" "); USE_SERIAL.print(speed_gust_max/100.); 
    #endif
  }
  void get_SPEED(){
    poll_SPEED();
    speed_gust_max_avg=speed_gust_max;
    // this causes an underread with any (interrupt) blocking calls as the time is longer with missing speed_counter total numbers.
    // first_speed_event_millis will be reset again at the first speed_int event (now there is actually some wind)
    first_speed_event_millis=last_speed_event_millis=millis(); speed_counter_total=0; ; speed_counter=0; speed_gust_max=0; speed_gust_counter=0; 
  }
#endif
volatile uint16_t rtc_int_counter=0;
#if HW_ADS1115 == 1
  // use median filter with window size 3 here for ADC values.
  #include <Adafruit_ADS1015.h>
  Adafruit_ADS1115 ads;
  #if SW_MEDIUM_FILTER == 1 // sometimes the ADC values have outliers and this is noticable through dips in the average values. Simple fix to median filter, but may require further investigation to the root cause.
    #include "MedianFilterLib.h"
    MedianFilter<int16_t> med_adc0(CONF_MEDIAN_WINDOW); MedianFilter<int16_t> med_adc1(CONF_MEDIAN_WINDOW); MedianFilter<int16_t> med_adc2(CONF_MEDIAN_WINDOW); MedianFilter<int16_t> med_adc3(CONF_MEDIAN_WINDOW);
    int adc_counter=-CONF_MEDIAN_WINDOW+1; //-3+1=-2 // count only the 3rd value (when counter=0) if filtering
  #else
    int adc_counter=0; // otherwise act normally
  #endif
  const float pi = 3.141579;
  int16_t adc0, adc1, adc2, adc3; //int16_t as values can also be negative -2^15 to +2^15-1
  uint32_t adc0_total=0, adc1_total=0, adc2_total=0, adc3_total;
  int16_t adc0_avg=0, adc1_avg = 0, adc2_avg=0;
  uint16_t adc3_avg=0; //36000 
  boolean ads_last_error=true;
  void init_ADS1115(){
    #if HW_SSD1306 == 1
      if(lcd_enable){ boot_display_SSD1306("ADS1115..", false); }
    #endif
    #if HW_SERIAL == 1
      USE_SERIAL.print("Init ADS1115..");
    #endif
    if(ping_i2c(ADS_ADDRESS)){
      ads.begin();
      ads.setGain(GAIN_TWOTHIRDS); // 2/3x gain +/- 6.144V  1 bit = 0.1875mV
      ads_last_error=false;
    #if HW_SERIAL == 1
      USE_SERIAL.println("OK 0-3:"+String(ads.readADC_SingleEnded(0))+":"+String(ads.readADC_SingleEnded(1))+":"+String(ads.readADC_SingleEnded(2))+":"+String(ads.readADC_SingleEnded(3)));
    #endif
    #if HW_SSD1306 == 1
      if(lcd_enable){ boot_display_SSD1306("OK"); }
    #endif
    }
    else
    {
    #if HW_SERIAL == 1
      USE_SERIAL.println("ERROR ping 0x"+String(ADS_ADDRESS,HEX)+" init_ADS1115()");
    #endif
    #if HW_SSD1306 == 1
      if(lcd_enable){ boot_display_SSD1306("ERROR 0x"+String(ADS_ADDRESS,HEX)); }
    #endif
    }
  }
  uint8_t rawToRadians(uint16_t analogRaw){
    if(analogRaw <= 678) return (-1); // Open circuit?  Probably means the sensor is not connected
    if(analogRaw >= 679 && analogRaw <= 1851) return 12; //270°
    if(analogRaw >= 1852 && analogRaw <= 2863) return 14;//315°
    if(analogRaw >= 2864 && analogRaw <= 3734) return 13;//292.5°
    if(analogRaw >= 3735 && analogRaw <= 4806) return 0;//000°
    if(analogRaw >= 4807 && analogRaw <= 6144) return 15;//337.5°
    if(analogRaw >= 6145 && analogRaw <= 7033) return 10;//225°
    if(analogRaw >= 7034 && analogRaw <= 8483) return 11;//247.5°
    if(analogRaw >= 8484 && analogRaw <= 10145) return 2;//45°
    if(analogRaw >= 10146 && analogRaw <= 11641) return 1;//22.5°
    if(analogRaw >= 11642 && analogRaw <= 13028) return 8;//180°
    if(analogRaw >= 13029 && analogRaw <= 13910) return 9;//202.5°
    if(analogRaw >= 13911 && analogRaw <= 14925) return 6;//135°
    if(analogRaw >= 14926 && analogRaw <= 15712) return 7;//157.5°
    if(analogRaw >= 15713 && analogRaw <= 16080) return 4;//90°
    if(analogRaw >= 16081 && analogRaw <= 16313) return 3;//67.5°
    if(analogRaw >= 16314 && analogRaw <= 17038) return 5;//112.5°
    if(analogRaw >= 17039) return(-1); // Open circuit?  Probably means the sensor is not connected
  }
  void poll_ADS1115(){
    if(ping_i2c(ADS_ADDRESS)){
      if(ads_last_error){init_ADS1115();}
      #if SW_MEDIUM_FILTER == 1
        adc0 = med_adc0.AddValue(ads.readADC_SingleEnded(0));
        adc1 = med_adc1.AddValue(ads.readADC_SingleEnded(1));
        adc2 = med_adc2.AddValue(ads.readADC_SingleEnded(2));
        //adc3 = med_adc3.AddValue(ads.readADC_SingleEnded(3)); // Does not make sense for wind direction
        adc3 = ads.readADC_SingleEnded(3);
        // Only use the 3rd median value (counter starts at -1); probably no further filtering needed.
        if (adc_counter >= 0 ) { adc0_total=adc0_total+adc0; adc1_total=adc1_total+adc1; adc2_total=adc2_total+adc2; adc3_total = adc3_total + rawToRadians(adc3);}
      #else
        adc0 = ads.readADC_SingleEnded(0);
        adc1 = ads.readADC_SingleEnded(1);
        adc2 = ads.readADC_SingleEnded(2);
        adc3 = ads.readADC_SingleEnded(3);
        if (adc0 >= 0 && adc0 < 65000) { adc0_total=adc0_total+adc0; } // Poor man's filtering. valid reading @ 3.3V and 10k pulldown
        if (adc1 >  0 && adc1 < 65000) { adc1_total=adc1_total+adc1); } // valid reading @ 3.3V and 10k pulldown
        if (adc2 >= 0 && adc2 <= 32767) { adc2_total=adc2_total+adc2; } // valid reading @ 3.3V and 10k pulldown
        if (adc3 >= 678 && adc3 <= 17039) { adc3_total = adc3_total + rawToRadians(adc3);} //valid reading @ 3.3V and 10k pullup (with wind vane conrad)
      #endif
      adc_counter=adc_counter+1; // this is problematic if error occures (in the non-filtering case), but counter is incremented anyway
      ads_last_error=false;
      #if HW_SERIAL == 1
        USE_SERIAL.print("ADS1115:"); USE_SERIAL.print(adc0); USE_SERIAL.print(" "); USE_SERIAL.print(int16_t(adc2*ADC2_RATIO*100+0.005)/100.); USE_SERIAL.print("V "); USE_SERIAL.print(uint16_t((rawToRadians(adc3)*22.5*100)+0.005)/100.0); USE_SERIAL.print(" "); 
        Serial.print("rtc_int:"+String(rtc_int_counter)+" Min:" + String(esp_get_minimum_free_heap_size())); Serial.print(" Cap:" + String( heap_caps_get_largest_free_block(0))); Serial.print(" Free:" + String(esp_get_free_heap_size())+" ");
      #endif
    }
    else {
      #if HW_SERIAL == 1
        USE_SERIAL.print("ADS1115:x ");
      #endif
      ads_last_error=true;
    }
  }
  void get_ADS1115(){
    if ( adc_counter <= 0 ) { poll_ADS1115(); poll_ADS1115(); poll_ADS1115();}
    if ( adc_counter <= 0 ) { adc0_avg=-32768; adc1_avg=-32768 ; adc2_avg=-32768 ; adc3_avg=-32768; } else {
    adc0_avg = uint16_t(0.5+((float)(adc0_total / adc_counter)* 0.1875 / 5000 * 32.4)*100); // 0.1875 mV per bit, 32.4m/s full scale, 
    if (adc3_total > 0 ) { adc3_avg = uint16_t(0.5 + 360*100*(float)((adc3_total/(float)adc_counter)/16.0)); } else { adc3_avg=65535; }
    //adc1_avg = (uint16_t)(((float)(adc1_total / adc_counter) * 0.1875 / 1000) * 3.2619 * 100);
    adc2_avg = int16_t( 0.5 + ((float)(adc2_total / adc_counter)* ADC2_RATIO * 100.0)); 
    adc_counter = 0; adc0_total = 0; adc1_total = 0; adc2_total = 0; adc3_total=0;
    }
    #if HW_SERIAL == 1
      USE_SERIAL.print("ADS1115:"); USE_SERIAL.print(adc0_avg/100.); USE_SERIAL.print(" "); USE_SERIAL.print(adc1_avg/100.); USE_SERIAL.print(" "); USE_SERIAL.print(adc2_avg/100.); USE_SERIAL.print(" ");
    #endif
  }
#endif

#if HW_INA219 == 1
  #include <Wire.h>
  #include <INA219.h>
  INA219 ina219( INA219::I2C_ADDR_41 );  // unsure how to use this constructor properly with the INA219_ADDRESS macro
 // ina219::INA219(t_i2caddr addr): i2c_address(0x41);
  #define R_SHUNT 0.1
  #define V_SHUNT_MAX 0.1
  #define V_BUS_MAX 16
  #define I_MAX_EXPECTED 1
  int16_t ina_current = 0, ina_voltage = 0, ina_energy = 0;
  int16_t ina_current_avg = 0, ina_voltage_avg = 0, ina_energy_avg = 0;
  RTC_DATA_ATTR int32_t ina_current_total = 0, ina_voltage_total = 0, ina_energy_total = 0;
  uint32_t ina_last_read=0; // used to calculate Ah
  uint32_t ina_tick;         // current read time - last read
  boolean ina_last_error = true;
  #if SW_MEDIUM_FILTER == 1 // sometimes the sensor values have outliers and this is noticable through dips in the average values. Simple fix to median filter, but may require further investigation to the root cause.
    #include "MedianFilterLib.h"
    MedianFilter<int16_t> med_ina_current(CONF_MEDIAN_WINDOW); MedianFilter<int16_t> med_ina_voltage(CONF_MEDIAN_WINDOW);
    int16_t ina_counter=-CONF_MEDIAN_WINDOW+1; //-3+1=-2 // count only the 3rd value (when counter=0) if filtering
  #else
    int16_t ina_counter=0; // otherwise act normally
  #endif

  void init_INA219(){
    USE_SERIAL.print("Init INA219...");
    #if HW_SSD1306 == 1
      if(lcd_enable){ boot_display_SSD1306("INA219...", false); }
    #endif
    if(ping_i2c(INA219_ADDRESS)){
      ina219.begin();
      ina219.configure(INA219::RANGE_16V, INA219::GAIN_2_80MV, INA219::ADC_128SAMP, INA219::ADC_128SAMP, INA219::CONT_SH_BUS); // configure INA219 for averaging at 128 samples (8.51ms) continous measurments on shunt and bus.
      ina219.calibrate(R_SHUNT, V_SHUNT_MAX, V_BUS_MAX, I_MAX_EXPECTED); // calibrate INA219 with out shunt values
      USE_SERIAL.println("OK: "+String(ina219.busVoltage())+"V "+String(ina219.shuntCurrent())+"mA");
      #if HW_SSD1306 == 1
        if(lcd_enable){ boot_display_SSD1306("OK"); }
      #endif
        }
    else {
      USE_SERIAL.println("ERROR ping 0x"+String(INA219_ADDRESS)+" init_INA219()");
      #if HW_SSD1306 == 1
        if(lcd_enable){ boot_display_SSD1306("ERROR (ping)"); }
      #endif
    }
  }
  void poll_INA219() {
    uint32_t count = 0, ina_read_millis;
    if(ping_i2c(INA219_ADDRESS)){
      #if SW_MEDIUM_FILTER == 1
        ina_voltage = med_ina_voltage.AddValue(ina219.busVoltage()*1000);
        ina_current = med_ina_current.AddValue(ina219.shuntCurrent()*100000);
      #else
        ina_voltage = ina219.busVoltage()*1000; //3 decimal places
        ina_current = ina219.shuntCurrent()*100000; //1000*100 A -> mA; *1000 round to 2 decimal places
      #endif
      //USE_SERIAL.print("ina_voltage: "+String(ina_voltage)); USE_SERIAL.println(" ina_current: "+String(ina_current)); 
      ina_read_millis = millis();
      if (ina_counter >= 0 ){
        ina_tick = ina_read_millis - ina_last_read;
        ina_energy_total = ina_energy_total + (ina_current * (ina_tick/3600000.0));
        #if HW_EEPROM
          if (sram.rec.ina_energy_total!=ina_energy_total ){
            sram.rec.ina_energy_total=ina_energy_total;
            write_dsram();
          }
        #endif
        ina_last_read = ina_read_millis;
        ina_voltage_total=ina_voltage_total+ina_voltage;
        ina_current_total=ina_current_total+ina_current;
      }
      ina_counter = ina_counter + 1;
      #if HW_SERIAL == 1
        USE_SERIAL.print("INA219: "); USE_SERIAL.print((float)ina_voltage/1000); USE_SERIAL.print("V "); USE_SERIAL.print((float)ina_current/100.0); USE_SERIAL.print("mA "); USE_SERIAL.print(ina_energy_total/100.0,2); USE_SERIAL.print("mAh ");
      #endif 
      ina219.recalibrate(); // prepare for next read -- this is security just in case the ina219 is reset by transient current
      ina219.reconfig(); }
    else {
      USE_SERIAL.print("INA219:x ");
      ina_last_error=true;
    }
    USE_SERIAL.flush();
  }
  void get_INA219(){
      if ( ina_counter <= 0 ) { poll_INA219(); poll_INA219(); poll_INA219(); }
      if ( ina_counter <= 0 ) { ina_voltage_avg = 32767; ina_current_avg=32767;} else {
        ina_voltage_avg = int16_t(0.5 + float(ina_voltage_total) / ina_counter);
        ina_current_avg = int16_t(0.5 + (ina_current_total / float(ina_counter))); 
        #if HW_SERIAL == 1
          USE_SERIAL.print("INA219:"); USE_SERIAL.print(ina_voltage_avg/1000.0,3); USE_SERIAL.print("V "); USE_SERIAL.print(ina_current_avg/100); USE_SERIAL.print("mA "); USE_SERIAL.print(ina_energy_total/100,2); USE_SERIAL.print("mAh ");
        #endif 
        ina_counter = 0; ina_voltage_total = 0; ina_current_total = 0;
      }
  }
#endif

#if HW_SI7021 == 1
  #include "Adafruit_Si7021.h"
  Adafruit_Si7021 si = Adafruit_Si7021();
  int16_t si_temperature, si_humidity;
  int32_t si_temperature_total = 0; uint32_t si_humidity_total = 0;
  int16_t si_temperature_avg = 0; uint16_t si_humidity_avg = 0;
  boolean si_last_error=true;
  #if SW_MEDIUM_FILTER == 1 // sometimes the ADC values have outliers and this is noticable through dips in the average values. Simple fix to median filter, but may require further investigation to the root cause.
    #include "MedianFilterLib.h"
    MedianFilter<int16_t> med_si_temperature(CONF_MEDIAN_WINDOW); MedianFilter<int16_t> med_si_humidity(CONF_MEDIAN_WINDOW);
    int16_t si_counter = -CONF_MEDIAN_WINDOW+1; //-3+1=-2 // count only the 3rd value (when counter=0) if filtering
  #else
    int16_t si_counter = 0;
  #endif
  
  void init_SI7021(){
    #if HW_SSD1306 == 1
      if(lcd_enable){ boot_display_SSD1306("SI7021...", false); }
    #endif
    #if HW_SERIAL == 1
      USE_SERIAL.print("Init SI7021...");
    #endif
    #if HW_REMOTE_I2C == 1
      if(!remote_i2c_online){init_remote_i2c();}
    #endif
    if(ping_i2c(SI_ADDRESS)){
      if(si.begin()){
      #if HW_SERIAL == 1
        USE_SERIAL.println("OK "+String(si.readHumidity(),2)+"% "+String(si.readTemperature(),2)+"C");
      #endif
      #if HW_SSD1306 == 1
        if(lcd_enable){ boot_display_SSD1306("OK"); }
      #endif
      si_last_error=false;
      }
    }
    else {
      #if HW_SERIAL == 1
        USE_SERIAL.println("ERROR ping 0x"+String(SI_ADDRESS)+" init_SI7021()");
      #endif
      #if HW_SSD1306 == 1
        if(lcd_enable){ boot_display_SSD1306("ERROR"); }
      #endif
      si_last_error=true;
    }
  }
  
  boolean poll_SI7021(boolean verb=false){
    if(ping_i2c(SI_ADDRESS)){
      if(si_last_error){init_SI7021();}
      #if SW_MEDIUM_FILTER == 1 
        si_humidity = med_si_humidity.AddValue((int16_t)0.5+100*si.readHumidity());
        si_temperature = med_si_temperature.AddValue((int16_t)0.5+100*si.readTemperature());
      #else
        si_humidity = (int16_t)0.5+100*si.readHumidity(); // float change to int for better performance
        si_temperature = (int16_t)0.5+100*si.readTemperature(); // float change to int for better performance
      #endif
      if ((si_humidity < 0) && (si_humidity >-500)){si_humidity = 0;}
      if ((si_humidity < 11000) && (si_humidity >10000)){si_humidity = 10000;} // as per data sheet recommendations
      if ( si_counter >= 0 ) {  // exclude the first few median filter measurements
        si_temperature_total = si_temperature_total + si_temperature;
        si_humidity_total = si_humidity_total + si_humidity;
      }
      si_counter = si_counter + 1;
      #if SW_DEBUG_SI == 1 && HW_SERIAL == 1
        USE_SERIAL.println("SI7021:"+String(si_humidity)+"% "+String(si_temperature)+"C total%:"+String(si_humidity_total)+" totalC:"+String(si_temperature_total)+" count:"+String(si_counter));
      #endif
      si_last_error=false;
      #if HW_SERIAL == 1
        if (verb){ USE_SERIAL.print("SI7021:"); USE_SERIAL.print(si_temperature/100.0); USE_SERIAL.print("C "); USE_SERIAL.print(si_humidity/100.0); USE_SERIAL.print("% "); }
      #endif 
    } else {
      #if HW_SERIAL == 1
        USE_SERIAL.print("SI7021:x ");
      #endif 
      si_last_error=true;
    }
  }
  void get_SI7021(){
    if ( si_counter <= 0 ) { poll_SI7021(); poll_SI7021(); poll_SI7021(); }
    if ( si_counter <= 0 ) { si_temperature_avg = 32767; si_humidity_avg=32767; } else {
    si_temperature_avg = int16_t(0.5+si_temperature_total / float(si_counter)); si_humidity_avg = uint16_t(0.5 + si_humidity_total / float(si_counter));
    si_counter = 0; si_temperature_total = 0; si_humidity_total = 0;
    #if HW_SERIAL == 1
      USE_SERIAL.print("SI7021:"); USE_SERIAL.print(si_temperature_avg/100.0,2); USE_SERIAL.print("C "); USE_SERIAL.print(si_humidity_avg/100.0,2); USE_SERIAL.print("% ");
    #endif 
    }
  }
#endif
#if HW_SHT35 == 1
  #include "ClosedCube_SHT31D.h"
  ClosedCube_SHT31D sht35;
  int16_t sht_temperature, sht_humidity;
  int32_t sht_temperature_total = 0; uint32_t sht_humidity_total = 0;
  int16_t sht_temperature_avg = 0; uint16_t sht_humidity_avg = 0;
  boolean sht_last_error=true;
  SHT31D sht;
  #if SW_MEDIUM_FILTER == 1 // sometimes the ADC values have outliers and this is noticable through dips in the average values. Simple fix to median filter, but may require further investigation to the root cause.
    #include "MedianFilterLib.h"
    MedianFilter<int16_t> med_sht_temperature(CONF_MEDIAN_WINDOW); MedianFilter<int16_t> med_sht_humidity(CONF_MEDIAN_WINDOW);
    int16_t sht_counter = -CONF_MEDIAN_WINDOW+1; //-3+1=-2 // count only the 3rd value (when counter=0) if filtering
  #else
    int16_t sht_counter = 0;
  #endif
  
  void init_SHT35(){
    #if HW_SSD1306 == 1
      if(lcd_enable){ boot_display_SSD1306("SHT35...", false); }
    #endif
    #if HW_SERIAL == 1
      USE_SERIAL.print("Init SHT35...");
    #endif
    #if HW_REMOTE_I2C == 1
      if(!remote_i2c_online){init_remote_i2c();}
    #endif
    if(ping_i2c(SHT35_ADDRESS)){
      if( sht35.begin(SHT35_ADDRESS)==0 ){
      #if HW_SSD1306 == 1
        if(lcd_enable){ boot_display_SSD1306("OK"); }
      #endif
      #if HW_SERIAL == 1
        sht=sht35.readTempAndHumidity(SHT3XD_REPEATABILITY_HIGH, SHT3XD_MODE_POLLING, 50);
        USE_SERIAL.println("OK Serial# "+String(sht35.readSerialNumber())+" "+String(sht.rh,2)+"% "+String(sht.t,2)+"C");
      #endif
      sht_last_error=false;
      }
    }
    else {
      #if HW_SERIAL == 1
        USE_SERIAL.println("ERROR ping 0x"+String(SHT35_ADDRESS)+" init_SHT35()");
      #endif
      #if HW_SSD1306 == 1
        if(lcd_enable){ boot_display_SSD1306("ERROR"); }
      #endif
      sht_last_error=true;
    }
  }
  
  boolean poll_SHT35(boolean verb=false){
    if(ping_i2c(SHT35_ADDRESS)){
      if(sht_last_error){init_SHT35();}
      sht=sht35.readTempAndHumidity(SHT3XD_REPEATABILITY_HIGH, SHT3XD_MODE_POLLING, 50);
      #if SW_MEDIUM_FILTER == 1
        sht_humidity = med_sht_humidity.AddValue((int16_t)0.5+100*sht.rh);
        sht_temperature = med_sht_temperature.AddValue((int16_t)0.5+100*sht.t);
      #else
        sht_humidity = (int16_t)0.5+100*sht.rh; // float change to int for better performance
        sht_temperature = (int16_t)0.5+100*sht.t; // float change to int for better performance
      #endif
      if ( sht_counter >= 0 ){
        sht_temperature_total = sht_temperature_total + sht_temperature;
        sht_humidity_total = sht_humidity_total + sht_humidity;
      }
      sht_counter = sht_counter + 1;
      #if SW_DEBUG_SHT == 1 && HW_SERIAL == 1
        USE_SERIAL.println("SHT35:"+String(sht.rh)+"% "+String(sht.t)+"C total%:"+String(sht_humidity_total)+" totalC:"+String(sht_temperature_total)+" count:"+String(sht_counter));
      #endif
      si_last_error=false;
      #if HW_SERIAL == 1
        if (verb){ USE_SERIAL.print("SHT35:"); USE_SERIAL.print(sht_temperature/100.0); USE_SERIAL.print("C "); USE_SERIAL.print(sht_humidity/100.0); USE_SERIAL.print("% "); }
      #endif 
    } else {
      #if HW_SERIAL == 1
        USE_SERIAL.print("SHT35:x ");
      #endif 
      sht_last_error=true;
    }
  }
  void get_SHT35(){
    if ( sht_counter <= 0 ) { poll_SHT35(); poll_SHT35(); poll_SHT35(); }
    if ( sht_counter <= 0 ) { sht_temperature_avg = 32767; sht_humidity_avg=32767; } else {
    sht_temperature_avg = int16_t(0.5+sht_temperature_total / float(sht_counter)); sht_humidity_avg = uint16_t(0.5 + sht_humidity_total / float(sht_counter));
    sht_counter = 0; sht_temperature_total = 0; sht_humidity_total = 0;
    #if HW_SERIAL == 1
      USE_SERIAL.print("SHT35:"); USE_SERIAL.print(sht_temperature_avg/100.0,2); USE_SERIAL.print("C "); USE_SERIAL.print(sht_humidity_avg/100.0,2); USE_SERIAL.print("% ");
    #endif 
    }
  }
#endif //HW_SHT35

#if HW_HDC1080 == 1
  #include "ClosedCube_HDC1080.h"
  ClosedCube_HDC1080 hdc1080;
  int16_t hdc_temperature; uint16_t hdc_humidity;
  int32_t hdc_temperature_total = 0; uint32_t hdc_humidity_total = 0;
  int16_t hdc_temperature_avg = 0; uint16_t hdc_humidity_avg = 0;
  boolean hdc_last_error=true;
  #if SW_MEDIUM_FILTER == 1 // sometimes the sensor values have outliers and this is noticable through dips in the average values. Simple fix to median filter, but may require further investigation to the root cause.
    #include "MedianFilterLib.h"
    MedianFilter<int16_t> med_hdc_temperature(CONF_MEDIAN_WINDOW); MedianFilter<uint16_t> med_hdc_humidity(CONF_MEDIAN_WINDOW);
    int16_t hdc_counter = -CONF_MEDIAN_WINDOW+1; //-3+1=-2 // count only the 3rd value (when counter=0) if filtering
  #else
    int16_t hdc_counter = 0;
  #endif

  void init_HDC1080(){
    #if HW_SSD1306 == 1
      if(lcd_enable){ boot_display_SSD1306("HDC1080...", false); }
    #endif
    #if HW_SERIAL == 1
      USE_SERIAL.print("Init HDC1080..");
    #endif
    #if HW_REMOTE_I2C == 1
      if(!remote_i2c_online){init_remote_i2c();}
    #endif
    if(ping_i2c(HDC1080_ADDRESS)){
      hdc1080.begin(HDC1080_ADDRESS);
      #if HW_SERIAL == 1
        USE_SERIAL.println("OK");
      #endif
      #if HW_SSD1306 == 1
        if(lcd_enable){ boot_display_SSD1306("OK"); }
      #endif
      hdc_last_error=false;
      }
      else {
        #if HW_SERIAL == 1
          USE_SERIAL.println("ERROR ping 0x"+String(HDC1080_ADDRESS,HEX)+" init_HDC1080()");
        #endif
        #if HW_SSD1306 == 1
          if(lcd_enable){ boot_display_SSD1306("ERROR 0x"+String(HDC1080_ADDRESS,HEX)); }
        #endif
        hdc_last_error=true;
      }
    }
  void poll_HDC1080(){
    if( (ping_i2c(HDC1080_ADDRESS)) ) {
      if( hdc_last_error ){init_HDC1080();}
      if ( hdc1080.readDeviceId()==0x1050 ) {  //identifies HDC1080 sensor
      #if SW_MEDIUM_FILTER == 1
        hdc_temperature = med_hdc_temperature.AddValue((int16_t)100*hdc1080.readTemperature());
        hdc_humidity = med_hdc_humidity.AddValue((uint16_t)100*hdc1080.readHumidity());
      #else
        hdc_temperature = (int16_t)100*hdc1080.readTemperature(); // float change to int for better performance
        hdc_humidity = (uint16_t)100*hdc1080.readHumidity(); // float change to int for better performance
      #endif
      if ( (hdc_temperature < 8900) && (hdc_humidity <= 10000) ){ //temperature is less than 89C and humidiy is less than or equal to 100%
        if ( hdc_counter >= 0 ) {  // catch first counts when median filter is not yet full
          hdc_temperature_total = hdc_temperature_total + hdc_temperature;
          hdc_humidity_total = hdc_humidity_total + hdc_humidity;
        }
        hdc_counter = hdc_counter + 1;
        hdc_last_error=false;
      } else { hdc_last_error=true; }
      #if HW_SERIAL == 1
        USE_SERIAL.print("HDC1080:"); USE_SERIAL.print((float)hdc_temperature/100); USE_SERIAL.print("C "); USE_SERIAL.print((float)hdc_humidity/100); USE_SERIAL.print("% ");
      #endif 
    } else {
      USE_SERIAL.print("HDC1080:x ");
      hdc_last_error=true;
    }
    }
  }
  void get_HDC1080(){
    if ( hdc_counter <= 0 ) { poll_HDC1080(); poll_HDC1080(); poll_HDC1080(); }
    if ( hdc_counter <= 0 ) { hdc_temperature_avg = 32767; hdc_humidity_avg=32767; } else {
      hdc_temperature_avg = int16_t(0.5+hdc_temperature_total / float(hdc_counter)); hdc_humidity_avg = uint16_t(0.5 + hdc_humidity_total / float(hdc_counter));
      hdc_counter = 0; hdc_temperature_total = 0; hdc_humidity_total = 0;
      #if HW_SERIAL == 1
        USE_SERIAL.print("HDC1080:"); USE_SERIAL.print(hdc_temperature_avg/100.0,2); USE_SERIAL.print("C "); USE_SERIAL.print(hdc_humidity_avg/100.0,2); USE_SERIAL.print("% ");
      #endif 
    }
  }
#endif
#if HW_ENCODER == 1
  #if defined(ESP32)
    #include "AiEsp32RotaryEncoder.h"
    #include "Arduino.h"
    #define ROTARY_ENCODER_A_PIN ENCODER_PIN_A
    #define ROTARY_ENCODER_B_PIN ENCODER_PIN_B
    #define ROTARY_ENCODER_BUTTON_PIN ENCODER_PIN_C
    #define ROTARY_ENCODER_VCC_PIN -1 /*put -1 of Rotary encoder Vcc is connected directly to 3,3V; else you can use declared output pin for powering rotary encoder */
    AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, ROTARY_ENCODER_VCC_PIN);
    int test_limits = 2;
    long oldPosition  = 0;
    long newPosition = 0;
    boolean push_button=false, last_push_button=false;
    uint8_t encoder_sequence=1;
    uint32_t unix_time_start=0, unix_time_end=0, unix_time_mod=0, unix_time_pub=0;
    
    void rotary_onButtonClick() {
      //rotaryEncoder.reset();
      //rotaryEncoder.disable();
      rotaryEncoder.setBoundaries(-test_limits, test_limits, false);
      test_limits *= 2;
      portENTER_CRITICAL_ISR(&mux); 
        last_lcd_enable=lcd_enable;
      portEXIT_CRITICAL_ISR(&mux); 
      lcd_enable=!lcd_enable; //toggles the LCD
      //USE_SERIAL.print("mode_interactive:"); USE_SERIAL.println(mode_interactive);
      mode_interactive=true;
      push_button=!push_button;
      last_screensaver_activity=loopMillis; // reset screenscaver timout
      USE_SERIAL.println("ESP32 Click");
    }
    void rotary_loop() {
      //first lets handle rotary encoder button click
      if (rotaryEncoder.currentButtonState() == BUT_RELEASED) { //Inverse logic with the schmitt trigger inverting chip
        rotary_onButtonClick();
      }
      #if SW_TOGGLE_DS_INT == 1
      if (last_push_button != push_button){
        encoder_sequence=(encoder_sequence+1)%3;
        if (encoder_sequence==0){
          disableRTCalarm(getRTCEpoch()-1770);
        } else if(encoder_sequence==1){
          setAlarmEpoch(unix_time_mod+modulus);
        } else if (encoder_sequence==2){
          setAlarmS(unix_time_mod+modulus);
        }
      last_push_button=push_button;
      }
      #endif

      //lets see if anything changed
      int16_t encoderDelta = rotaryEncoder.encoderChanged();  //optionally we can ignore whenever there is no change
      if (encoderDelta == 0) return;
      
      //for some cases we only want to know if value is increased or decreased (typically for menu items)
      //if (encoderDelta>0) USE_SERIAL.print("+"); if (encoderDelta<0) USE_SERIAL.print("-");
      //for other cases we want to know what is current value. Additionally often we only want if something changed
      //example: when using rotary encoder to set termostat temperature, or sound volume etc
      //if value is changed compared to our last read
      if (encoderDelta!=0) { //now we need current value
        //int16_t encoderValue = 
        //USE_SERIAL.print("Value: "); USE_SERIAL.println(encoderValue);
        newPosition = rotaryEncoder.readEncoder();;
        oldPosition = newPosition;
        last_screensaver_activity=loopMillis; // reset screenscaver timout
        #if HW_SSD1306 == 1
          update_SSD1306();
        #endif
        //USE_SERIAL.println(newPosition);
        lcd_enable=true; //start the lcd on encoder change
      }  
    }
    void init_ENCODER(){
      USE_SERIAL.print("Init ENCODER..");  // no serial available at this stage
      pinMode(ROTARY_ENCODER_A_PIN, INPUT_PULLUP);
      pinMode(ROTARY_ENCODER_B_PIN, INPUT_PULLUP); 
      pinMode(ROTARY_ENCODER_BUTTON_PIN, INPUT_PULLUP); 
      rotaryEncoder.begin();
      rotaryEncoder.setup([]{rotaryEncoder.readEncoder_ISR();});
      //optionally we can set boundaries and if values should cycle or not
      rotaryEncoder.setBoundaries(0, 10, true); //minValue, maxValue, cycle values (when max go to min and vice versa)
      rotaryEncoder.enable();
      USE_SERIAL.println("OK Pin A/B/C:"+String(ROTARY_ENCODER_A_PIN)+" "+String(ROTARY_ENCODER_B_PIN)+" "+String(ROTARY_ENCODER_BUTTON_PIN));
    }
    void call_ENCODER(){
      rotary_loop(); // for program consistency, quick hack
    }
  #else
    #include <Encoder.h>
    Encoder myEnc(ENCODER_PIN_A, ENCODER_PIN_B); //ping 4 and 6
    long oldPosition  = 0;
    long newPosition = 0;

    void init_ENCODER(){
        //USE_SERIAL.println("Init Encoder...");
        pinMode(ENCODER_PIN_A, INPUT_PULLUP);
        pinMode(ENCODER_PIN_B, INPUT_PULLUP); 
        pinMode(ENCODER_PIN_C, INPUT_PULLUP); 
        attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_C),click,FALLING); // Pin 7
    }
   IRAM_ATTR void click(){
      portENTER_CRITICAL_ISR(&mux);
        last_lcd_enable=lcd_enable;
        lcd_enable=!lcd_enable;
      //USE_SERIAL.print("State: ");
      //USE_SERIAL.print(digitalRead(ENCODER_PIN_A)); USE_SERIAL.print(" "); USE_SERIAL.print(digitalRead(ENCODER_PIN_B)); USE_SERIAL.print(" "); USE_SERIAL.println(digitalRead(ENCODER_PIN_C)); 
      portEXIT_CRITICAL_ISR(&mux);
    }
    void call_ENCODER(){
      newPosition = myEnc.read();
      if (newPosition != oldPosition) {
        oldPosition = newPosition;
        last_screensaver_activity=loopMillis; // reset screenscaver timout
        USE_SERIAL.println(newPosition);
        portENTER_CRITICAL_ISR(&mux); 
          lcd_enable=true;
        portEXIT_CRITICAL_ISR(&mux); 
        }
    }  
  #endif
#endif

#if HW_BH1750 == 1
  #include <BH1750.h>
  BH1750 lightMeter(BH1750_ADDRESS);
  uint16_t lux, lux_avg;
  uint32_t lux_total = 0;
  boolean bh_last_error=true;
  #if SW_MEDIUM_FILTER == 1 // sometimes the ADC values have outliers and this is noticable through dips in the average values. Simple fix to median filter, but may require further investigation to the root cause.
    #include "MedianFilterLib.h"
    MedianFilter<int16_t> med_lux(CONF_MEDIAN_WINDOW);
    int16_t lux_counter = -CONF_MEDIAN_WINDOW+1; //-3+1=-2 // count only the 3rd value (when counter=0) if filtering
  #else
    int16_t lux_counter = 0;
  #endif
  
  boolean init_BH1750(){
    #if HW_SSD1306 == 1
      if(lcd_enable){ boot_display_SSD1306("BH1750...", false); }
    #endif
    #if HW_SERIAL == 1
      USE_SERIAL.print("Init BH1750...");
    #endif
    #if HW_REMOTE_I2C == 1
      if(!remote_i2c_online){init_remote_i2c();}
    #endif
    if(ping_i2c(BH1750_ADDRESS)){
      lightMeter.begin(BH1750_CONTINUOUS_LOW_RES_MODE); //also BH1750_CONTINUOUS_HIGH_RES_MODE
      bh_last_error=false;
      #if HW_SSD1306 == 1
        if(lcd_enable){ boot_display_SSD1306("OK"); }
      #endif
      #if HW_SERIAL == 1
         USE_SERIAL.println("OK "+String(lightMeter.readLightLevel())+"lx");
      #endif
      return true;
    } else {
    #if HW_SERIAL == 1
      USE_SERIAL.println("ERROR ping 0x"+String(BH1750_ADDRESS,HEX)+" init_BH1750()"); 
    #endif
    #if HW_SSD1306 == 1
      if(lcd_enable){ boot_display_SSD1306("ERROR 0x"+String(BH1750_ADDRESS,HEX)); }
    #endif
      bh_last_error=true;
      return false;
    }
  }
  void poll_BH1750(){
    if(ping_i2c(BH1750_ADDRESS)){
      if(bh_last_error){init_BH1750();}
      #if SW_MEDIUM_FILTER == 1
        lux = med_lux.AddValue(lightMeter.readLightLevel());
      #else
        lux = lightMeter.readLightLevel(); //uint16_t      
      #endif
      if ( lux < 54612){  // upper limit through the filter
        // Only use the 3rd median value (counter starts at -1); probably no further filtering needed.
        if (lux_counter >= 0 ) { lux_total = lux_total + lux;}
        lux_counter = lux_counter + 1;
        bh_last_error=false;
      } else { bh_last_error=true; }
    #if HW_SERIAL == 1
      USE_SERIAL.print("BH1750:"); USE_SERIAL.print(lux); USE_SERIAL.print("lx ");
    #endif
    }
    else {
      USE_SERIAL.print("BH1750:x ");
      bh_last_error=true;
    }
  }
  void get_BH1750(){
    if ( lux_counter <= 0 ) { poll_BH1750(); poll_BH1750(); poll_BH1750();}
    if ( lux_counter <= 0 ) {lux_avg = 65535; } else {
      lux_avg = uint16_t(0.5 + lux_total / float(lux_counter));
      lux_counter = 0; lux_total = 0;
    #if HW_SERIAL == 1
      USE_SERIAL.print("BH1750:"); USE_SERIAL.print(lux_avg); USE_SERIAL.print("lx ");
    #endif
    }
  }
#endif
uint32_t lastPeriodPublishTimestamp=0;

boolean check_publish_data(uint32_t timestamp){
  /* 1) If lastPeriodPublishTimestamp is excessively high, this is caught in the initialization and it is reset to the current unix_pub, so we can assume that it is a sane value by the time it reaches here
   * 2) timestamp is normally a unix_mod or a unix_pub
   * 3) check that no more than one data set is published every periodPublish, ensure that timestamp - lastPeriodPublishTimestamp >= periodPublish/1000
   * 4) The "publish first" is set elsewhere and not via this function, so it would be safe to wait 
   */
  if ( ( (timestamp)%(periodPublish/1000) == 0) )  {  // in case lastPeriodPublishTimestamp is excessivily high only catch the first period.
    if ( ( (lastPeriodPublishTimestamp + periodPublish/1000) <= timestamp ) /*|| (loopMillis > periodPublish) */ ){
      if (interactive_mode){collect_sample=false;} else {collect_sample=true;}
      return true; /*publish data*/  //this matches multiples of alarm times with publishPeriod and triggers the publish_data flag.
    } // only collect more samples in the main loop in non-interactive mode as it is quick. In interactive mode dump the data straight to the server in the main loop
  }
  return false;
}

#if defined (ESP32)
  uint32_t getESPEpoch(){
    return uint32_t(time(nullptr));
  }
#endif

#if HW_uRTClib == 1
  #include "Arduino.h"
  #include <Wire.h>
  #include "uRTCLib.h"
  //#include <Time.h>   
  uRTCLib rtc; //external DS3232 or DS3231
  struct tm now2, t;
  String date_str = ""; String time_str = ""; String datestring = "";
  int16_t ds_temperature=0, ds_temperature_avg=0;
  int32_t ds_temperature_total = 0;
  volatile boolean alarm_set=false;
  boolean ds_is_available=false;
  int32_t adjust_ms=0;
  #if SW_MEDIUM_FILTER == 1 // sometimes the ADC values have outliers and this is noticable through dips in the average values. Simple fix to median filter, but may require further investigation to the root cause.
    #include "MedianFilterLib.h"
    MedianFilter<int16_t> med_ds_temperature(CONF_MEDIAN_WINDOW);
    int16_t ds_counter = -CONF_MEDIAN_WINDOW+1; //-3+1=-2 // count only when filter is full (when counter=0)
  #else
    int16_t ds_counter = 0;
  #endif

  #if SW_TOGGLE_DS_INT == 1
    void disableRTCalarm(uint32_t epoch){
      struct tm t;
      time_t tt = epoch;
      gmtime_r(&tt, &t);
      USE_SERIAL.print("Alarm FIXED_MS: "+String(epoch)+" "+formatEpoch(epoch));
      rtc.alarmSet(URTCLIB_ALARM_TYPE_1_FIXED_MS, t.tm_sec, t.tm_min, t.tm_hour, t.tm_mday);
      rtc.alarmSet(URTCLIB_ALARM_TYPE_2_FIXED_M, t.tm_sec, t.tm_min, t.tm_hour, t.tm_mday);
      USE_SERIAL.print(" AL1: b"+String(URTCLIB_ALARM_TYPE_1_FIXED_MS,BIN));
      USE_SERIAL.println(" AL2: b"+String(URTCLIB_ALARM_TYPE_2_FIXED_M,BIN));    
    }
    void setAlarmS(uint32_t epoch){
      struct tm t;
      time_t tt = epoch;
      gmtime_r(&tt, &t);
      USE_SERIAL.print("Alarm Second:"+String(epoch)+" "+formatEpoch(epoch));
      rtc.alarmSet(URTCLIB_ALARM_TYPE_1_ALL_S, t.tm_sec, t.tm_min, t.tm_hour, t.tm_mday);
      rtc.alarmSet(URTCLIB_ALARM_TYPE_2_ALL_M, t.tm_sec, t.tm_min, t.tm_hour, t.tm_mday);
      USE_SERIAL.print(" AL1: b"+String(URTCLIB_ALARM_TYPE_1_FIXED_MS,BIN));
      USE_SERIAL.println(" AL2: b"+String(URTCLIB_ALARM_TYPE_2_FIXED_M,BIN));    
    }

  #endif
    
  void setAlarmEpoch(uint32_t epoch){
    struct tm t;
    time_t tt = epoch;
    gmtime_r(&tt, &t);
    USE_SERIAL.print("Init Alarm....OK At:"+String(epoch)+" "+formatEpoch(epoch));
    rtc.alarmClearFlag(URTCLIB_ALARM_1);
    rtc.alarmClearFlag(URTCLIB_ALARM_2);
    #if CONF_MODULUS_S == 60
      rtc.alarmSet(URTCLIB_ALARM_TYPE_1_FIXED_S, t.tm_sec, t.tm_min, t.tm_hour, t.tm_mday);
      USE_SERIAL.print(" AL1: b"+String(URTCLIB_ALARM_TYPE_1_FIXED_S,BIN));
    #else
      rtc.alarmSet(URTCLIB_ALARM_TYPE_1_FIXED_MS, t.tm_sec, t.tm_min, t.tm_hour, t.tm_mday);
      USE_SERIAL.print(" AL1: b"+String(URTCLIB_ALARM_TYPE_1_FIXED_MS,BIN));
    #endif
      rtc.alarmSet(URTCLIB_ALARM_TYPE_2_ALL_M, t.tm_sec, t.tm_min, t.tm_hour, t.tm_mday);
      USE_SERIAL.print(" AL2: b"+String(URTCLIB_ALARM_TYPE_2_ALL_M,BIN));  //bits
      USE_SERIAL.print("\n");

    /* Alarm 1:
       --------
       URTCLIB_ALARM_TYPE_1_ALL_S - Every second
       URTCLIB_ALARM_TYPE_1_FIXED_S - Every minute at given second
       URTCLIB_ALARM_TYPE_1_FIXED_MS - Every hour at given Minute:Second
       URTCLIB_ALARM_TYPE_1_FIXED_HMS - Every day at given Hour:Minute:Second
       URTCLIB_ALARM_TYPE_1_FIXED_DHMS - Every month at given DAY-Hour:Minute:Second
       URTCLIB_ALARM_TYPE_1_FIXED_WHMS - Every week at given DOW + Hour:Minute:Second

       Alarm 2 (triggers always at 00 seconds):
       ----------------------------------------
       URTCLIB_ALARM_TYPE_2_ALL_M - Every minute at 00 Seconds
       URTCLIB_ALARM_TYPE_2_FIXED_M - Every hour at given Minute(:00)
       URTCLIB_ALARM_TYPE_2_FIXED_HM - Every day at given Hour:Minute(:00)
       URTCLIB_ALARM_TYPE_2_FIXED_DHM - Every month at given DAY-Hour:Minute(:00)
       URTCLIB_ALARM_TYPE_2_FIXED_WHM - Every week at given DOW + Hour:Minute(:00)
    */
  }
  void setRTCEpoch(uint32_t epoch){
    struct tm t;
    time_t tt = epoch;
    gmtime_r(&tt, &t);
    rtc.set(t.tm_sec, t.tm_min, t.tm_hour, (t.tm_wday+8)%8, t.tm_mday, t.tm_mon, t.tm_year-100); // set the DS3232
  }
  uint32_t getRTCEpoch(){
    uint32_t epoch=0;
    if(ping_i2c(DS_ADDRESS)){
      ds_is_available=true;
      struct tm t;
      rtc.refresh();
      t.tm_year=rtc.year()+100; t.tm_mon=rtc.month()-1; t.tm_mday=rtc.day(); t.tm_hour=rtc.hour(); t.tm_min=rtc.minute(); t.tm_sec=rtc.second();
      epoch=mktime(&t);
    }
    else { 
      ds_is_available=false;
      #if defined (esp32)
        epoch=getESPEpoch();
      #endif
      }
    return epoch;
  }
  void init_RTC(){
    USE_SERIAL.print("Init DS3232...");
    #if HW_SSD1306 == 1
      if(lcd_enable){ boot_display_SSD1306("uRTC...", false); }
    #endif
    pinMode(RTC_INT_PIN, INPUT_PULLUP);
    if(ping_i2c(DS_ADDRESS)){
      ds_is_available=true;
      rtc.set_rtc_address(DS_ADDRESS);
      rtc.set_model(URTCLIB_MODEL_DS3232);
      #if HW_SSD1306 == 1
        if(lcd_enable){ boot_display_SSD1306("OK"); }
      #endif
      uint32_t temp = getRTCEpoch();  //only for uRTClib
      unix_time_mod=temp-(temp%modulus); //round to nearest previous modulus
      unix_time_pub=temp-(temp%(periodPublish/1000)); //round to nearest periodPublish
      USE_SERIAL.print("OK "); 
      if (poll_RTC(false)){ USE_SERIAL.println(formatEpoch(temp)+" "+String(ds_temperature/4., 2)+"C Now:"+String(temp)+" Mod:"+String(unix_time_mod)+" Pub:"+String(unix_time_pub)+" Elapsed:"+String(temp-unix_time_pub)+"/"+String(periodPublish/1000.,0)+"s"); }
    } else {
      #if HW_SERIAL == 1
        USE_SERIAL.println("ERROR ping 0x"+String(DS_ADDRESS, HEX)+" init_RTC()");
      #endif
      #if HW_SSD1306 == 1
        if(lcd_enable){ boot_display_SSD1306("ERROR 0x"+String(DS_ADDRESS, HEX)); }
      #endif
      ds_is_available=false;
    }
    #if defined(ESP32)
      //zrtc.setEpoch(now2.unixtime()); // Set time from ESP32 NTP
    #endif
  }
  boolean init2_RTC(){
    if(ping_i2c(DS_ADDRESS)){
      ds_is_available=true;
      unix_time_mod=getRTCEpoch();
      unix_time_mod=unix_time_mod-(unix_time_mod%modulus);
      #if (HW_VERSION_MAJOR == 1) && (HW_VERSION_MINOR == 2)
        attachInterrupt(RTC_INT_PIN, alarmMatch, RISING);
      #else
        attachInterrupt(RTC_INT_PIN, alarmMatch, FALLING);
      #endif
      rtc.alarmClearFlag(URTCLIB_ALARM_1); rtc.alarmClearFlag(URTCLIB_ALARM_2);
      setAlarmEpoch(unix_time_mod+modulus);
    } else { ds_is_available=false; }
    return ds_is_available;
  }
  String formatEpoch(uint32_t epoch){
    uint8_t dd=0, mm=0, yy=0, h=0, m=0, s=0;
    time_t now = epoch;
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    dd=timeinfo.tm_mday; mm=timeinfo.tm_mon+1; yy=timeinfo.tm_year-100; h=timeinfo.tm_hour; m=timeinfo.tm_min; s=timeinfo.tm_sec;
    #if defined(ESP32)
      char dt[9]; //8 characters plus termination is 9
      sprintf(dt, "%02d/%02d/%02d", dd, mm, yy);  
      date_str = dt;   
      sprintf(dt, "%02d:%02d:%02d", h, m, s);  
      time_str = dt;
    #else    
      /* char dt[9];
      uint8_t nday=dd, nmonth=mm;
      #if defined(ARDUINO_ARCH_SAMD)
        int yy=rtc.year();
      #else
        int yy=rtc.year()-2000;
      #endif
      */
      String sday, smonth, syear;
      if ( dd < 10 ){ sday = "0" + String(dd); } else { sday  = String(dd); }
      if ( mm < 10 ){ smonth="0" + String(mm); } else { smonth= String(mm); }
      if ( yy < 10 ){ syear= "0" + String(yy); } else { syear = String(yy); }
      date_str = sday+"/"+smonth+"/"+syear;
      String shour, sminute, ssecond;
      if ( h < 10 ){ shour = "0" + String(h); } else { shour  = String(h); }
      if ( m < 10 ){ sminute="0" + String(m); } else { sminute= String(m); }
      if ( s < 10 ){ ssecond="0" + String(s); } else { ssecond= String(s); }
      time_str = shour+":"+sminute+":"+ssecond;
    #endif
    datestring = date_str + "     " + time_str;
    return date_str + " " + time_str;
  }
  boolean poll_RTC(boolean verb){
    String time_source, s_temperature;
    uint32_t temp;
    if(ping_i2c(DS_ADDRESS)){
      ds_is_available=true;
      time_source="RTC:";
      rtc.alarmClearFlag(URTCLIB_ALARM_1); rtc.alarmClearFlag(URTCLIB_ALARM_2);
      temp=getRTCEpoch();
      #if SW_MEDIUM_FILTER == 1
        ds_temperature = med_ds_temperature.AddValue((int16_t)4*rtc.temp()/100.);
      #else
        ds_temperature = (int16_t)4*rtc.temp()/100.; // 4 times the temperature
      #endif
      if ( ds_counter >= 0 ){ ds_temperature_total = ds_temperature_total + ds_temperature; }
      ds_counter = ds_counter + 1;
      s_temperature=" "+String(ds_temperature/4., 2)+"C ";
      formatEpoch(temp); // to update time_str and date_str
    } else {
      ds_is_available=false;
      #if defined(ESP32)
        time_source="ESP:";
        temp = getESPEpoch();
        s_temperature=" ";
      #else if HW_SERIAL == 1
        if (verb){ USE_SERIAL.print("RTC:x ");}
        return ds_is_available;
      #endif 
    }
      #if HW_SERIAL == 1
        #if SW_DEBUG_PUBLISH == 1
          USE_SERIAL.print("Publish: "+String(millis() - lastPublishMillis)+" >= "+String(periodPublish + adjust_ms)+" "); 
        #endif
        if (verb){ USE_SERIAL.print("[POLL] "+time_source+formatEpoch(temp)+String(s_temperature)); }
      #endif
      return ds_is_available;
  }
  boolean get_RTC(boolean verb=false){
    poll_RTC();
    String time_source, s_temperature;
    if (ds_counter > 0 && ds_is_available){
      unix_time_end=getRTCEpoch();
      //USE_SERIAL.print("RTC: "+String(unix_time_end)); 
      //ds_temperature_avg = 0;
      #if SW_DEBUG_DS == 1 && HW_SERIAL == 1
        USE_SERIAL.print("  DEBUG get_RTC() counter:"+String(ds_counter)); 
      #endif
      ds_temperature_avg = int16_t(0.5+100*(ds_temperature_total / ds_counter /4.));
      #if SW_DEBUG_DS == 1 && HW_SERIAL == 1
        USE_SERIAL.println(" Temp:"+String(ds_temperature/4.)); 
      #endif
      time_source="RTC:";
      s_temperature=" "+String(ds_temperature/4., 2)+"C ";
    }
    else if ( !ds_is_available ){
      time_source="ESP:";
      s_temperature=" ";
      ds_temperature_avg = 65000;
      unix_time_end=getESPEpoch();
    }
    else {
      unix_time_pub = 0;
      ds_temperature_avg = 65000;
    }
    ds_counter = 0; ds_temperature_total = 0;  //reset variables, except ds_temperature_avg
    unix_time_pub=unix_time_start=unix_time_end;
    unix_time_pub=unix_time_pub-(unix_time_pub%(periodPublish/1000)); //round to nearest periodPublish
    #if HW_SERIAL == 1
      USE_SERIAL.print(" [GET] "+time_source+formatEpoch(unix_time_end)+String(s_temperature)+String(unix_time_end)+" "); 
    #endif
    return ds_is_available;
}
  IRAM_ATTR void alarmMatch(){ // this function gets called at every RTC interrupt and should trigger an update to the logging server (usually via lora)
    portENTER_CRITICAL_ISR(&mux);
    #if defined (ESP32)
      //detachInterrupt(RTC_INT_PIN);
      #if SW_DS_INT_ENABLE == 1
        alarm_set=true; // disable to test millis timer ONLY
      #endif
      rtc_int_counter++;      
      #if SW_DEBUG_DS == 1 && HW_SERIAL == 1
        USE_SERIAL.println("  DEBUG RTC alarmMatch() INT!");
      #endif
    #else
      USE_SERIAL.println("Alarm Match! PROBLEM Check code ;-)");
    #endif
    portEXIT_CRITICAL_ISR(&mux);
  }
  void call_RTC(){
    if (alarm_set){
      uint32_t t_int = getRTCEpoch();  //only for uRTClib
      uint32_t unix_time_mod=t_int-(t_int%modulus); //round to nearest previous modulus
      rtc.alarmClearFlag(URTCLIB_ALARM_1); rtc.alarmClearFlag(URTCLIB_ALARM_2);
      //digitalWrite(13, !digitalRead(13)); // toggling I2C_CHIP for testing
      #if CONF_MODULUS_S != 60  //only do if not a minutely poll: is less work and more stable in case an interrupt is missed or the I2C bus has issues
        setAlarmEpoch(unix_time_mod+modulus);
      #endif // =>> unix_time_pub is not yet updated to the new value at this point although it may be due by now but it is just a display problem
      USE_SERIAL.println("INFO call_RTC() Alarm Match! Now:"+String(t_int)+" Mod:"+String(unix_time_mod)+" Pub:"+String(unix_time_pub)+" Next:"+String(unix_time_mod+modulus)+" "+String(t_int-unix_time_mod+modulus)+"s");
      //adjust_time();
      #if (HW_VERSION_MAJOR == 1) && (HW_VERSION_MINOR == 2)
        //attachInterrupt(RTC_INT_PIN, alarmMatch, RISING);
      #else
        //attachInterrupt(RTC_INT_PIN, alarmMatch, FALLING);
      #endif
      portENTER_CRITICAL_ISR(&mux);
      alarm_set=false;
      portEXIT_CRITICAL_ISR(&mux);
      if( ((millis() - lastPublishMillis) >= (periodPublish/5)) || lastPublishMillis < periodPublish ){ // rate limit, but only after periodPublish from the start
        publish_data=check_publish_data(unix_time_mod);
      }
        #if SW_DEBUG_DS == 1 && HW_SERIAL == 1
          USE_SERIAL.println("  DEBUG call_RTC() unix_time_mod: "+String(unix_time_mod)+" periodPublish: "+String(periodPublish)+" publish_data: "+String(publish_data)); 
        #endif
      }
    }
#endif
#if HW_RTClib == 1
  #include <Wire.h>
  #include "RTClib.h"
  //#include <TimeLib.h>
  RTC_DS3231 rtc; //external DS3231
DateTime now2; DateTime t;
  String date_str = ""; String time_str = ""; String datestring = "";
  int16_t ds_temperature=0;
  int32_t ds_temperature_total = 0;
  uint16_t ds_counter = 0;
  int16_t ds_temperature_avg=0;
  uint32_t unix_time_mod=0, unix_time_start=0, unix_time_end=0;

  void init_RTC(){
    #if defined(ARDUINO_ARCH_SAMD)
      USE_SERIAL.println("Init RTCzero...");
      zrtc.begin(); // initialize RTC
    #endif
    USE_SERIAL.println("Init RTClib DS3231...");
    if (!rtc.begin()) { USE_SERIAL.println("Couldn't find RTC - FATAL STOPPING"); while (1); }
    if (rtc.lostPower()) {
      //USE_SERIAL.println("RTC lost power, lets set the time!"); // following line sets the RTC to the date & time this sketch was compiled
      //USE_SERIAL.println("RTC Lost power - FATAL STOPPING");
      //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
      USE_SERIAL.println("RTC LOST POWER!");
    }
    now2 = rtc.now(); //single time poll avoids inadvertent rollover during polling
    #if defined(ARDUINO_ARCH_SAMD)
      zrtc.setEpoch(now2.unixtime()); // Set time from DS3231
    #endif
    
    unix_time_start=now2.unixtime()-7200;    
    uint8_t offset=now2.unixtime()%modulus;
    #if defined(ARDUINO_ARCH_SAMD)
      if( offset < (modulus-2) ){ zrtc.setAlarmEpoch(now2.unixtime()- offset + modulus);} else { zrtc.setAlarmEpoch(now2.unixtime() - offset + 2*modulus);} // call update if close to cycle time (first if).
      zrtc.enableAlarm(zrtc.MATCH_MMSS); //MATCH_HHMMSS
      zrtc.attachInterrupt(alarmMatch);
    #endif
  }

  void poll_RTC(boolean verb=false){
    #if defined(ARDUINO_ARCH_SAMD)
      char dt[8];
      t = zrtc.getEpoch();
      sprintf(dt, "%02d/%02d/%02d", t.day(), t.month(), t.year()-2000);  
      date_str = dt;   //(String)(t.year())+"/"+(String)(t.month())+"/"+(String)(t.day());
      sprintf(dt, "%02d:%02d:%02d", t.hour(), t.minute(), t.second());  
      time_str = dt; //(String)(t.hour())+"/"+(String)(t.minute())+"/"+(String)(t.second());
      datestring = date_str + "     " + time_str;
    #else
    
      now2 = rtc.now(); //single time poll avoids inadvertent rollover during polling
      char dt[8];
      int nday=now2.day(), nmonth=now2.month(), nyear=now2.year()-2000;
      String sday=String(nday), smonth=String(nmonth), syear=String(nyear);
      if (now2.day()<10){sday="0"+String(nday);} if (now2.month()<10){smonth="0"+String(nmonth);} if (now2.year()<10){syear="0"+String(nyear);}
      //sprintf(dt, "%02d/%02d/%02d", nday, nmonth , nyear);
      //USE_SERIAL.print(nday); USE_SERIAL.print("/"); USE_SERIAL.print(nmonth); USE_SERIAL.print("/"); USE_SERIAL.println(nyear);
      
      date_str = sday+"/"+smonth+"/"+syear; // might be nice to have 2 character numbers here...
      int nhour=now2.hour(), nminute=now2.minute(), nsecond=now2.second();
      String shour=String(nhour), sminute=String(nminute), ssecond=String(nsecond);
      //sprintf(dt, "%02d:%02d:%02d", now2.hour(), now2.minute(), now2.second());  
      if (now2.hour()<10){shour="0"+String(nhour);} if (now2.minute()<10){sminute="0"+String(nminute);} if (now2.second()<10){ssecond="0"+String(nsecond);}
      time_str = shour+":"+sminute+":"+ssecond; // sprintf does not work properly ESP32
      //sprintf(dt, "%02d/%02d/%02d     %02d:%02d:%02d", now2.day(), now2.month(), now2.year()-2000, now2.hour(), now2.minute(), now2.second());
      datestring = date_str + "     " + time_str;
    #endif
      ds_temperature = rtc.temperature(); // 4 times the temperature
      ds_temperature_total = ds_temperature_total + ds_temperature;
      ds_counter = ds_counter + 1;
      #if HW_SERIAL == 1
        if (verb){ USE_SERIAL.print("RTC: "); USE_SERIAL.print(ds_temperature/4.); USE_SERIAL.print("C "); USE_SERIAL.print(date_str); USE_SERIAL.print(" "); USE_SERIAL.print(time_str); USE_SERIAL.print(" ");}
      #endif
  }
  void get_RTC(){
    if (ds_counter <= 0){ poll_RTC(); poll_RTC(); poll_RTC();}
    unix_time_end=now2.unixtime()-7200;
    unix_time_mod=unix_time_start+((unix_time_end-unix_time_start)/2);
    unix_time_mod=unix_time_mod-(unix_time_mod%modulus); //round to nearest 5 minutes
    unix_time_start=unix_time_end;
    ds_temperature_avg = 0;
    ds_temperature_avg = int16_t(0.5 + 100*(ds_temperature_total / ds_counter /4.0));
    ds_counter = 0; ds_temperature_total = 0;
    #if HW_SERIAL == 1
      USE_SERIAL.print("RTC: "); USE_SERIAL.print(ds_temperature_avg/100.); USE_SERIAL.print("C "); USE_SERIAL.print(date_str); USE_SERIAL.print(" "); USE_SERIAL.print(time_str); USE_SERIAL.print(" ");
    #endif
  }
  void alarmMatch(){ // this function gets called every modulus seconds and should trigger an update to the logging server (usually via lora)
  #if defined(ARDUINO_ARCH_SAMD)
    zrtc.detachInterrupt();
    DateTime t_int = zrtc.getEpoch()-1;
    uint32_t unix_time_mod=t_int.unixtime()-(t_int.unixtime()%modulus); //round to nearest 120s
    zrtc.disableAlarm();
    zrtc.setAlarmEpoch(unix_time_mod+modulus);
    zrtc.enableAlarm(zrtc.MATCH_MMSS); //MATCH_HHMMSS
    zrtc.attachInterrupt(alarmMatch);
    USE_SERIAL.print("Alarm Match INT! "); USE_SERIAL.print(t_int.unixtime()); USE_SERIAL.print(" "); 
    char dt[8];
    sprintf(dt, "%02d/%02d/%02d", t_int.day(), t_int.month(), t_int.year()-2000);  
    USE_SERIAL.print(dt); USE_SERIAL.print(" "); 
    sprintf(dt, "%02d:%02d:%02d", t_int.hour(), t_int.minute(), t_int.second());  
    USE_SERIAL.println(dt);
  #endif
    USE_SERIAL.print("Alarm Match INT! "); USE_SERIAL.print(" "); 
}
#endif //RTClib
#if HW_BMP280 == 1
  #include <Adafruit_Sensor.h>
  #include <Adafruit_BMP280.h>
  Adafruit_BMP280 bme; // I2C
  int16_t bmp_temperature, bmp_temperature_avg=0;
  uint32_t bmp_pressure=0, bmp_pressure_total = 0, bmp_temperature_total = 0;
  uint16_t bmp_pressure_avg=0; 
  boolean bmp_last_error=true;
  #if SW_MEDIUM_FILTER == 1 // sometimes the BMP280 values have outliers and this is noticable through dips in the average values. Simple fix to median filter, but may require further investigation to the root cause.
    #include "MedianFilterLib.h"
    MedianFilter<uint32_t> med_bmp_pressure(CONF_MEDIAN_WINDOW); MedianFilter<int16_t> med_bmp_temperature(CONF_MEDIAN_WINDOW);
    int16_t bmp_counter = -CONF_MEDIAN_WINDOW+1; //-3+1=-2 // count only the 3rd value (when counter=0) if filtering
  #else
    int16_t bmp_counter = 0;
  #endif

  boolean init_BMP280(){
    #if HW_SSD1306 == 1
      if(lcd_enable){ boot_display_SSD1306("BMP280...", false); }
    #endif
    #if HW_SERIAL == 1
      USE_SERIAL.print("Init BMP280...");
    #endif
    #if HW_REMOTE_I2C == 1
      if(!remote_i2c_online){init_remote_i2c();}
    #endif
    if(ping_i2c(0x76)){
      #if HW_SERIAL == 1 && SW_DEBUG_BMP280 == 1
        USE_SERIAL.print("  DEBUG Ping 0x"+String(0x76, HEX)+" OK init_BMP280()");
      #endif
      bmp_last_error=false;
      if (!bme.begin(0x76)) {
      #if HW_SERIAL == 1
        USE_SERIAL.println("ERROR begin() 0x"+String(0x76, HEX)+" init_BMP280()");
      #endif
      bmp_last_error = true;
      }
        #if HW_SSD1306 == 1
          if(lcd_enable){ boot_display_SSD1306("OK"); }
        #endif
        #if HW_SERIAL == 1
          USE_SERIAL.print("OK "); USE_SERIAL.print(bme.readPressure()/100.,2); USE_SERIAL.print("hPa "); USE_SERIAL.print(bme.readTemperature(),2); USE_SERIAL.println("C");
        #endif
        return true;
      }
    else {
      #if HW_SERIAL == 1
        USE_SERIAL.println("ERROR ping 0x"+String(0x76, HEX)+" init_BMP280()");
      #endif
      #if HW_SSD1306 == 1
        if(lcd_enable){ boot_display_SSD1306("ERROR 0x"+String(0x76, HEX)); }
      #endif
      bmp_last_error = true;
      return false;
    }
  }
  boolean poll_BMP280(boolean verb){  // Barometer & Thermometer BMP280
    if(ping_i2c(0x76)){ // pressure and temperature reach a sane value after 35ms after bme.begin(). Temperature takes about 1000ms to reach a stable value (heating effect?)
      if (bmp_last_error){init_BMP280();}
      #if SW_MEDIUM_FILTER == 1
        bmp_pressure = med_bmp_pressure.AddValue((uint32_t)0.5+bme.readPressure());
        bmp_temperature = med_bmp_temperature.AddValue((int16_t)0.5+100.*bme.readTemperature());
      #else
        bmp_pressure=(uint32_t)0.5+bme.readPressure(); // in Pa; read pressure first as it stabilizes to a sane value faster than temperature
        bmp_temperature=(int16_t)0.5+100.*bme.readTemperature();  //round to 2 decimal places
      #endif
      if ( (bmp_temperature < 8900) && (bmp_pressure < 110000) && (bmp_pressure > 74900) ){
        if ( bmp_counter >= 0 ) {  // need to wait until the median filter has sane values
          bmp_temperature_total = bmp_temperature_total + bmp_temperature;
          bmp_pressure_total = bmp_pressure_total + bmp_pressure;
        }
        bmp_counter = bmp_counter + 1;
        bmp_last_error=false;} else { bmp_last_error = true; }
      #if HW_SERIAL == 1
        if (verb) {USE_SERIAL.print("BMP280:"); USE_SERIAL.print(bmp_temperature/100.); USE_SERIAL.print("C "); USE_SERIAL.print(bmp_pressure/100.); USE_SERIAL.print("hPa ");}
      #endif
      return true;
    } else {
      #if HW_SERIAL == 1
        if (verb) { USE_SERIAL.print("BMP280:x "); }
      #endif
      bmp_last_error=true;
      return false;
    }
  }
  void get_BMP280(){
    if ( bmp_counter <= 0 ) { poll_BMP280(true); poll_BMP280(true); poll_BMP280(true);}
    if ( bmp_counter <= 0 ) { bmp_temperature_avg = 32767; bmp_pressure_avg=65535; }
    else {
      bmp_temperature_avg = int16_t(0.5 + bmp_temperature_total / (float)bmp_counter);
      bmp_pressure_avg = uint16_t(0.5 + (bmp_pressure_total / (float)bmp_counter) - 60000); //in Pa less 60000 (to fit into int16).
      bmp_counter = 0; bmp_pressure_total = 0; bmp_temperature_total = 0;
      #if HW_SERIAL == 1
        USE_SERIAL.print("BMP280:"); USE_SERIAL.print(bmp_temperature_avg/100.); USE_SERIAL.print("C "); USE_SERIAL.print(bmp_pressure_avg/100.+600); USE_SERIAL.print("hPa ");
      #endif  
    }
  }
#endif

#if HW_BMP180 == 1
  #include <Wire.h>
  #include <Adafruit_BMP085.h>
  Adafruit_BMP085 bmp;
  int16_t bmp_temperature; uint32_t bmp_pressure=0;
  uint32_t bmp_pressure_total = 0, bmp_temperature_total = 0; uint16_t bmp_pressure_avg=0;
  uint16_t bmp_counter = 0, bmp_temperature_avg=0;

  void poll_BMP180(){  // Barometer & Thermometer BMP280
    bmp_temperature=(uint16_t)0.5+100.*bmp.readTemperature();
    bmp_pressure=(uint32_t)bmp.readPressure(); // in Pa
    bmp_temperature_total = bmp_temperature_total + bmp_temperature;
    bmp_pressure_total = bmp_pressure_total + bmp_pressure;
    bmp_counter = bmp_counter + 1;
      #if HW_SERIAL == 1
        USE_SERIAL.print("BMP180:"); USE_SERIAL.print(bmp_temperature/100.); USE_SERIAL.print("C "); USE_SERIAL.print(bmp_pressure/100.); USE_SERIAL.print("hPa ");
      #endif  
  }
  void init_BMP180(){
    if (!bmp.begin()) {
    #if HW_SERIAL == 1
      USE_SERIAL.println("Could not find a valid BMP180 sensor, check wiring!");
    #endif
    while (1); }
      #if HW_SERIAL == 1
        USE_SERIAL.println("BMP180 Init Success");
      #endif
  }
  void get_BMP180(){
    if ( bmp_counter <= 0 ) { poll_BMP180(); poll_BMP180(); poll_BMP180();}
    bmp_temperature_avg = bmp_temperature_total / bmp_counter / 10;
    bmp_pressure_avg = (uint16_t)(0.5 + (bmp_pressure_total / bmp_counter)-60000  ); //in Pa less 60000 (to fit into int16).
    bmp_counter = 0; bmp_pressure_total = 0; bmp_temperature_total = 0;
      #if HW_SERIAL == 1
        USE_SERIAL.print("BMP180:"); USE_SERIAL.print(bmp_temperature_avg/10.); USE_SERIAL.print("C "); USE_SERIAL.print(bmp_pressure_avg/100.+600); USE_SERIAL.print("hPa ");
      #endif  
  }
#endif
#if HW_SDCARD == 1
  boolean sd_card_available = false;
  void init_sd(){
    USE_SERIAL.print("SD Card init...");
      #if HW_SSD1306 == 1
        if(lcd_enable){ boot_display_SSD1306("SD Card...", false); }
      #endif
      if (!SD.begin(SD_CS)) { USE_SERIAL.println("ERROR"); sd_card_available = false; 
      #if HW_SSD1306 == 1
        if(lcd_enable){ boot_display_SSD1306("ERROR", true);}
      #endif
      }
      else { sd_card_available = true; USE_SERIAL.println("OK"); 
      #if HW_SSD1306 == 1
        if(lcd_enable){ boot_display_SSD1306("OK", true);}
      #endif
      }
  }
  File myFile;
#endif

#if HW_SERIAL == 1
  void clean_SERIAL(){
    USE_SERIAL.println("");
  }
#endif

#if HW_SSD1306 == 1
  void update_SSD1306(){
    #if SW_DEBUG_SSD1306 == 1
      USE_SERIAL.println("");
      USE_SERIAL.print("      SSD : 1");
    #endif
    lastLCDMillis = loopMillis;
    display.clearDisplay();
    display.setTextSize(1); // Draw 2X-scale text
    display.setTextColor(WHITE);
    #if SW_DEBUG_I2C == 1
      USE_SERIAL.println("I2C Clock: "+ String(Wire.getClock()));
    #endif
    #if SW_DEBUG_SSD1306 == 1
      USE_SERIAL.print(" SSD : 2");
    #endif
    #if HW_ENCODER == 1
      //  USE_SERIAL.println(position);
      display.setCursor(0, 8);
      display.print("Rec# "+String(buffer_elements())); //display.println(millis());
    #endif
    #if SW_DEBUG_SSD1306 == 1
      USE_SERIAL.print(" SSD : 3");
    #endif
    #if HW_RAIN_INT == 1
      display.setCursor(54, 8);
      display.print("Rain: "+String(rain_counter)); //display.println(millis());
    #else
    #endif
    // Send current temperature
    char result[5]="1234";
    #if SW_DEBUG_SSD1306 == 1
      USE_SERIAL.print(" SSD : 4");
    #endif
    #if HW_uRTClib == 1
      poll_RTC(false); //update clock with terse output
    #elif HW_RTClib == 1
      poll_RTC();
    #elif HW_DS3231 == 1
      poll_DS3231();
    #endif
    #if SW_DEBUG_SSD1306 == 1
      USE_SERIAL.print(" SSD : 5");
    #endif
    #if HW_uRTClib == 1 || (HW_RTClib == 1) || (HW_DS3231 == 1)
      display.setCursor(0, 0);
      display.print(date_str);
      uint16_t packet_age=(uint16_t)((loopMillis-last_packet_millis)/1000); //packet age in seconds
      char ac[6]; 
      if (packet_age > 999){sprintf(ac, "OVR"); } else {sprintf(ac, "%03u", packet_age);}
      display.setCursor(54, 0);
      display.print(ac);
      display.setCursor(78, 0);
      display.print(time_str);
      display.setCursor(0, 16);
      char st[22]; sprintf(st, "Sig: %3ddB RTC: %4.1fC ", rssi_wifi, ds_temperature/4.);
      display.print(st);
    #else
      display.setCursor(0, 0);
      display.print(datestring);
      display.setCursor(100, 0);
      display.print("Time "); //display.println(millis());
      display.setCursor(0, 16);
      display.print("RTC: no hardware"); //display.println(millis());
    #endif

    #if SW_DEBUG_SSD1306 == 1
      USE_SERIAL.print(" SSD : 6");
    #endif
    #if HW_BMP280 == 1
      display.setCursor(0, 24);
      char bmp[22]; sprintf(bmp, "BMP280:%6.2fmb %4.1fC  ", bmp_pressure/100., bmp_temperature/100.);
      display.print(bmp);
    #elif HW_BMP180 == 1 
      display.setCursor(0, 24);
      char bmp[22]; sprintf(bmp, "BMP180:%4.1fC %6.2fmb ", bmp_temperature/100., bmp_pressure/100.);
      display.print(bmp);
    #else
      display.setCursor(0, 24);
      display.print("BMP280: no hardware");
    #endif  

    #if SW_DEBUG_SSD1306 == 1
      USE_SERIAL.print(" SSD : 7");
    #endif
    #if HW_HDC1080 == 1
      display.setCursor(0, 32);
      char buff[22]; sprintf(buff, "HDC1080:%4.1f%%   %4.1fC", hdc_humidity/100., hdc_temperature/100.);
      display.print(buff);
    #elif HW_SI7021 == 1
      display.setCursor(0, 32);
      char buff[22]; sprintf(buff, "SI7021: %4.1f%%   %4.1fC", si_humidity/100., si_temperature/100.);
      display.print(buff);    
    #else
      display.setCursor(0, 32);
      display.print("HDC1080: no hardware"); //display.println(millis());
    #endif
    #if SW_DEBUG_SSD1306 == 1
      USE_SERIAL.print(" SSD : 8");
    #endif
    #if HW_BH1750 == 1
      display.setCursor(0, 48);
      char buf[22];
      sprintf(buf, "BH1750: %5d", lux);
      display.print(buf);
    #else
      display.setCursor(0, 48);
      display.print("BH1750: !hw");
    #endif
    #if SW_DEBUG_SSD1306 == 1
      USE_SERIAL.print(" SSD : 9");
    #endif
    #if HW_INA219 == 1
      display.setCursor(86,48);
      char ina[22];
      sprintf(ina, "%5.1f%mA", ina_current/100.0);
      display.print(ina);
      display.setCursor(80,40);
      sprintf(ina, "%05d%mAh", uint32_t(0.5+ina_energy_total/100.0));
      display.print(ina);
    #endif
    #if SW_DEBUG_SSD1306 == 1
      USE_SERIAL.print(" SSD : 10");
    #endif
    #if HW_ADS1115 == 1
      display.setCursor(0, 56);
      char adc[28];
      #if HW_INA219 == 1
        uint16_t ws=0, wd=0;
        if (uint16_t((rawToRadians(adc3)*22.5)+0.5)>360){ wd=0; } else { wd=uint16_t((rawToRadians(adc3)*22.5)+0.5); }
        if (speed_avg > 65000){ ws=0; } else { ws=speed_avg; }
        sprintf(adc, "%4.1fV %03d %3.1fkmh %4.1fV", (float)((adc2)* ADC2_RATIO), wd, ws/100.0, (ina_voltage+0.0005)/1000.0);
      #else
        sprintf(adc, "%4.1fV %2d %2dkmh", (float)((adc2)* ADC2_RATIO), adc0, adc1);
      #endif
        display.print(adc);
    #else
    display.setCursor(0, 56);
    display.setCursor(24, 56);
    display.print("V");
    display.setCursor(90, 56);
    display.print("T");
    display.setCursor(70, 56);
    display.print("kmh");
    #endif
    #if SW_DEBUG_SSD1306 == 1
      USE_SERIAL.print(" SSD : 11");
    #endif
    #if HW_SOIL == 1
      display.setCursor(0, 40);
      char soil[23]; sprintf(soil, "Soil:     %4.1f%% %4.1fC", float(soil_moisture*0.169491525-35.59322), soil_temperature/10.); // 210 in open air (=0%); up tp 800 in salty water (=100%) linear interpolation
      display.print(soil);
    #elif HW_SOLAR == 1 || HW_SOLAR_TBQ == 1
      display.setCursor(0, 40);
      display.print("Sun: "+String(solar_radiation)+"W/m2");    
    #endif
    
    #if SW_DEBUG_SSD1306 == 1
      USE_SERIAL.println(" SSD : 12");
    #endif

    display.display();
  }
    void clear_SSD1306(){
      portENTER_CRITICAL_ISR(&mux);
        last_lcd_enable=false;
      portEXIT_CRITICAL_ISR(&mux);
      display.clearDisplay();
      display.display();
    }
#endif

#if HW_EEPROM == 1 /* This part does the SRAM and EEPROM coordination via a FIFO circular buffer */
                   /* requires DS3232 (with uRTClib); AT24Cxx EEPROM with AT24CX library;        */
  // Simple fifo test, basis for the EEPROM FIFO circular buffeer to store data records in case of internet outage unable to deliver data to server.
  // data is  put / popped before tail and or head are advanced.
  // unpop is used to recover from a failed http POST
  //#define BUFFER_SIZE 906
  //#define BUFFER_SIZE 881 // 32k x 8 = 256k EEPROM leaving 1024*8bit at the beginning for config data
  
  #include "Arduino.h"
  #include "Wire.h"
  #include "uRTCLib.h"
  #define SRAM_SIZE 235
  uint16_t packet_counter=0;
  uint32_t periodPublishTimestamp=0;
  uint16_t reboot_counter;
  // data to save in SRAM: length uint32_t rain_counter, uint32_t ina_energy_total, uint16_t head, tail; boolean buffer_is_full, crc16
  // Other NVram data: length, Site Name, root certificate, DB: username, password, ssid, password, rain_counter, boot logo, crc16;
  struct config_t{  // better put this in EEPROM, as it get written infrequently
    uint8_t len;
    String influx_host;
    String influx_user;
    String influx_pass;
    String wifi_ssid;
    String wifi_pass;
    uint16_t eeprom_offset;
    uint16_t eeprom_size;
    uint16_t crc;
  };
  uint32_t tick=0; boolean dsram_available=false;
  boolean init_dsram(){  // only works with RTC DS3232 and should be excluded if not present by pre-processor macro
    if (ping_i2c(DS_ADDRESS)){
      Serial.println("  INFO init_dsram() Size: "+String(SRAM_SIZE)+"b Header:"+String(sizeof(record_t))+"b Avail:" + String(SRAM_SIZE-sizeof(record_t))+"b Rec# "+String((SRAM_SIZE-sizeof(record_t))/float(record_size),2)+" @ "+String(record_size)+"b each");
      sram.rec.len=sizeof(record_t);
      sram.rec.ver=1;
      sram.rec.rain_counter=0;
      sram.rec.ina_energy_total=0;
      sram.rec.head=0;
      sram.rec.tail=0;
      sram.rec.buffer_is_full=false;
      sram.rec.periodPublishTimestamp=0;
      sram.rec.packet_counter=0;
      sram.rec.reboot_counter=0;
      sram.rec.store=0;
      sram.rec.status_flags=0;
      sram.rec.crc=CRC::crc16(sram.bytes, sizeof(record_t)-2);
      return write_dsram();
    } else { dsram_available=false; return false;}
  }
  
  boolean write_dsram(){  // update the crc16 and write the complete record_t to sram
    if (ping_i2c(DS_ADDRESS)){
      dsram_available=true;
      uint16_t dsram_crc=CRC::crc16(sram.bytes, sizeof(record_t)-2);
      uint16_t dsram_crc_old=sram.rec.crc;
      if( dsram_crc_old == dsram_crc ){
        #if SW_DEBUG_SRAM == 1
          Serial.println("  DEBUG write_dsram() " + String(sizeof(record_t)) + "b Old CRC16:" + String(sram.rec.crc)+ " New CRC16:" + String(dsram_crc)+" No Changes, Returning!");
        #endif
        return true;
      } else {
      sram.rec.crc=dsram_crc;
      for (uint8_t i = 0; i < sizeof(record_t); i++) {
        rtc.ramWrite(i, sram.bytes[i]);
      }
      #if SW_DEBUG_SRAM == 1
        Serial.println("  DEBUG write_dsram() " + String(sizeof(record_t)) + "b written CRC16:" + String(dsram_crc_old)+"->"+String(sram.rec.crc));
      #endif
      return true;
    }} else { dsram_available=false; return false;}
  }
  bool buffer_empty() {
    return ( (!buffer_is_full) && (head == tail) );
  }
  uint16_t buffer_elements() {
    uint16_t size = BUFFER_SIZE;
    if (!buffer_is_full) {
      if (head >= tail) {
        size = head - tail;
      } else {
        size = BUFFER_SIZE + head - tail;
      }
    } else if (buffer_empty()) { size = 0; } 
    return size;
  }
  boolean upgrade_dsram(){
    #if SW_DEBUG_SRAM == 1
      String message=String("  DEBUG upgrade_dsram() ");
    #endif
    if (ping_i2c(DS_ADDRESS)){
      for (uint8_t i = 0; i < sizeof(record_t); i++) {
        sram.bytes[i]=rtc.ramRead(i);
      }
      #if SW_DEBUG_SRAM == 1
        Serial.println(message+"read "+String(sizeof(record_t)));
      #endif
      if( sram.rec_old.crc == CRC::crc16(sram.bytes, sizeof(record_t_old)-2)){
        //we have a good crc16 from the old structure; read old values and write to new structure; initialize new values with zero.
        #if SW_DEBUG_SRAM == 1
          Serial.println(message+"old good CRC:"+String(sram.rec_old.crc));
        #else
          Serial.print("Init SRAM.....");
        #endif
        union uni_t sram_old=sram;  //make a temporary copy from which to copy values back to the sram.rec
        sram.rec.len=sizeof(record_t);
        sram.rec.rain_counter=sram_old.rec_old.rain_counter;
        sram.rec.ina_energy_total=sram_old.rec_old.ina_energy_total;
        sram.rec.head=sram_old.rec_old.head;
        sram.rec.tail=sram_old.rec_old.tail;
        sram.rec.buffer_is_full=sram_old.rec_old.buffer_is_full;
        sram.rec.periodPublishTimestamp=0;
        sram.rec.packet_counter=0;
        sram.rec.reboot_counter=0;
        sram.rec.ver=1;
        sram.rec.store=0;
        sram.rec.status_flags=0;
        sram.rec.crc=CRC::crc16(sram.bytes, sizeof(record_t)-2);
        #if SW_DEBUG_SRAM == 1
          Serial.println(message+"upgraded new CRC:"+String(sram.rec.crc));
        #else
          Serial.println("UPGRADED to ver: "+String(sram.rec.ver));
        #endif
        return write_dsram(); //return true if success
      } else {
        #if SW_DEBUG_SRAM == 1
          Serial.println(message+"old bad CRC:"+String(sram.rec_old.crc));
        #endif
        return init_dsram(); //return true if success
      }
    }
  }
  boolean read_dsram(){
    String msg = "";
    Serial.print("Init SRAM.....");
    if (ping_i2c(DS_ADDRESS)){
      for (uint8_t i = 0; i < sizeof(record_t); i++) {
        sram.bytes[i]=rtc.ramRead(i);
      }
      if( (sram.rec.crc != 0) && (sram.rec.crc == CRC::crc16(sram.bytes, sizeof(record_t)-2)) ){
        ds_ram_is_good = true;
        Serial.print("OK");
        //Serial.println("Read RAM in " + String(millis()-tick) + "ms. Checksum: " + String(sram.rec.crc));
      } else {
        //upgrade dsram to new structure
        Serial.println("ERROR");
        ds_ram_is_good=upgrade_dsram(); 
      }
      rain_counter               = sram.rec.rain_counter;
      packet_counter             = sram.rec.packet_counter;
      lastPeriodPublishTimestamp = sram.rec.periodPublishTimestamp;
      reboot_counter             = sram.rec.reboot_counter;
      #if HW_INA219 == 1
        ina_energy_total=sram.rec.ina_energy_total;
      #endif
      buffer_is_full=sram.rec.buffer_is_full;
      if( ((sram.rec.tail >=  max_records)) ){
         if( (sram.rec.head==sram.rec.tail) && (!sram.rec.buffer_is_full) ){ head=0; tail=0; Serial.println("SRAM & EEPROM inconsistency have RESET");} else {Serial.println("SRAM & EEPROM inconsistency. Need to RESET manually. STOPPED."); while(1){}; }
      }
      else {
        head=sram.rec.head;
        tail=sram.rec.tail;
      }
    Serial.println(" Len:"+String(sram.rec.len)+" Rain:"+String(sram.rec.rain_counter)+" Energy:"+String(sram.rec.ina_energy_total)+" Head:"+String(sram.rec.head)+
                 " Tail:"+String(sram.rec.tail)+" Rec: "+String(buffer_elements())+" Full?:"+String(sram.rec.buffer_is_full)+" Empty?:"+String(buffer_empty())+" Packet: "+String(packet_counter)+
                 " Reboot:"+String(reboot_counter)+" Unix:"+String(lastPeriodPublishTimestamp)+" CRC: "+String(sram.rec.crc));
    return ds_ram_is_good;
    }
    dsram_available=false; return false;
    Serial.println("ERROR ping 0x"+String(DS_ADDRESS,HEX)+" read_dsram()");
  }
  void init_buffer(uint16_t t, uint16_t h, boolean empty = false) {
    if ((t == h) && empty) {
      buffer_is_full = false;
    }
    if ((t == h) && !empty) {
      buffer_is_full = true;
    }
    head = h % BUFFER_SIZE; tail = t % BUFFER_SIZE;
    tail_address = eeprom_offset + tail * record_size;
    head_address = eeprom_offset + head * record_size;
  }

  //#include <Wire.h>
  #include <AT24CX.h>
  //#include <CRC.h>
  #define EEPROM_SIZE 32767 //bytes
  AT24C256 mem; // EEPROM object
  boolean eeprom_available = false;

  void init_eeprom() {
  #if HW_SERIAL == 1
    String message="Init EEPROM...";
  #endif
    if (ping_i2c(EEPROM_ADDRESS) && ping_i2c(DS_ADDRESS) ){
      eeprom_available=true;
      max_records = (EEPROM_SIZE - eeprom_offset) / sizeof(data_packet); // beep 1 page at start 64bytes and 64 bytes at end = 128 byte
      #if HW_uRTClib == 1
        if ( dsram_available=read_dsram() ){
          if (reboot){
            reboot_counter=sram.rec.reboot_counter;
            reboot_counter++;
            sram.rec.reboot_counter=reboot_counter;
            dsram_available=write_dsram();
          }
          init_buffer(sram.rec.tail, sram.rec.head, !sram.rec.buffer_is_full);
          /* Avoid overwriting the previous (better) data set */
          lastPeriodPublishTimestamp=sram.rec.periodPublishTimestamp;
          /* ALSO NEED TO CHECK FOR BUFFERED DATA to AVOID DUPLICATE -> write a get_last_timestamp() function from DSRAM and EEPROM and compare with unix_time_pub */ 
          if( lastPeriodPublishTimestamp < unix_time_pub ){ publish_data=true; 
            #if HW_SERIAL == 1
              Serial.println(message+"PUBLISH First! Pub:" + String(unix_time_pub) + " Last:" + String(lastPeriodPublishTimestamp) + " periodPublish:"+String(periodPublish/1000)+" Missing#:"+String((unix_time_pub-lastPeriodPublishTimestamp)/(periodPublish/1000.),0));
            #endif
          }
        }
      // check wether to set the alarm_set flag to trigger a data submission based on last unix timestamp in dsram
      #endif
      #if HW_SERIAL == 1
         Serial.println(message+"OK "+String(EEPROM_SIZE)+"b @ " + String(sizeof(data_packet))+"b Rec# " + String(max_records) + "/" + String(max_records * periodPublish /(24*3600*1000.0), 2) + " days @ "+String(periodPublish/1000.,0)+"s offset: " + String(eeprom_offset));
      #endif
    } else { 
      #if HW_SERIAL == 1
        Serial.println(message+"ERROR ping 0x"+String(EEPROM_ADDRESS, HEX)+" init_eeprom()");
      #endif
    }
  }
  void advance_head() {
    if (buffer_is_full) {
      tail = (tail + 1) % BUFFER_SIZE;
      tail_address = eeprom_offset + tail * record_size;
    }
    head = (head + 1) % BUFFER_SIZE;
    head_address = eeprom_offset + head * record_size;
    if (head == tail) {
      buffer_is_full = true;
    }
  }
  void advance_tail() {
    buffer_is_full = false;
    tail = (tail + 1) % BUFFER_SIZE;
    tail_address = eeprom_offset + tail * record_size;
  }
  void reverse_tail() {
    tail = (tail + BUFFER_SIZE - 1) % BUFFER_SIZE;
    tail_address = eeprom_offset + tail * record_size;
    if ( tail == head ) {buffer_is_full = true;}
  }
  void put() {
    if (ping_i2c(EEPROM_ADDRESS) && ping_i2c(DS_ADDRESS) ){
      head_address = eeprom_offset + head * record_size;
      mem.write(head_address, (byte*) &rx.packet, sizeof(data_packet));  //rx is received by esp32 or lora gateway and will be put in buffer
      advance_head();
      sram.rec.head=head; sram.rec.tail=tail; sram.rec.buffer_is_full=buffer_is_full; //need to update both in case that buffer is full and oldest record is overwritten
      write_dsram();
      #if SW_DEBUG_BUFFER == 1
        Serial.println("    WRITE (put) pos#" + String(head) + " to address: " + String(head_address)+" checksum "+String(tx.packet.checksum)+" in "+String(tick)+"ms"+" Records Stored: "+String(buffer_elements()));
      #endif
    } else { Serial.println("EEPROM PING ERROR put()");}
  }
  boolean pop() {
    String msg="";
    if (ping_i2c(EEPROM_ADDRESS) && ping_i2c(DS_ADDRESS) ){
      if (!buffer_empty()) {
        tail_address = eeprom_offset + tail * record_size;
        //Serial.println("Read:"); // read eeprom record
        tick = millis();
        mem.read(tail_address, tx.bytes, sizeof(data_packet)); // tx packet ir retrieved from buffer and sent to server via http or lora
        advance_tail();
        sram.rec.tail=tail; sram.rec.buffer_is_full=buffer_is_full;
        write_dsram();
        if ( tx.packet.checksum != CRC::crc16(tx.bytes, sizeof(data_packet) - 2) ) { Serial.println("Bad Checksum from EEPROM - discarding record."); pop(); }
        tick = millis() - tick;
        #if SW_DEBUG_BUFFER == 1
          Serial.println("    READ (pop) pos#"+String(tail) + " from address: " + String(tail_address)+" checksum "+String(rx.packet.checksum)+msg+" in "+String(tick)+"ms"+" Records Stored: "+String(buffer_elements()));
        #endif
        return true;
      }
      return false;
    } else { Serial.println("EEPROM PING ERROR pop()");}
  }
  boolean unpop() {
    if (ping_i2c(EEPROM_ADDRESS) && ping_i2c(DS_ADDRESS) ){
       if (!buffer_is_full) { //only possible if buffer is not full.
         tail_address = eeprom_offset + tail * record_size;
         //Serial.println("Read:"); // read eeprom record
         mem.read(tail_address, rx.bytes, sizeof(data_packet));
         reverse_tail();
         sram.rec.tail=tail; sram.rec.buffer_is_full=buffer_is_full;
         write_dsram();
         #if SW_DEBUG_BUFFER == 1
           Serial.println("    REVERT (unpop) pos#"+String(tail) + " from address: " + String(tail_address)+" checksum "+String(rx.packet.checksum)+" in "+String(tick)+"ms");
         #endif
      return true;
      }
    return false;
    } else { Serial.println("EEPROM PING ERROR unpop()"); }
  }
#endif

#if defined(ESP32)
  #if HW_WIFI == 1 // start WIFI if ESP32
    #include <WiFi.h>
    #if SW_WIFI_MULTI == 1
      #include <WiFiMulti.h>
      WiFiMulti wifiMulti;
    #endif
    #include <WiFiClientSecure.h>
    WiFiClientSecure influx;
    const char* rootCACertificate =   // Let's Encrypt Root Certificate - Will eventualy go to EEPROM or SPIFFS
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDSjCCAjKgAwIBAgIQRK+wgNajJ7qJMDmGLvhAazANBgkqhkiG9w0BAQUFADA/\n"
    "MSQwIgYDVQQKExtEaWdpdGFsIFNpZ25hdHVyZSBUcnVzdCBDby4xFzAVBgNVBAMT\n"
    "DkRTVCBSb290IENBIFgzMB4XDTAwMDkzMDIxMTIxOVoXDTIxMDkzMDE0MDExNVow\n"
    "PzEkMCIGA1UEChMbRGlnaXRhbCBTaWduYXR1cmUgVHJ1c3QgQ28uMRcwFQYDVQQD\n"
    "Ew5EU1QgUm9vdCBDQSBYMzCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEB\n"
    "AN+v6ZdQCINXtMxiZfaQguzH0yxrMMpb7NnDfcdAwRgUi+DoM3ZJKuM/IUmTrE4O\n"
    "rz5Iy2Xu/NMhD2XSKtkyj4zl93ewEnu1lcCJo6m67XMuegwGMoOifooUMM0RoOEq\n"
    "OLl5CjH9UL2AZd+3UWODyOKIYepLYYHsUmu5ouJLGiifSKOeDNoJjj4XLh7dIN9b\n"
    "xiqKqy69cK3FCxolkHRyxXtqqzTWMIn/5WgTe1QLyNau7Fqckh49ZLOMxt+/yUFw\n"
    "7BZy1SbsOFU5Q9D8/RhcQPGX69Wam40dutolucbY38EVAjqr2m7xPi71XAicPNaD\n"
    "aeQQmxkqtilX4+U9m5/wAl0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNV\n"
    "HQ8BAf8EBAMCAQYwHQYDVR0OBBYEFMSnsaR7LHH62+FLkHX/xBVghYkQMA0GCSqG\n"
    "SIb3DQEBBQUAA4IBAQCjGiybFwBcqR7uKGY3Or+Dxz9LwwmglSBd49lZRNI+DT69\n"
    "ikugdB/OEIKcdBodfpga3csTS7MgROSR6cz8faXbauX+5v3gTt23ADq1cEmv8uXr\n"
    "AvHRAosZy5Q6XkjEGB5YGV8eAlrwDPGxrancWYaLbumR9YbK+rlmM6pZW87ipxZz\n"
    "R8srzJmwN0jP41ZL9c8PDHIyh8bwRLtTcm1D9SZImlJnt1ir/md2cXjbDaJWFBM5\n"
    "JDGFoqgCWjBH4d1QB7wCCZAA62RjYJsWvIjJEubSfZGL+T0yjWW06XyxV3bqxbYo\n"
    "Ob8VZRzI9neWagqNdwvYkQsEjgfbKbYK7p2CNTUQ\n"
    "-----END CERTIFICATE-----\n";
    #include <HTTPClient.h>
    boolean internet_is_working = true, wifi_available=false;
    boolean setClock() {
      String message="INFO setClock() ";
      #if HW_SSD1306 == 1
        if(lcd_enable){ boot_display_SSD1306("NTP...", false); }
      #endif
        configTime(0, 0, CONF_NTP_SERVER_1, CONF_NTP_SERVER_2);
        USE_SERIAL.print("Init NTP......");
        #if SW_DEBUG_DS == 1 && HW_SERIAL == 1
          USE_SERIAL.print("ESP Time(before): "+String(asctime(&t))); // asctime terminates with a \n
        #endif
        //if(wifi connected){
        time_t nowSecs = time(nullptr);
        uint32_t startMillis = millis();
        boolean result = true;
        while (nowSecs < compiledEpoch) {
          USE_SERIAL.print(".");
          delay(500); yield();
          nowSecs = time(nullptr);
          if ( millis() - startMillis > NTP_TIMEOUT ) { USE_SERIAL.println("ERROR"); return false;}
        }
        struct tm t;
        gmtime_r(&nowSecs, &t);
        uint32_t temp=time(nullptr);
        USE_SERIAL.print("OK "); //compare lasttimestamp to now and flag if in future
        if ( lastPeriodPublishTimestamp < temp ) { USE_SERIAL.println("lastPeriodPublishTimestamp:"+String(lastPeriodPublishTimestamp)+" Elapsed:"+String(temp-lastPeriodPublishTimestamp)+"/"+String(periodPublish/1000.,0)+"s"); }
        else {
          sram.rec.periodPublishTimestamp=lastPeriodPublishTimestamp=unix_time_pub;
          USE_SERIAL.println("lastPeriodPublishTimestamp in FUTURE! Setting to unix_time_pub "+String(unix_time_pub)+ "Result:"+String(write_dsram()));
        }
        #if HW_RTClib == 1
          rtc.adjust(DateTime(time(nullptr))); //set to UTC unix time
        #elif HW_uRTClib == 1
          rtc.set(t.tm_sec, t.tm_min, t.tm_hour, (t.tm_wday+8)%8, t.tm_mday, t.tm_mon+1, t.tm_year-100); // set the DS3232
          rtc.refresh();
          if (ds_is_available){
            //#if SW_DEBUG_DS == 1
            temp=getRTCEpoch();
            USE_SERIAL.print("  RTC: "+String(temp)+" "+formatEpoch(temp)+"\n"); 
            //#endif
          } else {
            USE_SERIAL.println(message+"DS Unavailable!");
          }
        #endif
        USE_SERIAL.print      ("  ESP: "+String(temp)+" "+formatEpoch(temp)+"\n"); 
        #if HW_SSD1306 == 1
          if(lcd_enable){ boot_display_SSD1306("OK"); }
        #endif
      return result;
    }
    int8_t get_rssi(){
      rssi_wifi = WiFi.RSSI();
      return rssi_wifi;
    }
    boolean init_http(uint16_t wifi_connect_timeout){ // 3 different possible states 1) WIFI is availabe, at the start and exit, 2) No WIFI at START and Available at exit, 3) No WIFI at start and END
      #if SW_WIFI_MULTI == 1
        #define WIFI_STATUS wifiMulti.run()
      #else
        #define WIFI_STATUS WiFi.status()
      #endif
      if((WIFI_STATUS != WL_CONNECTED)) {
        USE_SERIAL.print("Init WIFI.....");
        #if HW_SSD1306 == 1
          if(lcd_enable){ boot_display_SSD1306("WiFi...", false); }
        #endif
        WiFi.mode(WIFI_STA);
        #if SW_WIFI_MULTI == 1
          wifiMulti.addAP(WIFI_SSID, WIFI_PWD);
          #if defined (WIFI_SSID2) && defined (WIFI_PWD2)
            wifiMulti.addAP(WIFI_SSID2, WIFI_PWD2);
          #endif
          #if defined (WIFI_SSID3) && defined (WIFI_PWD3)
            wifiMulti.addAP(WIFI_SSID3, WIFI_PWD3);
          #endif
        #else
          WiFi.begin(WIFI_SSID, WIFI_PWD);
        #endif
        uint32_t wifi_start_millis=millis(), wifi_cycle_millis=wifi_start_millis;
        while ( millis() - wifi_start_millis < wifi_connect_timeout ){
          if (millis() - wifi_cycle_millis > 500){ wifi_cycle_millis=millis(); USE_SERIAL.print("."); USE_SERIAL.flush();}
          if((WIFI_STATUS == WL_CONNECTED)) {
            wifi_available=true;
            Serial.print("OK SSID: "+String(WiFi.SSID())+" MAC: "+String(WiFi.macAddress())+" RSSI: "+String(WiFi.RSSI())+"dBm IP: "); Serial.println(WiFi.localIP()); 
            #if HW_SSD1306 == 1
              if(lcd_enable){ boot_display_SSD1306("OK"); }
            #endif
            return wifi_available;
          }
        }
      }
      if(WIFI_STATUS != WL_CONNECTED) {        
        wifi_available=false;
        USE_SERIAL.println("ERROR "+String(WIFI_SSID)+" failed");
        #if HW_SSD1306 == 1
          if(lcd_enable){ boot_display_SSD1306("ERROR"); }
        #endif
        return wifi_available;
      }
      if (WIFI_STATUS == WL_CONNECTED){wifi_available=true; return wifi_available;}
      return false;
    }
    String post="", post_redacted="";
    //if client address is x then host=bush else if then host=test
    void assemble_post(){
      String vsys="", esys="", dwind="", swind="", gwind="", psun="", rsun="", patm="", hair="", tout="", tsoil="", tin="", rssi="", noise="", hsoil="", realtime_data;
      #if HW_WIFI == 1 //this is a client / server combo system
        rx=tx;
      #endif
      #if SW_DEBUG_HEAP == 1
        /* ONLY SEND REALTIME DATA AT THE TIME OF MEASUREMENT */
        if( update_data && !go_send_buffer ){
          realtime_data = ",rtc_int="+String(rtc_int_counter)+",esp_get_minimum_free_heap_size="+String(esp_get_minimum_free_heap_size())+",heap_caps_get_largest_free_block="+String(heap_caps_get_largest_free_block(0))+",esp_get_free_heap_size="+String(esp_get_free_heap_size());}
        else { realtime_data = ""; }
      #endif

      //
      if ( rx.packet.system_voltage > 2000) {vsys="";} else {vsys=",v_sys=" + String(rx.packet.system_voltage/100.,2);}
      if ( rx.packet.system_energy == 65535) {esys="";} else {esys=",e_sys=" + String(rx.packet.system_energy/100.,2);}
      if ( rx.packet.wind_direction > 36000) {dwind="";} else {dwind=",d_wind="+String(rx.packet.wind_direction/100.,2);} //only values up to 360.00 degrees
      if ( rx.packet.wind_speed > 20000) {swind="";} else {swind=",s_wind="+String(rx.packet.wind_speed/100.,2);}         //only values up to 200.00km/h
      if ( rx.packet.wind_gust > 20000) {gwind="";} else {gwind=",g_wind="+String(rx.packet.wind_gust/100.,2);}           //only values up to 200.00km/h; filter out error values of 65535
      if ( rx.packet.solar_illuminance >= 54612 ) {rsun="";} else {rsun=",r_sun="+String(rx.packet.solar_illuminance);}
      if ( rx.packet.solar_radiation > 15000 ) {psun="";} else {psun=",p_sun="+String(rx.packet.solar_radiation/10.,1);}
      if ( (rx.packet.atmospheric_pressure > 50000) || (rx.packet.atmospheric_pressure < 15000) ) {patm="";} else {patm=",p_atm="+String(rx.packet.atmospheric_pressure/100.+600,2);} // >1100.00 (110000=60000+50000); <750.00 (15000=15000-6000) good to about 1000m MSL; 60000 Pa offset
      if ( rx.packet.temperature_inside > 10000 ) {tin="";} else {tin=",t_in="+String(rx.packet.temperature_inside/100.,2);}
      if ( rx.packet.temperature_soil > 10000 ) {tsoil="";} else {tsoil=",t_soil="+String(rx.packet.temperature_soil/100.,2);}
      if ( rx.packet.relative_humidity > 10000 ) {hair="";} else {hair=",h_air="+String(rx.packet.relative_humidity/100.,2);}
      if ( rx.packet.temperature_outside > 10000 ) {tout="";} else {tout=",t_out="+String(rx.packet.temperature_outside/100.,2);}
      if ( rx.packet.rssi==127 ) {rssi="";} else {rssi=",rssi="+String((int8_t)rx.packet.rssi);}
      if ( rx.packet.noise==127 ) {rssi="";} else {noise=",noise="+String((int8_t)rx.packet.noise);}
      if ( rx.packet.soil_moisture==65535 ) {hsoil="";} else {hsoil=",h_soil="+String(rx.packet.soil_moisture/100.,2);}
      post = "weather,site="+INFLUX_HOST+" millis="+String(millis())+",rssi="+String(get_rssi())+noise+
              tout+tin+tsoil+
              vsys+esys+
              hair+",rain="+String(rx.packet.rainfall_counter)+
              rsun+psun+patm+
              hsoil+dwind+
              realtime_data+
              swind+gwind+",epoch="+String(rx.packet.unix_time)+",packet="+String(packet_counter + 1)+",reboot="+String(reboot_counter)+" "+String(rx.packet.unix_time);  // only update the packet counter on successful submission
      #if HW_SDCARD == 1
        if (sd_card_available){
          myFile = SD.open(SD_FILE_CSV, FILE_WRITE);
        if (myFile) { // if the file opened okay, write to it:
          //USE_SERIAL.print("Writing to wx.csv ...");
          myFile.println(post); myFile.close();
          USE_SERIAL.println("FILE: "+post);
          // close the file:
          //USE_SERIAL.println("done.");
        } else {
          // if the file didn't open, print an error:
          USE_SERIAL.println("error opening test.txt");
        }
        }
      #endif  
  }

    HTTPClient http;
    
    uint32_t header_date(String date_string){ // takes a http Date: header String and returns the epoch equivalent Example Date header: "Date: Fri, 10 Apr 2020 23:56:52 GMT\n"
      if (date_string.length() < 5){ Serial.print("  ERROR header_date() Input \""+date_string+"\" len:"+date_string.length()+"\n"); return 0;}
      char hL[40]; date_string.toCharArray(hL, 40);
      char *strings[10], *ptr = NULL;
      uint16_t iyear; uint8_t imonth, iday, ihour, iminute, isecond; byte index = 0;
      static const char month_names[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
      #if SW_DEBUG_HTTP == 1
        Serial.print("  DEBUG header_date() Input \""+date_string+"\"\n");
      #endif
      ptr = strtok(hL, " ,:");
      while(ptr != NULL){
        strings[index] = ptr; 
        #if SW_DEBUG_HTTP == 1
          Serial.print("  DEBUG header_date() strings["+String(index)+"]:"); Serial.println(strings[index]);
        #endif
        index++; ptr = strtok(NULL, " ,:");
      }
      imonth = (strstr(month_names, strings[2])-month_names)/3+1;
      iday=String(strings[1]).toInt(); iyear=String(strings[3]).toInt(); ihour=String(strings[4]).toInt(); iminute=String(strings[5]).toInt(); isecond=String(strings[6]).toInt();
      #if SW_DEBUG_HTTP == 1
        Serial.println("  DEBUG header_date() Result: "+String(iday)+"/"+String(imonth)+"/"+String(iyear)+" "+String(ihour)+":"+String(iminute)+":"+String(isecond));
      #endif
      if ( (iyear>2019 && iyear < 2100) && (imonth>0 && imonth<13) && (iday>0 && iday<32) && (ihour>-1 && ihour <24) && (iminute>-1 && iminute <60) && (isecond >-1 && isecond<60) ) {
        struct tm t;
        time_t t_server;
        t.tm_year=iyear-1900; t.tm_mon=imonth-1; t.tm_mday=iday; t.tm_hour=ihour; t.tm_min=iminute; t.tm_sec=isecond;
        t_server=mktime(&t);
        #if SW_DEBUG_HTTP == 1
          Serial.println("  DEBUG header_date() Return: "+String(t_server));
        #endif
        return uint32_t(t_server);
      }
      #if SW_DEBUG_HTTP == 1
        Serial.println("  DEBUG header_date() Return: 0");
      #endif
      return 0; //error condition
    }
    /* Compares http server; RTC & ESP dates, returns the largest difference */
    uint32_t compare_time(uint32_t t_server){
      uint32_t t_esp, t_rtc;
      int32_t diff_temp, diff_max, diff1, diff2, diff3;
      #if defined (ESP32)
        t_esp=getESPEpoch();
      #else
        t_esp=0;
      #endif
      diff_temp=t_server - t_esp; diff1=abs(diff_temp);
      #if HW_uRTClib == 1
        if(ds_is_available){
          t_rtc=getRTCEpoch();  
          diff_temp = t_server - t_rtc; diff2 = abs(diff_temp);
          diff_temp = t_rtc    - t_esp; diff3 = abs(diff_temp);
          diff_max = max(max(diff1, diff2),diff3);
        } else { diff_max = diff1; }
      #else
        diff_max = diff1;
      #endif
      #if (SW_DEBUG_HTTP == 1) && (HW_SERIAL == 1)
        Serial.println("  DEBUG compare_time() SRV: "+String(t_server)+" diff1: "+String(diff1)+" Max: "+String(diff_max));
        Serial.print  ("  DEBUG compare_time() ESP: "+String(t_esp)); 
        #if HW_uRTClib == 1 
          Serial.println                                               (" diff2: "+String(diff2)); 
          Serial.println("  DEBUG compare_time() RTC: "+String(t_rtc) +  " diff3: "+String(diff3)); 
        #endif
      #endif
      return diff_max;  
    }
    /* To send an audio/visual alert */
    void alert(){
      //buzzer and LED alert
    }
    /* Post the sensor data */
    int16_t send_http(String host, boolean secure=true){  // need to fix hostname, ssl/normal, (redacted) url, 
      if(!wifi_available && go_send_buffer){ Serial.println("No Wifi! no buffer sending"); go_send_buffer=false; return -1; }
      char code = -1; String proto, pass, url, url_redacted; int16_t protoCode=-1; uint8_t http_retries; uint32_t tick=millis();
      boolean toggle=false;
      #if HW_SERIAL == 1
        String msg=" [HTTP] ";
      #endif
      
      #define REDACTED String("************")
      wifi_available=init_http(5000); // Check if WIFI is available
      if( wifi_available ){
        /* DIFFERENTIATE BETWEEN LIVE DATA AND EEPROM DATA POST */
        if ( !update_data && go_send_buffer ){ //retrieved tx.packet from EEPROM between update_data
          pop();                         
          #if HW_SERIAL == 1
            USE_SERIAL.println(msg+"Buffer# "+String(buffer_elements()));
          #endif
        }
        /* PREPARE CONNECTION */
        if (!http.connected()){                    // Check Client is connected by http keep-alive
          proto = "https";
          url=String("/write?db=")+INFLUX_DB+"&precision=s&u="+INFLUX_USER+"&p="+INFLUX_PASS;
          url_redacted=proto+"://"+host+":"+CONF_INFLUX_PORT+"/write?db="+INFLUX_DB+"&precision=s&u="+INFLUX_USER+"&p="+REDACTED;
          //USE_SERIAL.println("      "+url_redacted);
          #if defined(ESP8266)  // probably broken now with SecureWifiClient use - needs testing / fixing
            http.begin(url, CERT_SHA1); //Fingerprint is the SHA1 hash of cert?.pem "openssl x509 -noout -in cert1.pem -fingerprint -sha1"
          #elif defined(ESP32)
            #if HW_SERIAL == 1
              USE_SERIAL.println(msg+"Connect: https://"+String(host)+":"+CONF_INFLUX_PORT);  // need to differentiate between http/s
            #endif
            http.begin(influx, host, CONF_INFLUX_PORT, url, true);
            influx.setCACert(rootCACertificate);
            #if (SW_DEBUG_HTTP == 1) && (HW_SERIAL == 1)
              Serial.println("  DEBUG send_http() Timeout SSL/CONNECT/HTTP: "+String(SSL_HANDSHAKE_TIMEOUT)+" "+String(HTTP_CONNECT_TIMEOUT)+" "+String(HTTP_TIMEOUT));
            #endif
            influx.setHandshakeTimeout(SSL_HANDSHAKE_TIMEOUT);
            http.setConnectTimeout(HTTP_CONNECT_TIMEOUT);
            http.setTimeout(HTTP_TIMEOUT);
            #if SW_HTTP_REUSE == 1
              http.setReuse(true);
            #else
              http.setReuse(false);
            #endif
          #endif
        } else {
          #if HW_SERIAL == 1
            USE_SERIAL.println(msg+"Re-use: https://"+String(host)+":"+CONF_INFLUX_PORT);
          #endif
        }
        assemble_post();                  
        #if HW_SERIAL == 1
          USE_SERIAL.println(msg+"POST: "+post);
        #endif
        #if HW_LED_D2 == 1
          digitalWrite(LED_PIN_OK, HIGH);
        #endif
        /* POST THE DATA */
        const char * headerKeys[] = {"date"}; //const size_t numberOfHeaders = 1;
        http.collectHeaders(headerKeys,size_t(1));
        if (internet_is_working){ http_retries=3; } else { http_retries=1; }
        for(uint8_t t = 0; t < http_retries; t++) { //3 connection attempts
          protoCode = http.POST(post);
          internet_is_working=false; 
          uint16_t timeout_post;
          #if (HW_SERIAL == 1)
            if( t > 0 ){Serial.println(msg+"POST retry: "+String(t+1)+" result: "+String(protoCode));}
          #endif
          if ( protoCode == 204 ){ last_packet_millis=millis(); internet_is_working = true; break;}
          if ( http.connected() ){ timeout_post=HTTP_TIMEOUT; } else { timeout_post=3500+3500*pow(2,t);} // adaptive timeout depending on connection status: 5s (connected) or max 3.5 + 7 = 10.5s (unconnected)
          if ( millis()-tick > timeout_post ) { internet_is_working = false; break; }
        }
        /* POST PROCESSING ACCORDING TO THE RESULT */
        if (protoCode == 204) {                                                          // POST successfully submitted to remote server
          uint32_t time_diff, t_server = header_date(http.header("date"));  
          if ( t_server > compiledEpoch ){                                                     //current date (use compile date - better!)
            time_diff = compare_time(t_server);
            #if (SW_DEBUG_HTTP == 1) && (HW_SERIAL == 1)
              Serial.println("  DEBUG send_http() time_diff: "+String(time_diff));
            #endif
            if ( (time_diff > CONF_PERIOD_PUBLISH_MS/2000) && (time_diff > 60) ){  // if time skew is more than half a publish period AND over 1min
              go_set_ntp = true; // to be used in main loop to update time via NTP
              #if (SW_DEBUG_HTTP == 1) && (HW_SERIAL == 1)
                Serial.println("  DEBUG send_http() go_set_ntp: "+String(go_set_ntp));
              #endif
            }
          #if (HW_SERIAL == 1)
            USE_SERIAL.println(msg+String(protoCode)+" OK esp:"+String(uint32_t(time(nullptr)))+" rtc:"+String(getRTCEpoch())+" srv:"+String(t_server)+" '"+http.header("date")+"' Diff:"+String(time_diff));
          #endif            
          }
          go_send_buffer=!buffer_empty(); // internet is OK: Set flag if data is available in EEPROM FIFO buffer, for processing in main loop
          packet_counter++;  //Increment counter as packet was successfully sent 
          sram.rec.packet_counter=packet_counter;
          /* ONLY UPDATE NEW TIMESTAMP, NOT FROM HISTORIC EEPROM DATA */
          if( lastPeriodPublishTimestamp < rx.packet.unix_time ){ //works because the latest packet is always sent BEFORE sending EEPROM buffered packets 
            sram.rec.periodPublishTimestamp = lastPeriodPublishTimestamp = rx.packet.unix_time;
          }
          write_dsram();
        } else {  
          go_send_buffer=false;
          if ( update_data  ) { put(); } else { unpop();  } // sending POST failed, push data to EEPROM, depending on the type of POST this is
          USE_SERIAL.printf(" [HTTP] %d POST ERROR Rec#: %d\n", protoCode, buffer_elements());
        }
      } else {
          assemble_post();
          put();
          USE_SERIAL.printf(" [HTTP] %d NO WIFI Rec#: %d\n", protoCode, buffer_elements());
      }
      // Would be nice to add an ERROR LED (the red) to run in the background for 500ms based on success or failure of POST       
      #if HW_LED_D2 == 1
        digitalWrite(LED_PIN_OK, LOW);
      #endif
      return protoCode; // return status code here to decide if to send ACK or NOK by calling function.  
    }
    boolean after_http(uint16_t http_code){
      return false;
    }
  #endif  //ESP32
#endif // HW_WIFI

#if HW_LORA == 1
  #include <RHReliableDatagram.h>
  #include <SPI.h>
  #if HW_RF69 == 1
    #include <RH_RF69.h>
    //RH_RF69 driver(10, 3); // For Arduino Nano
    RH_RF69 driver(4, 2); // For MoteinoMEGA https://lowpowerlab.com/shop/moteinomega
  #endif 
  #if HW_RF96 == 1
    #include <RH_RF95.h>
    //RH_RF69 driver(10, 3); // For Arduino Nano
    //RH_RF95 driver(4, 2); // For MoteinoMEGA https://lowpowerlab.com/shop/moteinomega
    //RH_RF95 driver(8, 11); // Rocketscream SAMD21
    RH_RF95 driver(RFM96_PIN_CS, RFM96_PIN_IRQ);
  #endif 
  RHReliableDatagram manager(driver, CLIENT_ADDRESS); // Class to manage message delivery and receipt, using the driver declared above
  void init_LORA(){
      pinMode(RFM96_PIN_RESET, OUTPUT);
  digitalWrite(RFM96_PIN_RESET, HIGH);
  delay(10);
  digitalWrite(RFM96_PIN_RESET, LOW);
  delay(10);
  digitalWrite(RFM96_PIN_RESET, HIGH);
  delay(10);
  USE_SERIAL.println("Init...Lora Start");
    if (!manager.init())
    #if HW_SERIAL == 1
      USE_SERIAL.println("init failed");
    #endif
  driver.setTxPower(10);
    if (!driver.setFrequency(433.0)){
    #if HW_SERIAL == 1
      USE_SERIAL.println("setFrequency failed");
    #endif
  }
    #if HW_RF69 == 1
      uint8_t key[] = ENCRYPTION_KEY; // The encryption key has to be the same as the one in the server
      driver.setEncryptionKey(key);
      driver.setModemConfig(RF_MODEM_CONFIG);
    #endif
  
  }
void type_ping(){
  tx.packet.len=sizeof(gen_packet);
  tx.packet.sender=rx.packet.receiver;
  tx.packet.receiver=rx.packet.sender;
  tx.packet.type=6;
    #if HW_SERIAL == 1
  USE_SERIAL.print("Packet TX: "); USE_SERIAL.print(tx.packet.sender); USE_SERIAL.print(" -> "); USE_SERIAL.print(tx.packet.receiver); USE_SERIAL.print(" "); USE_SERIAL.print(tx.packet.len); USE_SERIAL.print("b "); USE_SERIAL.print(" Type 0x"); USE_SERIAL.println("6 (pong)");
    #endif
  if (manager.sendtoWait(tx.bytes, tx.packet.len, tx.packet.receiver)) { last_packet_type=tx.packet.type; }
  else {
    #if HW_SERIAL == 1
    USE_SERIAL.println("sendtoWait failed"); USE_SERIAL.println(""); 
    #endif
    }  
}
void send_time(){
  tx.packet.len=sizeof(time_packet);
  tx.packet.sender=CLIENT_ADDRESS;
  tx.packet.receiver=SERVER_ADDRESS;
  tx.packet.type=7;
      tx.tpk.unix_time[0]=now2.unixtime()-7200;
      tx.tpk.unix_time[1]=0;
      tx.tpk.unix_time[2]=0;
      tx.tpk.unix_time[3]=0;
      tx.tpk.checksum=CRC::crc16(tx.bytes, sizeof(time_packet)-2);
    #if HW_SERIAL == 1
  USE_SERIAL.print("Packet TX: "); USE_SERIAL.print(tx.packet.sender); USE_SERIAL.print(" -> "); USE_SERIAL.print(tx.packet.receiver); USE_SERIAL.print(" "); USE_SERIAL.print(tx.packet.len); USE_SERIAL.print("b "); USE_SERIAL.print(" Type 0x"); USE_SERIAL.println("7 (time)");
    #endif
  if (manager.sendtoWait(tx.bytes, tx.tpk.len, tx.packet.receiver)) { last_packet_type=tx.packet.type; }
  else {
    #if HW_SERIAL == 1
    USE_SERIAL.println("sendtoWait failed"); USE_SERIAL.println(""); 
    #endif
  }
  //delay(1000);
}
void type_pong(){
    send_time(); // request the current time
}
void type_time(){
    tx.tpk.unix_time[3]=now2.unixtime()-7200;
    if(rx.tpk.checksum==CRC::crc16(rx.bytes, sizeof(time_packet)-2)){
      #if HW_SERIAL == 1
        USE_SERIAL.print(" CRC16: "); USE_SERIAL.print(rx.tpk.checksum, HEX); USE_SERIAL.print(" OK."); 
        int32_t offset=(int32_t)((rx.tpk.unix_time[1]-rx.tpk.unix_time[0])+(rx.tpk.unix_time[2]-tx.tpk.unix_time[3]))/2.0;
        if( 5 < offset && offset < 3200){rtc.adjust(DateTime(rx.tpk.unix_time[0]+offset+7200)); USE_SERIAL.print(" RTC Adjusted: "); USE_SERIAL.print(now2.unixtime()-7200); /*and set time alarm flag if offset more than 3200*/ } else {USE_SERIAL.println("");}
        USE_SERIAL.print(" Offset:"); USE_SERIAL.print(offset); USE_SERIAL.print(" Delay:"); USE_SERIAL.println((int32_t)(tx.tpk.unix_time[3]-rx.tpk.unix_time[0])-(rx.tpk.unix_time[2]-rx.tpk.unix_time[1])); 
      #endif
    } else {USE_SERIAL.print(" CRC16: "); USE_SERIAL.print(rx.tpk.checksum, HEX); USE_SERIAL.print(" "); USE_SERIAL.println(CRC::crc16(rx.bytes, sizeof(time_packet)-2), HEX);USE_SERIAL.print(" time_packet_size:"); USE_SERIAL.println(sizeof(time_packet)); }
    for (uint8_t i = 0; i < sizeof(time_packet); i++){
      USE_SERIAL.print(" 0x"); USE_SERIAL.print(rx.bytes[i], HEX);
    }
    USE_SERIAL.println("");  USE_SERIAL.print(" Time 1:"); USE_SERIAL.println(rx.tpk.unix_time[0]); USE_SERIAL.print(" Time 2:"); USE_SERIAL.println(rx.tpk.unix_time[1]); USE_SERIAL.print(" Time 3:"); USE_SERIAL.println(rx.tpk.unix_time[2]); USE_SERIAL.print(" Time 4:"); USE_SERIAL.println(tx.tpk.unix_time[3]);
    #if HW_LORA == 1 // this function should initiate formatting the data for transmission and send
      record_data();
      call_LORA(); 
    #endif
  }
void type_ack(){
  if ( rx.ack.checksum == tx.packet.checksum ) { 
        #if HW_SERIAL == 1
          USE_SERIAL.print(" CRC16: "); USE_SERIAL.print(rx.ack.checksum); USE_SERIAL.println(" OK"); USE_SERIAL.println(""); 
        #endif
    } 
  else { 
        #if HW_SERIAL == 1
          USE_SERIAL.println("Checksum Error!");
        #endif
    }
}
void type_poll(){
  send_data(); //if server is polling, send some data back
}
void type_err(){
    switch (last_packet_type) {
    case 0:    // what type of packet have we received?
    #if HW_SERIAL == 1
      USE_SERIAL.println("reserved");
    #endif
      break;
    case 1:    // poll
    #if HW_SERIAL == 1
      USE_SERIAL.println(" (poll) Sure ???");
    #endif
      //type_poll();
      break;
    case 2:    // data
    #if HW_SERIAL == 1
      USE_SERIAL.println(" Resending data...");
    #endif
      type_poll(); // resend data OR save for later transmission, need to add timnout counter to prevent DOS attacks and flooding radio link
      break;
    case 3:    // ack
    #if HW_SERIAL == 1
      USE_SERIAL.println(" (ack) Sure???");
    #endif
      //type_ack();
      break;
    case 4:    // error
    #if HW_SERIAL == 1
      USE_SERIAL.println(" (err) Sure???");
    #endif
      //type_err();
      break;
    } 
}
void type_data(){
  //client is not supposed to receive data packets
}
void call_LORA(){
  send_data();
}
void send_ping(){
  tx.packet.len=sizeof(gen_packet);
  tx.packet.sender=CLIENT_ADDRESS;
  tx.packet.receiver=SERVER_ADDRESS;
  tx.packet.type=5;
    #if HW_SERIAL == 1
  USE_SERIAL.print("Packet TX: "); USE_SERIAL.print(tx.packet.sender); USE_SERIAL.print(" -> "); USE_SERIAL.print(tx.packet.receiver); USE_SERIAL.print(" "); USE_SERIAL.print(tx.packet.len); USE_SERIAL.print("b "); USE_SERIAL.print(" Type 0x"); USE_SERIAL.println("5 (ping)");
    #endif
  if (manager.sendtoWait(tx.bytes, tx.packet.len, tx.packet.receiver)) { last_packet_type=tx.packet.type; }
  else {
    #if HW_SERIAL == 1
    USE_SERIAL.println("sendtoWait failed"); USE_SERIAL.println(""); 
    #endif
    }  
}
void send_data(){
  if (manager.sendtoWait(tx.bytes, sizeof(tx.bytes), SERVER_ADDRESS)) { last_packet_type=tx.packet.type; }
  else {
    #if HW_SERIAL == 1
    USE_SERIAL.println("sendtoWait failed"); USE_SERIAL.println(""); 
    #endif
    }
}
  void packet_receive(){
    rx.packet.len = sizeof(data_packet);
    if (manager.recvfromAck(rx.stream, &rx.packet.len, &rx.packet.sender))
    {
      last_packet_millis=millis();
    #if HW_SERIAL == 1
      USE_SERIAL.print("Packet RX: "); USE_SERIAL.print(rx.packet.sender); USE_SERIAL.print(" -> "); USE_SERIAL.print(rx.packet.receiver); USE_SERIAL.print(" "); USE_SERIAL.print(rx.packet.len); USE_SERIAL.print("b "); USE_SERIAL.print(" Type 0x"); USE_SERIAL.print(rx.packet.type, HEX);
    #endif
    switch (rx.packet.type) {
    case 0:    // what type of packet have we received?
    #if HW_SERIAL == 1
      USE_SERIAL.println("reserved");
    #endif
      break;
    case 1:    // poll
    #if HW_SERIAL == 1
      USE_SERIAL.println(" (poll)");
    #endif
      type_poll();
      break;
    case 2:    // data
    #if HW_SERIAL == 1
      USE_SERIAL.println(" (data)");
    #endif
      type_data();
      break;
    case 3:    // ack
    #if HW_SERIAL == 1
      USE_SERIAL.println(" (ack)");
    #endif
      type_ack();
      break;
    case 4:    // error
    #if HW_SERIAL == 1
      USE_SERIAL.println(" (err)");
    #endif
      type_err();
      break;
    case 5:    // ping
    #if HW_SERIAL == 1
      USE_SERIAL.println(" (ping)");
    #endif
      type_ping();
      break;
    case 6:    // pong
    #if HW_SERIAL == 1
      USE_SERIAL.println(" (pong)");
    #endif
      type_pong();
      break;
    case 7:    // time
    #if HW_SERIAL == 1
      USE_SERIAL.println(" (time)");
    #endif
      type_time();
      break;
    }
  }
}
#endif
void record_data(){
  tx.packet.len=sizeof(data_packet);
  tx.packet.sender=CLIENT_ADDRESS;
  tx.packet.receiver=SERVER_ADDRESS;
  tx.packet.type=2;
  #if HW_RF69 == 1
    tx.packet.noise=driver.rssiRead();
    tx.packet.rssi=driver.lastRssi();
  #elif HW_RF96 == 1
    tx.packet.rssi=127; //driver.lastRssi();
    tx.packet.noise=127; //driver.rssiRead();
    //tx.packet.noise=tx.packet.rssi-driver.lastSNR();
  #elif HW_WIFI == 1
    tx.packet.rssi=get_rssi();
    tx.packet.noise=127;
  #else
    tx.packet.noise=127;
    tx.packet.rssi=127;
  #endif
  #if HW_BMP280 == 1
    tx.packet.temperature_inside=bmp_temperature_avg;
    tx.packet.atmospheric_pressure=bmp_pressure_avg;
  #else
    tx.packet.temperature_inside=32767;
    tx.packet.atmospheric_pressure=-1;
  #endif
  #if HW_HDC1080 == 1
    tx.packet.temperature_outside=hdc_temperature_avg;
  #elif HW_SI7021 == 1
    tx.packet.temperature_outside=si_temperature_avg;;
  #else
    tx.packet.temperature_outside=32767;
  #endif
  #if HW_SOIL == 1
    tx.packet.temperature_soil=soil_temperature_avg;
  #elif HW_SHT35 == 1
    tx.packet.temperature_soil=sht_temperature_avg;
  #else
    tx.packet.temperature_soil=ds_temperature_avg;
  #endif
  #if HW_ADS1115 == 1
    tx.packet.system_voltage=adc2_avg;
  #else
    tx.packet.system_voltage=32767;
  #endif
  #if HW_INA219 == 1
    tx.packet.system_energy=ina_energy_total;
  #else
    tx.packet.system_energy=-1;
  #endif
  #if HW_HDC1080 == 1
    tx.packet.relative_humidity=hdc_humidity_avg;
  #elif HW_SI7021 == 1
    tx.packet.relative_humidity=si_humidity_avg;
  #else
    tx.packet.relative_humidity=-1;
  #endif
  #if HW_RAIN_INT == 1
    tx.packet.rainfall_counter=rain_counter;
  #else
    tx.packet.rainfall_counter=0;
  #endif
  #if HW_BH1750 == 1
    tx.packet.solar_illuminance=lux_avg;
  #else
    tx.packet.solar_illuminance=-1;
  #endif
  #if HW_SOLAR == 1 || HW_SOLAR_TBQ == 1
    tx.packet.solar_radiation=solar_radiation_avg;
  #else
    tx.packet.solar_radiation=-1;
  #endif
  #if HW_SOIL == 1
    tx.packet.soil_moisture=soil_moisture_avg;
  #elif HW_SHT35 == 1
    tx.packet.soil_moisture=sht_humidity_avg;
  #else
    tx.packet.soil_moisture=65535;
  #endif
  #if HW_SPEED_INT == 1
    tx.packet.wind_speed=speed_avg;
    tx.packet.wind_gust=speed_gust_max_avg;
  #elif HW_SPEED_ADC == 1
    tx.packet.wind_speed=adc0_avg;
  #else
    tx.packet.wind_speed=65535;  //to be filtered out below
    tx.packet.wind_gust=65535;   //to be filtered out below
  #endif
  #if HW_WIND_DIR == 1
    //if ((tx.packet.wind_speed > 0) && (tx.packet.wind_speed!=65535) ){ tx.packet.wind_direction=adc3_avg;} else {tx.packet.wind_direction=-1;} //report only on non zero wind
    if ( tx.packet.wind_speed!=65535 ){ tx.packet.wind_direction=adc3_avg;} else {tx.packet.wind_direction=-1;} //report all values except -1
  #else
    tx.packet.wind_direction=-1;
  #endif
    tx.packet.unix_time=unix_time_pub;
    tx.packet.checksum = CRC::crc16(tx.bytes, tx.packet.len-2);
    //create an influx cli file here which can be imported to the influxdb
    /* String influx = "weather,host=test,type=sensors rssi="+String((int8_t)rx.packet.rssi)+",noise="+String((int8_t)rx.packet.noise)+
      ",t_out="+String(rx.packet.temperature_outside/10.,2)+",t_in="+String(rx.packet.temperature_inside/100.,2)+
      ",t_soil="+String(rx.packet.temperature_soil/4.,2)+",v_sys="+String(rx.packet.system_voltage/100.,2)+
      ",h_air="+String(rx.packet.relative_humidity/100.,2)+",rain="+String(rx.packet.rainfall_counter)+
      ",r_sun="+String(rx.packet.solar_illuminance)+",p_atm="+String(rx.packet.atmospheric_pressure/100.+600,2)+
      ",h_soil="+String(rx.packet.soil_moisture/10.,2)+",d_wind="+String(rx.packet.wind_direction/100.,2)+
      ",s_wind="+String(rx.packet.wind_speed/100.,2)+" "+String(rx.packet.unix_time); */
    #if HW_SERIAL == 1
      #if HW_LORA == 1
       USE_SERIAL.print("Packet TX: "); USE_SERIAL.print(tx.packet.sender); USE_SERIAL.print(" -> "); USE_SERIAL.print(tx.packet.receiver); USE_SERIAL.print(" "); USE_SERIAL.print(tx.packet.len); USE_SERIAL.print("b "); USE_SERIAL.print(" Type 0x"); USE_SERIAL.println("2 (data)");
      #endif
      USE_SERIAL.print(" [DATA] RSSI: "); USE_SERIAL.print((int8_t)tx.packet.rssi); USE_SERIAL.print("/"); USE_SERIAL.print((int8_t)tx.packet.noise); 
      USE_SERIAL.print(" T out/in/soil: "); USE_SERIAL.print(tx.packet.temperature_outside/100.); USE_SERIAL.print(" "); USE_SERIAL.print(tx.packet.temperature_inside/100.); USE_SERIAL.print(" "); USE_SERIAL.print(tx.packet.temperature_soil/100.);
      USE_SERIAL.print(" Volt:"); USE_SERIAL.print(tx.packet.system_voltage/100.); USE_SERIAL.print(" Hum:"); USE_SERIAL.print(tx.packet.relative_humidity/100.); USE_SERIAL.print(" Rain:"); USE_SERIAL.print(tx.packet.rainfall_counter); 
      USE_SERIAL.print(" Sun:"); USE_SERIAL.print(tx.packet.solar_illuminance); USE_SERIAL.print(" Radiation:"); USE_SERIAL.print(tx.packet.solar_radiation/10., 1);  USE_SERIAL.print(" Atm: "); USE_SERIAL.print(tx.packet.atmospheric_pressure/100.+600); USE_SERIAL.print(" Soil%:"); USE_SERIAL.print(tx.packet.soil_moisture/100.);
      USE_SERIAL.print(" Wind:"); USE_SERIAL.print(tx.packet.wind_direction/100.); USE_SERIAL.print("° ");USE_SERIAL.print(String(tx.packet.wind_speed/100.)+"/"); USE_SERIAL.print(tx.packet.wind_gust/100.);
      USE_SERIAL.print("km/h Unix:"); USE_SERIAL.print(tx.packet.unix_time); USE_SERIAL.print(" CRC16:"); USE_SERIAL.println(tx.packet.checksum);
    #endif
  }

uint32_t getCompileEpoch(){
  String ServerDate =  "Sun, 03 May 2020 12:24:01 GMT";
  String CompileDate = String(__DATE__)+" "+String(__TIME__); // Like this: "May  3 2020 14:13:11";
  String ServerFormattedDate="Dow, "+CompileDate.substring(4, 6)+" "+CompileDate.substring(0, 3)+" "+CompileDate.substring(7,20)+" GMT\n";
  //Serial.println("Serverdate : '"+ServerDate+"'");
  //Serial.println("CompileDate: '"+CompileDate+"'");
  //Serial.println("NewDate    : '"+NewDate+"' unix:"+String(unix));
  return header_date(ServerFormattedDate)-7200;
}

#include <rom/rtc.h>

//this also needs to be called on reboot, not only sleeping stuff
void wakeup_SLEEP(){ //function called when the system wakes up from sleep
  //check wakeup count and send data if necessary
  //check wakup event and set mode_interactive true or false accordingly
  //Save non volatile SRAM DS3232 data (rain counter, )
  //ensure that Wind counter, wakup events, sample counter are stored in ESP32 RTC memeory, 
  //initialize variables if necessary
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();
  RESET_REASON reset_reason=rtc_get_reset_reason(0);
  reboot=false;
  //alarm_set=true; rather set this later on if needed to trigger a data set
  USE_SERIAL.print("Init WAKEUP...OK ");
  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0     : USE_SERIAL.print("RTC_IO"  +String(wakeup_reason)); bootCount=bootCount+1; break;
    case ESP_SLEEP_WAKEUP_EXT1     : USE_SERIAL.print("RTC_CNTL"+String(wakeup_reason)); bootCount=bootCount+1; break;
    case ESP_SLEEP_WAKEUP_TIMER    : USE_SERIAL.print("Timer"   +String(wakeup_reason)); bootCount=bootCount+1; break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : USE_SERIAL.print("Touchpad"+String(wakeup_reason)); bootCount=bootCount+1; break;
    case ESP_SLEEP_WAKEUP_ULP      : USE_SERIAL.print("ULP:"    +String(wakeup_reason)); bootCount=bootCount+1; break;
    default                        : USE_SERIAL.print("REBOOT:" +String(wakeup_reason)); reboot=true; break;
  }
  switch (reset_reason) // may want to report this in the first post following reboot
  {
    case 1 : Serial.print (" POWERON_RESET");break;          /**<1, Vbat power on reset*/
    case 3 : Serial.print (" SW_RESET");break;               /**<3, Software reset digital core*/
    case 4 : Serial.print (" OWDT_RESET");break;             /**<4, Legacy watch dog reset digital core*/
    case 5 : Serial.print (" DEEPSLEEP_RESET");break;        /**<5, Deep Sleep reset digital core*/
    case 6 : Serial.print (" SDIO_RESET");break;             /**<6, Reset by SLC module, reset digital core*/
    case 7 : Serial.print (" TG0WDT_SYS_RESET");break;       /**<7, Timer Group0 Watch dog reset digital core*/
    case 8 : Serial.print (" TG1WDT_SYS_RESET");break;       /**<8, Timer Group1 Watch dog reset digital core*/
    case 9 : Serial.print (" RTCWDT_SYS_RESET");break;       /**<9, RTC Watch dog Reset digital core*/
    case 10 : Serial.print (" INTRUSION_RESET");break;       /**<10, Instrusion tested to reset CPU*/
    case 11 : Serial.print (" TGWDT_CPU_RESET");break;       /**<11, Time Group reset CPU*/
    case 12 : Serial.print (" SW_CPU_RESET");break;          /**<12, Software reset CPU*/
    case 13 : Serial.print (" RTCWDT_CPU_RESET");break;      /**<13, RTC Watch dog Reset CPU*/
    case 14 : Serial.print (" EXT_CPU_RESET");break;         /**<14, for APP CPU, reseted by PRO CPU*/
    case 15 : Serial.print (" RTCWDT_BROWN_OUT_RESET");break;/**<15, Reset when the vdd voltage is not stable*/
    case 16 : Serial.print (" RTCWDT_RTC_RESET");break;      /**<16, RTC Watch dog reset digital core and rtc module*/
    default : Serial.print (" NO_MEAN");
  }
  compiledEpoch=getCompileEpoch();
  Serial.print(":"+String(reset_reason)); USE_SERIAL.println(" Compiled: "+String(compiledEpoch)+" "+String(__DATE__)+" "+String(__TIME__));
  if (!reboot){
    //do wakeup stuff here: check number of samples taken and then go back to sleep
    sampleCounter=0;
    collect_sample=true;
      #if HW_LED_D2 == 1
        digitalWrite(LED_PIN_OK, HIGH);
      #endif
    }
  //USE_SERIAL.print("lcd_enable: "); USE_SERIAL.print(lcd_enable); USE_SERIAL.print(" last_lcd_enable: "); USE_SERIAL.print(last_lcd_enable); USE_SERIAL.print(" mode_interactive: "); USE_SERIAL.println(mode_interactive);
}
#if HW_SLEEP == 1
void init_SLEEP(){
  // decide if this is a sleep wakeup event and do stuff accortdingly
  // initialize hardware if needed
}
void call_SLEEP(){
    #if HW_LED_D2 == 1
      digitalWrite(LED_PIN_OK, HIGH);
    #endif
  //check wakeup count and send data if necessary
  //Save non volatile SRAM DS3232 data (rain counter, )
  //ensure that Wind counter, wakup events, sample counter are stored in ESP32 RTC memeory, 
  //initialize variables if necessary
    //shutdown all peripherals - RS485, LEDS, I2C extendder, 
    //USE_SERIAL.print("lcd_enable: "); USE_SERIAL.print(lcd_enable); USE_SERIAL.print(" last_lcd_enable: "); USE_SERIAL.print(last_lcd_enable); USE_SERIAL.print(" mode_interactive: "); USE_SERIAL.println(mode_interactive);
    #if (HW_PCF8574 == 1) || (HW_PCF8574JM == 1)
      #if PCB_VERSION == 1.1
        pcf8574.digitalWrite(PORT2, LOW); // shutdown I2C module before sleeping
      #elif PCB_VERSION == 1.2
        #if HW_REMOTE_I2C == 1
          //shutdown_remote_i2c(); // shutdown I2C module before sleeping on PCB 1.2 connected directly to D13
        #endif
      #endif
      pcf8574.digitalWrite(PORT3, HIGH); // shutdown LED & buzzer just in case they are on PCB Lujeri 1.2
      pcf8574.digitalWrite(PORT4, LOW); // shutdown RS485 module before sleeping
      pcf8574.digitalWrite(PORT5, LOW); // shutdown Soil sensor before sleeping
    #endif
    USE_SERIAL.println(" Going to sleep now");
    #if HW_LED_D2 == 1
      digitalWrite(LED_PIN_OK, LOW);
    #endif
    if (lcd_enable){ clear_SSD1306(); portENTER_CRITICAL_ISR(&mux); lcd_enable=false; portEXIT_CRITICAL_ISR(&mux); } //to ensure it stays off on wakeup
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_27,0); //1 = High, 0 = Low
    esp_deep_sleep_start();
}
#endif

void call_loop(){
  
  // check loop parameters: samples collected, time to publish data, interactive_mode, screen off, & other housekeeping work
  if(mode_interactive){ // only when in interactive_mode
    #if HW_SSD1306 == 1
      if (last_lcd_enable != lcd_enable){ // check if lcd_enable has changed
          USE_SERIAL.print("lcd_enable: "); USE_SERIAL.print(lcd_enable); USE_SERIAL.print(" last_lcd_enable: "); USE_SERIAL.print(last_lcd_enable); USE_SERIAL.print(" mode_interactive: "); USE_SERIAL.print(mode_interactive);

        if(!last_lcd_enable && lcd_enable){ last_screensaver_activity = loopMillis; portENTER_CRITICAL_ISR(&mux); last_lcd_enable=true; portEXIT_CRITICAL_ISR(&mux); screensaver_enable=false;
          #if HW_SSD1306 == 1
            update_SSD1306();
          #endif
          USE_SERIAL.print(" LCD Toggled ON ") ;} //screen has come ON 
        if(last_lcd_enable && !lcd_enable){ portENTER_CRITICAL_ISR(&mux); last_lcd_enable=false; portEXIT_CRITICAL_ISR(&mux); screenOffMillis=loopMillis; clear_SSD1306(); USE_SERIAL.print(" LCD Toggled OFF") ;}}            //screen has gone OFF
      else if(lcd_enable && last_lcd_enable){ //if lcd_enable has not changed AND is true check the timing
          if( loopMillis - lastLCDMillis >= periodLCDupdate ) {                                                               //Update LCD if loopMillis has grown beyond the term "lastUpdate + updateperiod"
            //USE_SERIAL.println(last_screensaver_activity);
            #if HW_SSD1306 == 1
              update_SSD1306(); //USE_SERIAL.println(" Update LCD ");
            #endif
            if (loopMillis - last_screensaver_activity >= screensaver_timeout) {portENTER_CRITICAL_ISR(&mux); lcd_enable=false; portEXIT_CRITICAL_ISR(&mux); screensaver_enable=false; USE_SERIAL.println("Screensaver Active: LCD Off");}
            //USE_SERIAL.println("In Screensaver Timer");
            }
          }
      else if(!lcd_enable && !last_lcd_enable){ //screen is off
        #if HW_SLEEP == 1 
          if(loopMillis-screenOffMillis>=interactiveTimeout){mode_interactive=false; /*Only required for sleeping*/ ;}
        #endif
      }
    #endif
    // check for polling time if interactive mode
    //USE_SERIAL.println("Lastsamplemillis: "+(String)lastSampleMillis+" periodSample: "+(String)periodSample+" loopMillis: "+(String)loopMillis);
    if(publish_data){collect_sample=false; publish_data=false; update_data=true; lastPublishMillis=millis(); adjust_time(); }
    else if( loopMillis - lastSampleMillis >= periodSample ) {collect_sample=true; sampleCounter++; lastSampleMillis = loopMillis;} else {collect_sample=false;}
    }
  else {
    // not interactive mode: collect multiple number of samples and then go to sleep
    if(sampleCounter<multiple) { /*USE_SERIAL.print("1 sampleCounter: "); USE_SERIAL.print(sampleCounter); USE_SERIAL.print(" Publish: "); USE_SERIAL.println(publish_data);*/ collect_sample=true; sampleCounter++; lastSampleMillis = loopMillis;} //collect data before publishing as it is a short and quick affair in non interactive mode.
    else if ((sampleCounter>=multiple) && publish_data){ /*USE_SERIAL.print("2 SampleCounter: ");  USE_SERIAL.println(sampleCounter); */ collect_sample=false; publish_data=false; update_data=true; } /*sampleCounter is reset after flagging update_data */
    else { //non interactive, samples collected, no publishing required, go to sleep
      /*USE_SERIAL.print("3 sampleCounter: "); USE_SERIAL.println(sampleCounter); */
      collect_sample=true; sampleCounter=0;
      #if HW_uRTClib == 1
        init2_RTC(); //sets the alarm to the modulus period
      #endif
      #if HW_SLEEP == 1
        call_SLEEP(); //clean up and go to sleep
      #endif
    }
  }
}
  // infrequently required housekeeping checks: 1. Check heap size -> reboot if necessary; 2. Set NTP flag if timer expires; 3. connectivity timeout checker; 
  //trigPublishMillis, trigNtpMillis; trigRebootMillis;

boolean trigger_reboot=false;
int8_t publish_check(){
  uint32_t function_millis=millis(); // cannot use loopMillis as it may be smaller than some of the other millis timer causing a false positive
  if (trigger_reboot){
    USE_SERIAL.println("  WARNING trigger_reboot NOW!");
    #if SW_IAS_OTA == 1
      if ( init_IAS() ){ IAS.iasLog("Rebooting f:"+String(esp_get_free_heap_size())+" l:"+String(heap_caps_get_largest_free_block(0))+" m:"+String(esp_get_minimum_free_heap_size())); }
    #endif
    delay(1000);
    ESP.restart();    
  }
  /* FAILSAFE HEAP SIZE CHECK */  //todo set alarm flags and / or EEPROM or SDRAM status for debugging; below values may be too conservative?
  // 2020-06-17 10:50:08 Nchima  Log Nchima v1.0.59  1310720 Rebooting_f:131400_l:75904_m:58084
  if( (esp_get_free_heap_size() < 120000) || (heap_caps_get_largest_free_block(0) < 80000) || (esp_get_minimum_free_heap_size() < 50000) ){
    USE_SERIAL.println("  WARNING publish_check() heap size:"+String(esp_get_free_heap_size())+" "+String(esp_get_minimum_free_heap_size())+" "+String(heap_caps_get_largest_free_block(0)));
    trigger_reboot=true;
    #if SW_IAS_OTA == 1
      if ( init_IAS() ){ IAS.iasLog("Trigger f:"+String(esp_get_free_heap_size())+" l:"+String(heap_caps_get_largest_free_block(0))+" m:"+String(esp_get_minimum_free_heap_size())); }
    #endif
  }
  /* FAILSAFE PACKET ACTIVITY TIMER - HARD REBOOT AFTER 9h */   //todo set alarm flags and / or EEPROM or SDRAM status for debugging
  if( ( function_millis - last_packet_millis ) > 1000*3600*9 ){  
    USE_SERIAL.println("  WARNING publish_check() lastPacketMillis:"+String(millis() - last_packet_millis));
    last_packet_millis = loopMillis;
    trigger_reboot=true;
  }
  /* FAILSAFE NTP TIMER EVERY 6h (if internet is working and we are NOT sending buffered data */
  if ( ((function_millis - lastNtpMillis) > 1000*3600*6) && (internet_is_working) && (!go_send_buffer) ){  
    go_set_ntp=true;
    USE_SERIAL.println("  WARNING publish_check() lastNTP:"+String(loopMillis - lastNtpMillis)+" go_set_ntp:"+String(go_set_ntp));
  }
}
boolean adjust_time(){
  uint32_t unix_t;
  boolean result;
  if ( lastNtpMillis > 0 ){ //NTP has been set and is larger than the compile time -> we have a sane RTC value
    // Adjustment to the PeriodPublish above to hit half way through the periodPublish on the next loop
    if      ( (unix_t=getESPEpoch()) > compiledEpoch ){ adjust_ms = periodPublish/2 - 1000*(unix_t%(periodPublish/1000)); USE_SERIAL.print("Fallback ESP Now:"+String(unix_t)+" "+formatEpoch(unix_t)); result=true;}
  }
  else if ( (unix_t=getRTCEpoch()) > compiledEpoch ){ adjust_ms = periodPublish/2 - 1000*(unix_t%(periodPublish/1000)); USE_SERIAL.print("Fallback RTC Now:"+String(unix_t)+" "+formatEpoch(unix_t)); result=true;}
  else { adjust_ms = 0; USE_SERIAL.print("No Adjustment"); result = false; }
  if ( (abs(adjust_ms) ) > (periodPublish/2) ){ adjust_ms = 0; } // safety check in case the value is excessively large for some unknown reason
  Serial.println(" Timeout: "+formatEpoch(unix_t+(periodPublish+adjust_ms)/1000));
  return result;
}
int8_t timers_check(){ // called every loop
  uint32_t function_millis=millis(); // cannot use loopMillis as it may be smaller than some of the other millis timer causing a false positive
  /* IAS CALL HOME TIMER */
  //not using publish_check() above in case for some reason it is not called; need to add an adjust_ms to place itself right after posting data, in order not to interfere with the data posting in case of OTA 
  // will call home very 2h AND when internet_is_working after failure
  #if SW_IAS_OTA == 1
     if ( ( internet_is_working && callHomeInterval > 0 ) && ( function_millis - lastCallHomeTime >= callHomeInterval ) ) {
      USE_SERIAL.println("  INFO Calling Home timers_check() now:"+String(loopMillis)+" last call@"+String(lastCallHomeTime));
      #ifdef WDT_TIMEOUT_SEC
        esp_task_wdt_init(WDT_TIMEOUT_SEC, true);
      #endif
      if ( init_IAS() ){ /* IAS.iasLog("heap:"+String(esp_get_free_heap_size()));*/ IAS.callHome(false); }
      #ifdef WDT_TIMEOUT_SEC
        esp_task_wdt_init(WDT_TIMEOUT_SEC, true);
      #endif
      lastCallHomeTime=function_millis;
    }
  #endif
  /* FALLBACK PUBLISH TIMER IN CASE THERE IS AN ISSUE WITH THE DS3232 INTERRUPTS */
  //USE_SERIAL.println("  WARNING timers_check() PUBLISH DATA! lastNtpMillis:"+String(lastNtpMillis)+" Period:"+String(function_millis-lastPublishMillis)+" millis:"+String(function_millis)+" lastPublishMillis:"+String(lastPublishMillis)+" lastPeriodPublishTimestamp:"+String(lastPeriodPublishTimestamp)+" unix_time_esp:"+String(unix_t)+" publish_data:"+String(publish_data)+" adjust_ms:"+String(adjust_ms)+" next loopMillis:"+String(periodPublish+adjust_ms+function_millis));  // need to publish data without relying on the RTC    uint32_t unix_time;
  if( (function_millis - lastPublishMillis) >= (periodPublish + adjust_ms) ) {  // periodPublish + t_delay = loopMillis - lastPublishMillis
    uint32_t unix_t;
    publish_data=true;
    #if SW_DEBUG_PUBLISH == 1
      unix_t=getRTCEpoch();
      USE_SERIAL.println("UNIX_t:"+String(unix_t)+" compiled_epoch:"+String(compiledEpoch));
    #endif
    // may remove this and just call adjust_time() above
    if      ( (unix_t=getESPEpoch()) > compiledEpoch ){ adjust_ms = periodPublish/2 - 1000*(unix_t%(periodPublish/1000)); USE_SERIAL.println("ESP Epoch:"+String(unix_t)+" "+formatEpoch(unix_t));}  // Adjustment to the PeriodPublish above to hit half way through the periodPublish on the next loop
    else if ( (unix_t=getRTCEpoch()) > compiledEpoch ){ adjust_ms = periodPublish/2 - 1000*(unix_t%(periodPublish/1000)); USE_SERIAL.println("RTC Epoch:"+String(unix_t)+" "+formatEpoch(unix_t));}
    USE_SERIAL.println("  WARNING timers_check() PUBLISH DATA! Period:"+String(function_millis-lastPublishMillis)+" millis:"+String(function_millis)+" lastPublishMillis:"+String(lastPublishMillis)+" lastPeriodPublishTimestamp:"+String(lastPeriodPublishTimestamp)+" unix_time_esp:"+String(unix_t)+" publish_data:"+String(publish_data)+" adjust_ms:"+String(adjust_ms)+" next loopMillis:"+String(periodPublish+adjust_ms+function_millis));  // need to publish data without relying on the RTC    uint32_t unix_time;
    //lastPublishMillis = function_millis; this is set in call_loop() based on publish_data flag
  }
}
void setup(){
  loopMillis = millis(); // only gets initialized in main loop and may cause problems somewhere
  #if HW_SERIAL == 1
    /*USE_SERIAL.println("1 Init SERIAL...OK "+String(SW_SERIAL_SPEED));
    delay(1000);
    Serial.flush(); */
    USE_SERIAL.begin(SW_SERIAL_SPEED);
    USE_SERIAL.println("Init SERIAL...OK "+String(SW_SERIAL_SPEED)+" Site:"+INFLUX_HOST+" SSID:"+WIFI_SSID+" HW:"+String(HW_VERSION_MAJOR)+"."+String(HW_VERSION_MINOR)+" SW:"+SW_VERSION+" Server:"+CONF_INFLUX_SERVER+":"+CONF_INFLUX_PORT+" DB:"+INFLUX_DB+" User:"+INFLUX_USER);
    #if defined(ARDUINO_ARCH_SAMD)
      while(!Serial){}  // check if this does what is expected on the SAMD platform
    #endif
  #endif
  #ifdef WDT_TIMEOUT_SEC
    wdt_setup();
  #endif
  #if SW_UPDATE_SPIFFS_CERT == 1
    update_spiffs_cert();
  #endif
  #if HW_ENCODER == 1
    init_ENCODER();
  #endif
  #if HW_SLEEP == 1
    USE_SERIAL.println("Boot:" +String(bootCount)+ " Reboot: "+String(reboot);
  #endif
  #if HW_REMOTE_I2C == 1
    init_remote_i2c();
  #endif
  #if HW_WATCHDOG == 1
    init_watchdog();
  #endif
  #if HW_WATCHDOG == 1
    call_watchdog();
  #endif
  #if HW_SLEEP == 1
  #endif
    wakeup_SLEEP(); //stuff to do on boot, re-boot or wakeup like alarm_set=true to trigger a data submission
  Wire.begin(21, 22, I2C_FREQUENCY);
  Wire.setClock(I2C_FREQUENCY);
  #if SW_DEBUG_I2C == 1
    Serial.println("I2C Clock: "+String(Wire.getClock()));
  #endif
  #if HW_SSD1306 == 1
    init_SSD1306();
  #endif
  #if HW_LED_D2 == 1
    init_LED();
  #endif
  #if HW_PCF8574 == 1
    init_pcf8574();
  #endif
  #if HW_RTClib == 1
    init_RTC();
  #endif
  #if HW_uRTClib == 1
    init_RTC();
  #endif
  #if HW_DS3231 == 1
    init_DS3231();
  #endif
  #if HW_EEPROM == 1
    //init_dsram(); // !!!! this will reset the EEPROM head and tail counters to ZERO (all data in the buffer will effectively be lost. not for normal use.
    init_eeprom();
  #endif
  if(reboot){
    #if HW_WATCHDOG == 1
      call_watchdog();
    #endif
    #if HW_WIFI == 1
      if (init_http(15000) && go_set_ntp){ // check NTP only if WIFI is available
        #if SW_DEBUG_DS == 1 && HW_SERIAL == 1
          USE_SERIAL.println("  DEBUG loop() go_set_ntp:"+String(go_set_ntp));
        #endif
        #if SW_IAS_OTA == 1
          init_IAS();
        #endif
        for (uint8_t i=0; i<5 ; i++){
          #if HW_WATCHDOG == 1
            call_watchdog();
          #endif      
          if (setClock()){go_set_ntp=false; lastNtpMillis=millis();
            #if SW_DEBUG_DS == 1 && HW_SERIAL == 1
              Serial.println("  DEBUG loop() NTP Success! go_set_ntp:"+String(go_set_ntp)+" lastNtpMillis:"+String(lastNtpMillis));
            #endif
            break;
          } else { internet_is_working=false; }
        }
      }      
    #endif
    adjust_time();
    #if HW_WATCHDOG == 1
      call_watchdog();
    #endif
  }
  #if HW_uRTClib == 1
    if(reboot){init2_RTC();} else {call_RTC();}
  #endif
  #if HW_BMP280 == 1
    init_BMP280();
  #endif
  #if HW_SOIL == 1
    init_SOIL();
  #endif
  #if HW_SOLAR == 1 || HW_SOLAR_TBQ == 1
    init_solar();
  #endif
  #if HW_SI4466 == 1
    init_SI4466();
  #endif
  #if HW_SAMTEMP == 1
    init_SAMTEMP();
  #endif
  #ifdef WDT_TIMEOUT_SEC
    esp_task_wdt_reset();
  #endif
  #ifdef WDT_TIMEOUT_SEC
    esp_task_wdt_reset();
  #endif
  #if HW_RAIN_INT == 1
    init_RAIN();
  #endif
  #if HW_SPEED_INT == 1
    init_SPEED();
  #endif
  #if HW_HDC1080 == 1
    init_HDC1080();
  #endif
  #if HW_SI7021 == 1
    init_SI7021();
  #endif
  #if HW_SHT35 == 1
    init_SHT35();
  #endif
  #ifdef WDT_TIMEOUT_SEC
    esp_task_wdt_reset();
  #endif
  #if HW_BMP180 == 1
    init_BMP180();
  #endif
  #if HW_BH1750 == 1
    init_BH1750();
  #endif
  #if HW_ADS1115 == 1
    init_ADS1115();
  #endif
  #if HW_INA219 == 1
    init_INA219();
  #endif
  #if HW_LORA == 1
    init_LORA();
    send_ping();
    USE_SERIAL.println("SETUP After LORA Init/Ping");
  #endif
  #if HW_SDCARD == 1
    init_sd();
  #endif 
  if(reboot){
    #if HW_DS3231 == 1
      get_DS3231();
      USE_SERIAL.println(" SETUP After DS3231 get");
    #endif
    #if HW_RTClib == 1
      get_RTC();
      USE_SERIAL.println(" SETUP After RTClib get");
    #endif
    #if HW_uRTClib == 1
      get_RTC();
      #if SW_DEBUG_DS == 1
        USE_SERIAL.println(" SETUP After uRTClib get_RTC()");
      #endif
    #endif
    #if HW_SAMTEMP == 1
      get_SAMTEMP();
      USE_SERIAL.println(" SETUP After get_SAMTEMP()");
    #endif
    #if HW_BMP280 == 1
      get_BMP280();
      USE_SERIAL.println(" SETUP After get_BMP280()");
    #endif
    #if HW_BMP180 == 1
      get_BMP180();
      USE_SERIAL.println(" SETUP After get_BMP180()");
    #endif
    #if HW_HDC1080 == 1
      get_HDC1080();
      USE_SERIAL.println(" SETUP After get_HDC1080()");
    #endif
    #if HW_SI7021 == 1
      get_SI7021();
      USE_SERIAL.println(" SETUP After get_SI7021()");
    #endif
    #if HW_SHT35 == 1
      get_SHT35();
      USE_SERIAL.println(" SETUP After get_SHT35()");
    #endif
    #if HW_BH1750 == 1
      get_BH1750();
      USE_SERIAL.println(" SETUP After get_BH1750()");
    #endif
    #if HW_INA219 == 1
      get_INA219();
      USE_SERIAL.println(" SETUP After get_INA219()");
    #endif
    #if HW_ADS1115 == 1
      get_ADS1115();
      USE_SERIAL.println(" SETUP After get_ADS1115()");
    #endif
    #if HW_SOIL == 1
      get_SOIL();
      USE_SERIAL.println(" SETUP After get_SOIL()");
    #endif
    #if HW_SOLAR == 1 || HW_SOLAR_TBQ == 1
      get_solar();
    #endif
    #if HW_RAIN_INT == 1
      get_RAIN();
      USE_SERIAL.println(" SETUP After get_RAIN()");
    #endif
    #if HW_SPEED_INT == 1
      get_SPEED();
      USE_SERIAL.println(" SETUP After get_SPEED()");
    #endif
  }
  #if HW_WATCHDOG == 1
    call_watchdog();
  #endif
  #if HW_SERIAL == 1
    //clean_SERIAL();
  #endif
  #if HW_SSD1306 == 1
    //update_SSD1306(); this is handled in the call_loop function, no need for this.
  #endif
  #if HW_LORA == 1
    packet_receive();
  #endif
  #if HW_LORA == 1 // this function should initiate formatting the data for transmission and send
    //record_data();
    //call_LORA(); 
  #endif
  #if HW_WATCHDOG == 1
    call_watchdog();
  #endif
}

void loop(){
loopMillis = millis();
#ifdef WDT_TIMEOUT_SEC
  esp_task_wdt_reset();
#endif
#if HW_LORA == 1
  packet_receive();
#endif
#if HW_uRTClib == 1
  timers_check(); //check if an update_data=true is in order
  call_RTC();     //influences some call_loop decisions and flags; sets update_data for use below
#endif
call_loop();  //sets the different boolean variable to decide what to do below
if (collect_sample){
  #if HW_WATCHDOG == 1
    call_watchdog();
  #endif
  #if HW_DS3231 == 1
    poll_DS3231(true);
  #endif
  #if HW_RTClib == 1
    poll_RTC(true);
  #endif
  #if HW_uRTClib == 1
    poll_RTC(true);
  #endif
  #if HW_SAMTEMP == 1
    poll_SAMTEMP();
  #endif
  #if HW_BMP280 == 1
    poll_BMP280(true);
  #endif
  #if HW_BMP180 == 1
    poll_BMP180();
  #endif
  #if HW_HDC1080 == 1
    poll_HDC1080();
  #endif
  #if HW_SI7021 == 1
    poll_SI7021(true);
  #endif
  #if HW_SHT35 == 1
    poll_SHT35(true);
  #endif
  #if HW_BH1750 == 1
    poll_BH1750();
  #endif
  #if HW_INA219 == 1
    poll_INA219();
  #endif   
  #if HW_ADS1115 == 1
    poll_ADS1115();
  #endif
  #if HW_SOIL == 1
    poll_SOIL();
  #endif
  #if HW_SOLAR == 1 || HW_SOLAR_TBQ == 1
    poll_solar();
  #endif
  #if HW_RAIN_INT == 1
    get_RAIN();
  #endif
  #if HW_SPEED_INT == 1
    poll_SPEED();
  #endif
  #if HW_SERIAL == 1
    clean_SERIAL();
  #endif
  #if HW_SSD1306 == 1
    //if (lcd_enable){update_SSD1306(); } else {  }
  #endif
  collect_sample=false;
}

#if HW_WIFI == 1
  #if HW_WATCHDOG == 1
    call_watchdog();
  #endif
  
  if (go_set_ntp){
    Serial.println("      go_set_ntp:"+String(go_set_ntp));
    if(init_http(8000)){ i_ntp_counter=0; } else { i_ntp_counter=6;} // initiate the NTP sequence only if wifi is connected
    go_set_ntp=false;
  }
    if (i_ntp_counter <= 3){
      i_ntp_counter++;
      #if SW_DEBUG_DS == 1 && HW_SERIAL == 1
        Serial.println("Set NTP count "+String(i_ntp_counter));
      #endif
      if (setClock()){i_ntp_counter=6; lastNtpMillis=loopMillis;}
      #if HW_WATCHDOG == 1
        call_watchdog();
      #endif
    }
#endif

#if HW_WIFI == 1 
  if(go_send_buffer && !update_data && wifi_available){
    send_http(CONF_INFLUX_SERVER, true); //secure https true
    // better to call after_http from within send_http()
    #if HW_WATCHDOG == 1
      call_watchdog();
    #endif
  }
#endif
  
if(update_data){
    //updateDataMillis=millis();
    USE_SERIAL.println("INFO loop() update_data:"+String(update_data)+" lastPublishMillis:"+String(lastPublishMillis));
    #if HW_DS3231 == 1
      get_DS3231();
    #endif
    #if HW_RTClib == 1
      get_RTC();
    #endif
    #if HW_uRTClib == 1
      get_RTC();
    #endif
    #if HW_SAMTEMP == 1
      get_SAMTEMP();
    #endif
    #if HW_BMP280 == 1
      get_BMP280();
    #endif
    #if HW_BMP180 == 1
      get_BMP180();
    #endif
    #if HW_HDC1080 == 1
      get_HDC1080();
    #endif
    #if HW_SI7021 == 1
      get_SI7021();
    #endif
    #if HW_SHT35 == 1
      get_SHT35();
    #endif
    #if HW_BH1750 == 1
      get_BH1750();
    #endif
    #if HW_INA219 == 1
      get_INA219();
    #endif   
    #if HW_ADS1115 == 1
      get_ADS1115();
    #endif
    #if HW_SOIL == 1
      get_SOIL();
    #endif
    #if HW_SOLAR == 1 || HW_SOLAR_TBQ == 1
      get_solar();
    #endif
    #if HW_RAIN_INT == 1
      get_RAIN();
    #endif
    #if HW_SPEED_INT == 1
      get_SPEED();
    #endif
    #if HW_SERIAL == 1
      clean_SERIAL();
    #endif
    #if HW_LORA == 1
      record_data();
      call_LORA(); 
    #endif
    #if HW_WATCHDOG == 1
      call_watchdog();
    #endif
    #if HW_WIFI == 1 
      record_data(); // this function should initiate formatting the data for transmission and send
      send_http(CONF_INFLUX_SERVER, true);
      #if HW_WATCHDOG == 1
        call_watchdog();
      #else
      #endif
      #if SW_MILLIS_ROLLOVER_REBOOT == 1
        /* REBOOT 10*300s before millis() overflow  */
        if (loopMillis > CONF_MILLIS_REBOOT_MS ){ Serial.println("REBOOTING!!!!"); ESP.restart();}
      #endif
    #endif
    sampleCounter=0;  // used for burst mneasurements (and sleeping in between)
    publish_check(); //to ensure it reboots after data has been published
    update_data=false;
}

#if HW_SI4466 == 1
  if( loopMillis - lastPublishMillis >= periodPublish ) {
    lastPublishMillis = loopMillis;
    assemble_packet();
    call_SI4466();
  }
#endif

#if HW_SPEED_INT == 1
  call_SPEED();
#endif
#if HW_RAIN_INT == 1
  call_RAIN();
#endif
#if HW_ENCODER == 1
  call_ENCODER();
#endif

}

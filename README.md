 # Native nRF9160 Camera
 A program to interface the nRF9160 to the OV7675 camera module
 
 ## status file format
 As of firmware revision v1.4.0 the status file format is given as:
 DATE-TIME, TIME_SOURCE, CAPTURE_NO, BATTERY_mV, MCCMNC, RSRQ, RSRP
 So the log:
        2024/03/22-01:44:27 UTC,NETWORK_TIME,11,4175,310260,13,45
 means:
  DATE-TIME:     2024/03/22-01:44:27 UTC
  TIME_SOURCE:   NETWORK_TIME           (time obtained from the network)
  CAPTURE_NO:    11                     (number of images since last reset)
  BATTERY_mV:    4175                   (battery voltage (mV))
  MCCMNC:        310260                 (the current network operator MCCMNC code)
  RSRQ:          13                     (reference signal recieved power)
  RSRP:          45                     (reference signal recieved quality)
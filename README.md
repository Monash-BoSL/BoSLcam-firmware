 # Native nRF9160 Camera
 A program to interface the nRF9160 to the OV7675 camera module

 ## Building

 The firmware should be built against nrf-sdk v2.1.0. 
 The ftp_client needs to be patched with the version located in the `.\patch\` directory of this repo to enable large file upload.
 I recommend renaming the current `ftp_client.h` and `ftp_client.c` in the nrf-sdk and then symlinking those against the files located in the `.\patch\` directory.
 
 
 Once the appropriate environment variables have been set the firmware can be built as:
 ```
 west build -b native_camerans
 ```

 ## Programming

 Instructions for programming the BoSLcam can be found at: https://www.bosl.com.au/wiki/BoSLcam

 ## Config File Format
 An example config file is provided in `config.txt`.

 ## Status File Format
 As of firmware revision v1.4.0 the status file format is given as:
 ```
 DATE-TIME, TIME_SOURCE, CAPTURE_NO, BATTERY_mV, MCCMNC, RSRQ, RSRP
 ```
 So the log:
 ```
        2024/03/22-01:44:27 UTC,NETWORK_TIME,11,4175,310260,13,45
 ```
 means:
 ```
  DATE-TIME:     2024/03/22-01:44:27 UTC
  TIME_SOURCE:   NETWORK_TIME           (time obtained from the network)
  CAPTURE_NO:    11                     (number of images since last reset)
  BATTERY_mV:    4175                   (battery voltage (mV))
  MCCMNC:        310260                 (the current network operator MCCMNC code)
  RSRQ:          13                     (reference signal recieved quality)
  RSRP:          45                     (reference signal recieved power)
 ```

## Known Issues

Filesystem operations may return an error code even though they succeed according to [this thread](https://devzone.nordicsemi.com/f/nordic-q-a/115228/fatfs-sample-file-close-error--5). This issue is purportedly fixed in nrf-sdk v2.8.0.

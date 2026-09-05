# ESP32 API LED Example

This example extends the **ESP32 HTTPS Client Example** to periodically query a REST API. Based on a configurable integer threshold, the application automatically turns an onboard or external LED on or off.

For detailed setup requirements (Wi-Fi, certificates, and TLS configuration), please refer to the base `README.md` file of the [ESP32 HTTPS Client Example](https://github.com/VasquezPonte/esp32_https_client_example).

## Configure the Project

Run:

```sh
idf.py menuconfig
```

Configure the following project settings:

- Wi-Fi SSID
- Wi-Fi password
- Endpoint API URL
- GPIO pin connected to the LED
- Threshold value for LED decision

The settings can be found under the project's configuration menu.

## Build

Build the project with:

```sh
idf.py build
```

If the project has not been configured yet, run `idf.py menuconfig` first.

## Flash the ESP32

Connect the ESP32 development board and determine its serial device.

For example, on Linux:

```sh
ls /dev/ttyUSB*
```

Then flash the firmware:

```sh
idf.py -p /dev/ttyUSB0 flash
```

If necessary, replace `/dev/ttyUSB0` with the correct serial port.

## Monitor Serial Output

Start the ESP-IDF monitor:

```sh
idf.py -p /dev/ttyUSB0 monitor
```

To exit the monitor, press:

```text
Ctrl+]
```

## Build, Flash and Monitor

These operations can also be performed together:

```sh
idf.py -p /dev/ttyUSB0 flash monitor
```

## Example Successful Output

A successful request should produce output similar to:

```text
I () app: Starting API LED Example (Infinite Loop)
I () app: ========================================
I () app: STATE: INIT
I () app: ========================================
I () app: STATE: WIFI_CONNECT
I () app: Wi-Fi connected
I () app: STATE: TIME_SYNC
I () app: System time synchronized
I () app: STATE: API_REQUEST
I () https_client: GET https://example.com/api/random.php
I () esp-x509-crt-bundle: Certificate validated
I () https_client: HTTP status: 200
I () https_client: Parsed value: 5
I () app: STATE: PROCESS_RESULT | value=5 | threshold=3
I () app: Value >= 3 -> LED ON
I () app: STATE: DELAY (10000 ms)
I () app: STATE: API_REQUEST
I () https_client: GET hhttps://example.com/api/random.php
I () esp-x509-crt-bundle: Certificate validated
I () https_client: HTTP status: 200
I () https_client: Parsed value: 1
I () app: STATE: PROCESS_RESULT | value=1 | threshold=3
I () app: Value < 3 -> LED OFF
I () app: STATE: DELAY (10000 ms)
```

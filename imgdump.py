import time

from pynrfjprog import LowLevel
from pynrfjprog.API import DeviceFamily as NrfDeviceFamily

JLINK_KHZ = 4000
CHANNEL_INDEX = 1

def read_rtt():
    api = LowLevel.API(NrfDeviceFamily.NRF91)
    api.open()

    snr = api.enum_emu_snr()[0]
    api.connect_to_emu_with_snr(snr, JLINK_KHZ)

    mcu = api.read_device_version()
    print("connected to: ", mcu)

    api.rtt_start()
    time.sleep(1)
    while not api.rtt_is_control_block_found():
        print("Waiting for RTT control block")
        api.rtt_stop()
        time.sleep(0.5)
        api.rtt_start()
        time.sleep(0.5)

    while True:
        rd = api.rtt_read(CHANNEL_INDEX,1)
        print(rd, end = '', flush=True)

    api.close()


if __name__ == "__main__":
    read_rtt()

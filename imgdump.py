import time
import sys

from pynrfjprog import LowLevel
from pynrfjprog.API import DeviceFamily as NrfDeviceFamily

JLINK_KHZ = 4000
CHANNEL_INDEX = 1

def init_rtt() -> LowLevel.API:
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

    print("Connected")
    return api

BMP_HEADER_SIZE = 66

def twizzle(data):
    for p in range(BMP_HEADER_SIZE,len(data)):
        x = data[p]
        data[p] = (x & ~(0x3)) | ((x >> 0x1) & 0x1) | ((x << 1) & 0x2)
    return data


def dump_rtt(api: LowLevel.API, debug_board: bool):
    print("Dumping ...", end='', flush=True)
    tik = time.time()
    with open("dump.bmp", 'wb') as f:
        chunk_size = 1
        data = bytearray()
        while (chunk_size > 0):
            api.rtt_write(CHANNEL_INDEX, bytearray([0xCC]), None)
            chunk_size_raw = api.rtt_read(CHANNEL_INDEX,4, None)
            chunk_size = int.from_bytes(chunk_size_raw, byteorder='little')

            # print("chunk size:", chunk_size, end = '')
            data += api.rtt_read(CHANNEL_INDEX,chunk_size, None)
            # print(" OK")

        if debug_board:
            data = twizzle(data)

        f.write(data)


    tok = time.time()
    print(" Done ({:.2f} s)".format(tok-tik))




if __name__ == "__main__":
    debug_board = ("-d" in sys.argv)

    api = init_rtt()
    while True:
        dump_rtt(api, debug_board)
        input("Press Any Enter to Repeat")

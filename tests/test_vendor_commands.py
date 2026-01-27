#!/usr/bin/env python3

import argparse
import logging
import time
import sys
sys.path.append('../../utp_python_common_lib/libraries')
import dvk_probe
from If820Board import If820Board

"""
This test uses one IF820 DVK to test the DVK Probe (RP2040) USB vendor specific commands.

Hardware Setup
This sample requires the following hardware:
- IF820 connected to PC via USB
- Connect RP2040 side of BT_HOST_WAKE and BT_DEV_WAKE together
"""

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-d', '--debug', action='store_true',
                        help="Enable verbose debug messages")
    parser.add_argument('-b', '--bootloader', action='store_true',
                        help="Reboot the probe into bootloader mode")
    logging.basicConfig(format='%(asctime)s: %(message)s', level=logging.INFO)
    args, unknown = parser.parse_known_args()
    if args.debug:
        logging.info('Debugging mode enabled')
        logging.getLogger().setLevel(logging.DEBUG)

    boards = If820Board.get_connected_boards()
    if len(boards) == 0:
        logging.error("No boards found")
        exit(1)

    choice = 0
    if len(boards) > 1:
        print("Which board do you want to flash?")
        for i, board in enumerate(boards):
            print(f"{i}: {board.probe.id}")
        choice = int(input("Enter the number of the board: "))
    board = boards[choice]
    board.open_and_init_board(False)
    probe = board.probe

    if args.bootloader:
        logging.info('Rebooting probe into bootloader mode...')
        probe.reboot(True)
        exit(0)

    logging.info(f'Probe firmware version: {probe.firmware_version}')
    orig_settings = probe.read_internal_settings()
    logging.info(f'Current probe settings: {orig_settings}')

    logging.info('Writing new settings to probe and verifying...')
    probe.program_v2_settings(orig_settings.target_board_vendor.decode('utf-8'),
                              f'IF820 test {time.time():.0f}',
                              orig_settings.target_device_vendor.decode('utf-8'),
                              orig_settings.target_device_name.decode('utf-8'),
                              0x1FA3,
                              0x1111)
    logging.info('done!')
    logging.info('Restoring original settings')
    probe.write_internal_settings(orig_settings)

    # Test that trying to config an invalid IO triggers errors
    res = probe.gpio_to_input(1)
    assert res != 0, 'Should not be able to configure GPIO 1'
    res = probe.gpio_to_output_high(1)
    assert res != 0, 'Should not be able to configure GPIO 1'
    res = probe.gpio_read(1)
    assert res != 0 and res != 1, 'Should not be able to configure GPIO 1'

    # Loop back test to set and read an IO state
    logging.info(f'Setting IO {probe.GPIO_16} as output')
    res = probe.gpio_to_output(probe.GPIO_16)
    logging.info(f'Result: {res}')
    logging.info(f'Setting IO {probe.GPIO_17} as input')
    res = probe.gpio_to_input(probe.GPIO_17)
    logging.info(f'Result: {res}')
    res = probe.gpio_read(probe.GPIO_17)
    logging.info(f'IO {probe.GPIO_17} state: {res}')
    logging.info(f'Setting IO {probe.GPIO_16} as high')
    res = probe.gpio_to_output_high(probe.GPIO_16)
    logging.info(f'Result: {res}')
    # Loop back GPIO 0 and GPIO 1 to ensure GPIO 1 reads a high state
    res = probe.gpio_read(probe.GPIO_17)
    gpio_1_state = res
    logging.info(f'IO {probe.GPIO_17} state: {gpio_1_state}')
    assert gpio_1_state == dvk_probe.HIGH, f'GPIO1 state [{gpio_1_state}] is not high'
    # Test low state
    logging.info(f'Setting IO {probe.GPIO_16} as low')
    res = probe.gpio_to_output_low(probe.GPIO_16)
    logging.info(f'Result: {res}')
    res = probe.gpio_read(probe.GPIO_17)
    gpio_1_state = res
    logging.info(f'IO {probe.GPIO_17} state: {gpio_1_state}')
    assert gpio_1_state == dvk_probe.LOW, f'GPIO1 state [{gpio_1_state}] is not low'

    logging.info('Done!')
    probe.reboot()

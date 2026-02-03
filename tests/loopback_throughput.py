#!/usr/bin/env python3

import logging
import argparse
import time
import sys
import threading
import string
import random
sys.path.append('../../utp_python_common_lib/libraries')
from If820Board import If820Board


"""
This test uses one IF820 DVK to test the DVK Probe (RP2040) serial performance.
Received data is compared to the sent data to ensure data integrity.
Each data chunk sent is randomly generated.

Hardware Setup
This sample requires the following hardware:
- IF820 connected to PC via USB
- P_UART_TXD and P_UART_RXD jumpers are removed from the DVK
- Connect RP2040 side of P_UART_TXD and P_UART_RXD together for a loopback connection
"""

UART_BITS_PER_BYTE = 10
# UART baud rate to use for the test.
BAUD_RATE = 3000000
# How long to run the throughput test for.
THROUGHPUT_TEST_TIMEOUT_SECS = 10
# Length of the data chunk to send. This is the size of each chuck written in the send loop.
# To Achieve maximum throughput, this should be set to greater than or equal to the number of bytes
# that can be sent in one send loop iteration. For example:
# SEND_DATA_CHUNK_LEN = THROUGHPUT_TEST_TIMEOUT_SECS * BAUD_RATE / UART_BITS_PER_BYTE
SEND_DATA_CHUNK_LEN = 100000
# How long to wait for data to be received. This is used to determine when RX is finished.
RX_TIMEOUT_SECS = 1


def send_receive_data(board: If820Board):
    board.p_uart.clear_rx_queue()
    data_sent = bytearray()
    rx_done_event = threading.Event()
    tx_done_event = threading.Event()
    tx_start_time = time.time()
    rx_thread = threading.Thread(target=lambda: __receive_data_thread(board,
                                                                      data_sent,
                                                                      rx_done_event,
                                                                      tx_start_time,
                                                                      tx_done_event),
                                 daemon=True)
    rx_thread.start()
    logging.info(
        f"Start sending data for at least {THROUGHPUT_TEST_TIMEOUT_SECS} seconds...")
    packet_num = 0
    # Pre-generate random data once to avoid overhead in the send loop
    random_chunk = bytes(''.join(random.choices(string.ascii_letters +
                                                string.digits, k=int(SEND_DATA_CHUNK_LEN))), 'utf-8')
    while (time.time() - tx_start_time) < THROUGHPUT_TEST_TIMEOUT_SECS:
        # Create a mutable copy and add packet header
        send = bytearray(random_chunk)
        header = packet_num.to_bytes(4, byteorder='little')
        send[:4] = header
        send = bytes(send)
        board.p_uart.send(send)
        data_sent.extend(send)
        packet_num += 1

    logging.info(f"Sent {len(data_sent)} bytes, Wait for data to be received...")
    tx_done_event.set()
    if not rx_done_event.wait(THROUGHPUT_TEST_TIMEOUT_SECS * 2):
        logging.error("Timeout waiting for data to be received")


def __receive_data_thread(board: If820Board,
                          data_sent: bytearray,
                          rx_done_event: threading.Event,
                          tx_start_time: float,
                          tx_done_event: threading.Event):
    last_rx_time = time.time()
    logging.info("Start receiving data...")
    data_received = bytearray()
    while time.time() - last_rx_time < RX_TIMEOUT_SECS or len(data_received) <= 0 or not tx_done_event.is_set():
        rx_data = board.p_uart.read()
        if len(rx_data) > 0:
            last_rx_time = time.time()
            data_received.extend(rx_data)
    logging.info(f"All data received! Received {len(data_received)} bytes")
    if data_received != data_sent:
        logging.error(
            f"\r\n\r\nData received does not match data sent!\r\n")
        for index, (txd, rxd) in enumerate(zip(data_sent, data_received)):
            if txd != rxd:
                print(
                    f"RX mismatch at index: {index} (packet {hex(int(index/SEND_DATA_CHUNK_LEN))}), val: {hex(rxd)}")
                print(
                    f"tx[{index}]: {hex(data_sent[index])}, tx[{index + 1}]: {hex(data_sent[index + 1])}, tx[{index + 2}]: {hex(data_sent[index + 2])}")
                print(
                    f"rx[{index}]: {hex(data_received[index])}, rx[{index + 1}]: {hex(data_received[index + 1])}")
                data_received[index:index] = bytes([data_sent[index]])
                # Just print the first mismatch
                break
    bytes_per_sec = len(data_received) / (last_rx_time - tx_start_time)
    logging.info(
        f"Total time: {last_rx_time - tx_start_time:.1f} Throughput: {bytes_per_sec:.2f} Bps ({bytes_per_sec * 8:.2f} bps)")
    rx_done_event.set()


def run_test(baud: int):
    boards = If820Board.get_connected_boards()
    if len(boards) < 1:
        logging.critical(
            "One IF820 board required for this test.")
        exit(1)

    board = boards[0]
    board.open_and_init_board(False)
    board.reconfig_puart(baud)
    send_receive_data(board)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-d', '--debug', action='store_true',
                        help="Enable verbose debug messages")
    args, unknown = parser.parse_known_args()
    logging.basicConfig(
        format='%(asctime)s [%(module)s] %(levelname)s: %(message)s', level=logging.INFO)
    if args.debug:
        logging.info("Debugging mode enabled")
        logging.getLogger().setLevel(logging.DEBUG)

    logging.info(f"Running throughput test @{BAUD_RATE} baud")
    run_test(BAUD_RATE)
    logging.info("Done!")

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
Throughput is tested for these scenarios:
- P_UART to the HCI_UART
- HCI_UART to the P_UART
- Both directions simultaneously
Received data is compared to the sent data to ensure data integrity.
Each data chunk sent is randomly generated.

Hardware Setup
This test requires the following hardware:
- IF820 connected to PC via USB
- P_UART_TXD, P_UART_RXD, P_UART_RTS, P_UART_CTS, BT_UART_TXD, BT_UART_RXD, BT_UART_RTS,
  and BT_UART_CTS jumpers are removed from the DVK.
- Connect RP2040 side of P_UART_TXD to BT_UART_RXD, P_UART_RXD to BT_UART_TXD,
  P_UART_RTS to BT_UART_CTS, and P_UART_CTS to BT_UART_RTS.
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

DIR_PUART_TO_HCI = 0
DIR_HCI_TO_PUART = 1


def send_receive_data(board: If820Board, direction: int):
    tx_name = "P_UART"
    tx_uart = board.p_uart
    rx_uart = board.hci_uart
    if direction == DIR_PUART_TO_HCI:
        logging.info("Sending data from P_UART to HCI_UART")
    else:
        tx_name = "HCI_UART"
        tx_uart = board.hci_uart
        rx_uart = board.p_uart
        logging.info("Sending data from HCI_UART to P_UART")
    rx_uart.clear_rx_queue()

    data_sent = []
    data_received = []
    rx_done_event = threading.Event()
    tx_done_event = threading.Event()
    tx_start_time = time.time()
    rx_thread = threading.Thread(target=lambda: __receive_data_thread(board,
                                                                      direction,
                                                                      data_sent,
                                                                      data_received,
                                                                      rx_done_event,
                                                                      tx_start_time,
                                                                      tx_done_event),
                                 daemon=True)
    rx_thread.start()
    logging.info(
        f"{tx_name} start sending data for at least {THROUGHPUT_TEST_TIMEOUT_SECS} seconds...")
    packet_num = 0
    while (time.time() - tx_start_time) < THROUGHPUT_TEST_TIMEOUT_SECS:
        send = bytearray(''.join(random.choices(string.ascii_letters +
                                                string.digits, k=int(SEND_DATA_CHUNK_LEN))), 'utf-8')
        # Add a packet header that contains the packet number
        # This is useful for identifying the packet when looking at UART logic traces
        header = packet_num.to_bytes(4, byteorder='little')
        send[:4] = header
        send = bytes(send)
        tx_uart.send(send)
        data_sent.extend(list(send))
        packet_num += 1

    logging.info(
        f"{tx_name} sent {len(data_sent)} bytes, Wait for data to be received...")
    tx_done_event.set()
    if not rx_done_event.wait(THROUGHPUT_TEST_TIMEOUT_SECS * 2):
        logging.error(f"{tx_name} timeout waiting for data to be received")


def __receive_data_thread(board: If820Board,
                          direction: int,
                          data_sent: list,
                          data_received: list,
                          rx_done_event: threading.Event,
                          tx_start_time: float,
                          tx_done_event: threading.Event):
    rx_name = "P_UART"
    rx_uart = board.p_uart
    if direction == DIR_PUART_TO_HCI:
        rx_uart = board.hci_uart
        rx_name = "HCI_UART"
    last_rx_time = time.time()
    logging.info(f"{rx_name} receiving data...")
    data_received = []
    while time.time() - last_rx_time < RX_TIMEOUT_SECS or len(data_received) <= 0 or not tx_done_event.is_set():
        rx_data = rx_uart.read()
        if len(rx_data) > 0:
            last_rx_time = time.time()
            data_received.extend(list(rx_data))
    logging.info(
        f"{rx_name} all data received! Received {len(data_received)} bytes")
    if data_received != data_sent:
        logging.error(
            f"\r\n\r\n{rx_name} data received does not match data sent!\r\n")
        for index, (txd, rxd) in enumerate(zip(data_sent, data_received)):
            if txd != rxd:
                print(
                    f"{rx_name} RX mismatch at index: {index} (packet {hex(int(index/SEND_DATA_CHUNK_LEN))}), val: {hex(rxd)}")
                print(
                    f"{rx_name} tx[{index}]: {hex(data_sent[index])}, tx[{index + 1}]: {hex(data_sent[index + 1])}, tx[{index + 2}]: {hex(data_sent[index + 2])}")
                print(
                    f"{rx_name} rx[{index}]: {hex(data_received[index])}, rx[{index + 1}]: {hex(data_received[index + 1])}")
                data_received.insert(index, data_sent[index])
                # Just print the first mismatch
                break
    bytes_per_sec = len(data_received) / (last_rx_time - tx_start_time)
    logging.info(
        f"{rx_name} RX total time: {last_rx_time - tx_start_time:.1f} Throughput: {bytes_per_sec:.2f} Bps ({bytes_per_sec * 8:.2f} bps)")
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
    board.open_hci_uart_raw(baud)
    send_receive_data(board, DIR_PUART_TO_HCI)
    send_receive_data(board, DIR_HCI_TO_PUART)
    logging.info("Running bidirectional throughput test...")
    dir1_thread = threading.Thread(target=lambda: send_receive_data(board, DIR_PUART_TO_HCI),
                                   daemon=False)
    dir2_thread = threading.Thread(target=lambda: send_receive_data(board, DIR_HCI_TO_PUART),
                                   daemon=False)
    dir1_thread.start()
    dir2_thread.start()


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

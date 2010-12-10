#include <cc1100_ng.h>
#include <cc1100-internal.h>
#include <cc1100-config.h>
#include <cc1100-defaultSettings.h>
#include <cc1100_spi.h>
#include <cc1100-reg.h>

#include <hwtimer.h>
#include <msg.h>
#include <transceiver.h>

#include <cpu-conf.h>
#include <board.h>

static uint8_t receive_packet_variable(uint8_t *rxBuffer, uint8_t length);
static uint8_t receive_packet(uint8_t *rxBuffer, uint8_t length);

rx_buffer_t cc1100_rx_buffer[RX_BUF_SIZE];		    ///< RX buffer
volatile uint8_t rx_buffer_next;	    ///< Next packet in RX queue

void cc1100_rx_handler(void) {
    uint8_t res = 0;

	// Possible packet received, RX -> IDLE (0.1 us)
	rflags.CAA      = 0;
	rflags.MAN_WOR  = 0;
	cc1100_statistic.packets_in++;

	res = receive_packet((uint8_t*)&(cc1100_rx_buffer[rx_buffer_next].packet), sizeof(cc1100_packet_t));
	if (res) {
        // If we are sending a burst, don't accept packets.
		// Only ACKs are processed (for stopping the burst).
		// Same if state machine is in TX lock.
		if (radio_state == RADIO_SEND_BURST || rflags.TX)
		{
			cc1100_statistic.packets_in_while_tx++;
			return;
		}
        cc1100_rx_buffer[rx_buffer_next].rssi = rflags._RSSI;
        cc1100_rx_buffer[rx_buffer_next].lqi = rflags._LQI;
		cc1100_strobe(CC1100_SFRX);		// ...for flushing the RX FIFO

		// Valid packet. After a wake-up, the radio should be in IDLE.
		// So put CC1100 to RX for WOR_TIMEOUT (have to manually put
		// the radio back to sleep/WOR).
		//cc1100_spi_write_reg(CC1100_MCSM0, 0x08);	// Turn off FS-Autocal
		cc1100_write_reg(CC1100_MCSM2, 0x07);	// Configure RX_TIME (until end of packet)
        cc1100_strobe(CC1100_SRX);
        hwtimer_wait(IDLE_TO_RX_TIME);
        radio_state = RADIO_RX;
        
        /* notify transceiver thread if any */
        if (transceiver_pid) {
            msg m;  
            m.type = (uint16_t) RCV_PKT_CC1100;
            m.content.value = rx_buffer_next;
            msg_send_int(&m, transceiver_pid);
        }

        /* shift to next buffer element */
        if (++rx_buffer_next == RX_BUF_SIZE) {
            rx_buffer_next = 0;
        }
        return;
    }
	else
	{
		// No ACK received so TOF is unpredictable
		rflags.TOF = 0;

		// CRC false or RX buffer full -> clear RX FIFO in both cases
		cc1100_strobe(CC1100_SIDLE);	// Switch to IDLE (should already be)...
		cc1100_strobe(CC1100_SFRX);		// ...for flushing the RX FIFO

		// If packet interrupted this nodes send call,
		// don't change anything after this point.
		if (radio_state == RADIO_AIR_FREE_WAITING)
		{
			cc1100_strobe(CC1100_SRX);
			hwtimer_wait(IDLE_TO_RX_TIME);
			return;
		}
		// If currently sending, exit here (don't go to RX/WOR)
		if (radio_state == RADIO_SEND_BURST)
		{
			cc1100_statistic.packets_in_while_tx++;
			return;
		}

		// No valid packet, so go back to RX/WOR as soon as possible
		cc1100_switch_to_rx();
	}
}


static uint8_t receive_packet_variable(uint8_t *rxBuffer, uint8_t length) {
	uint8_t status[2];
	uint8_t packetLength = 0;

	/* Any bytes available in RX FIFO? */
	if ((cc1100_read_status(CC1100_RXBYTES) & BYTES_IN_RXFIFO)) {
        //LED_GREEN_TOGGLE;
        //LED_RED_TOGGLE;
		// Read length byte (first byte in RX FIFO)
        packetLength = cc1100_read_reg(CC1100_RXFIFO);
		// Read data from RX FIFO and store in rxBuffer
        if (packetLength <= length)
		{
			// Put length byte at first position in RX Buffer
			rxBuffer[0] = packetLength;

			// Read the rest of the packet
            // TODO: Offset + 2 here for cc430
			cc1100_readburst_reg(CC1100_RXFIFO, (char*)rxBuffer+1, packetLength);

            // Read the 2 appended status bytes (status[0] = RSSI, status[1] = LQI)
			cc1100_readburst_reg(CC1100_RXFIFO, (char*)status, 2);

			// Store RSSI value of packet
			rflags._RSSI = status[I_RSSI];

			// MSB of LQI is the CRC_OK bit
			rflags.CRC = (status[I_LQI] & CRC_OK) >> 7;
			if (!rflags.CRC) {
                cc1100_statistic.packets_in_crc_fail++;
            }

			// Bit 0-6 of LQI indicates the link quality (LQI)
			rflags._LQI = status[I_LQI] & LQI_EST;

			return rflags.CRC;
        }
        /* too many bytes in FIFO */
		else {
			// RX FIFO get automatically flushed if return value is false
            return 0;
        }
	}
    /* no bytes in RX FIFO */
	else {
        //LED_RED_TOGGLE;
		// RX FIFO get automatically flushed if return value is false
		return 0;
	}
}

static uint8_t receive_packet(uint8_t *rxBuffer, uint8_t length) {
	uint8_t pkt_len_cfg = cc1100_read_reg(CC1100_PKTCTRL0) & PKT_LENGTH_CONFIG;
	if (pkt_len_cfg == VARIABLE_PKTLEN)
	{
		return receive_packet_variable(rxBuffer, length);
	}
	// Fixed packet length not supported.
	// RX FIFO get automatically flushed if return value is false
	return 0;
}


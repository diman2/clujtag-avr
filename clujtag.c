#include "USBtoSerial.h"
#include "defines.h"
#include "jtag_commands.h"

/** LUFA CDC Class driver interface configuration and state information. This structure is
 *  passed to all CDC Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another.
 */
USB_ClassInfo_CDC_Device_t VirtualSerial_CDC_Interface =
	{
		.Config =
			{
				.ControlInterfaceNumber         = INTERFACE_ID_CDC_CCI,
				.DataINEndpoint                 =
					{
						.Address                = CDC_TX_EPADDR,
						.Size                   = CDC_TXRX_EPSIZE,
						.Banks                  = 1,
					},
				.DataOUTEndpoint                =
					{
						.Address                = CDC_RX_EPADDR,
						.Size                   = CDC_TXRX_EPSIZE,
						.Banks                  = 1,
					},
				.NotificationEndpoint           =
					{
						.Address                = CDC_NOTIFICATION_EPADDR,
						.Size                   = CDC_NOTIFICATION_EPSIZE,
						.Banks                  = 1,
					},
			},
	};

// blocking byte receiving
uint8_t get_usb_byte(void)
{
	int16_t ReceivedByte;
	do
	{
		ReceivedByte = CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);
		CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
		USB_USBTask();
	}
	while (ReceivedByte < 0);
	return (uint8_t) ReceivedByte;
}

/** Main program entry point. This routine contains the overall program flow, including initial
 *  setup of all components and the main program loop.
 */

void jtag_shutdown(void)
{
	PORT &= ~((1<<PIN_TMS) | (1<<PIN_TCK) | (1<<PIN_TDO) | (1<<PIN_TDI));
	PORT_DDR &= ~((1<<PIN_TMS) | (1<<PIN_TCK) | (1<<PIN_TDO) | (1<<PIN_TDI));
}

void jtag_setup(void)
{
	PORT &= ~((1<<PIN_TMS) | (1<<PIN_TCK) | (1<<PIN_TDO) | (1<<PIN_TDI));
	PORT_DDR |= (1<<PIN_TMS) | (1<<PIN_TCK) | (1<<PIN_TDI);
	PORT_DDR &= ~(1<<PIN_TDO);
}

int main(void)
{
	int initialized = 0;
	jtag_shutdown();
#ifdef PIN_LED
#ifdef LED_PORT_DDR
	LED_PORT_DDR |= 1<<PIN_LED;
	LED_PORT &= ~(1<<PIN_LED);
#endif
#endif

	SetupHardware();
	LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
	GlobalInterruptEnable();

	for (;;)
	{
		uint8_t command = get_usb_byte();
		uint8_t data, data2, ok, v, n;		
#ifdef PIN_LED
#ifdef LED_PORT
		if (initialized)
			LED_PORT ^= 1<<PIN_LED;
#endif
#endif
	
		switch (command)
		{
			case JTAG_SETUP:
				jtag_setup();
				initialized = 1;
#ifdef PIN_LED
#ifdef LED_PORT
				LED_PORT |= 1<<PIN_LED;
#endif
#endif
				break;

			case JTAG_SHUTDOWN:
				jtag_shutdown();
				initialized = 0;
#ifdef PIN_LED
#ifdef LED_PORT
				LED_PORT &= ~(1<<PIN_LED);
#endif
#endif
				break;
					
			/*
			case JTAG_SET_TMS:
				data = get_usb_byte();
				if (data == 0xff) continue;
				if (data) 
					PORT |= (1<<PIN_TMS);
				else
					PORT &= ~(1<<PIN_TMS);
				break;

			case JTAG_SET_TCK:
				data = get_usb_byte();
				if (data == 0xff) continue;
				if (data) 
					PORT |= (1<<PIN_TCK);
				else
					PORT &= ~(1<<PIN_TCK);
				break;
					
			case JTAG_SET_TDI:
				data = get_usb_byte();
				if (data == 0xff) continue;
				if (data)
					PORT |= (1<<PIN_TDI);
				else
					PORT &= ~(1<<PIN_TDI);
				break;
					
			case JTAG_GET_TDO:
				CDC_Device_SendByte(&VirtualSerial_CDC_Interface, (PORT_PIN >> PIN_TDO) & 1);	
				break;
			*/
					
			case JTAG_PULSE_TCK:
				data = get_usb_byte();
				if (data == 0xff) continue;
				if (data&1) 
					PORT |= (1<<PIN_TMS);
				else
					PORT &= ~(1<<PIN_TMS);
				if (data&(1<<1))
					PORT |= (1<<PIN_TDI);
				else
					PORT &= ~(1<<PIN_TDI);
				PORT &= ~(1<<PIN_TCK);
				_delay_us(1);
				PORT |= 1<<PIN_TCK;
				CDC_Device_SendByte(&VirtualSerial_CDC_Interface, (PORT_PIN >> PIN_TDO) & 1);
				break;
					
			case JTAG_PULSE_SCK:
				PORT &= ~(1<<PIN_TCK);
				_delay_us(1);
				PORT |= 1<<PIN_TCK;
				break;

			case JTAG_PULSE_TCK_DELAY:
				data = get_usb_byte();
				if (data == 0xff) continue;
				data2 = get_usb_byte();
				if (data2 == 0xff) continue;
				if (data) 
					PORT |= (1<<PIN_TMS);
				else
					PORT &= ~(1<<PIN_TMS);
				for (n = 0; n < data2; n++)
				{
					PORT &= ~(1<<PIN_TCK);
					_delay_us(1);
					PORT |= 1<<PIN_TCK;
					_delay_us(1);
				}
				break;
					
			case JTAG_PULSE_TCK_MULTI:
				data = get_usb_byte();
				if (data == 0xff) continue;
				ok = 1;
				v = 0;
				for (n = 0; n < data; n++)
				{
					v = get_usb_byte();
					if (v == 0xff) break;
					if (v&1) 
						PORT |= (1<<PIN_TMS);
					else
						PORT &= ~(1<<PIN_TMS);
					if (v&(1<<1))
					{
						if (v&(1<<2))
							PORT |= (1<<PIN_TDI);
						else
							PORT &= ~(1<<PIN_TDI);
					}
					PORT &= ~(1<<PIN_TCK);
					_delay_us(1);
					PORT |= 1<<PIN_TCK;
					_delay_us(1);
					if (v&(1<<3))
					{
						if (((v >> 4)&1) != ((PORT_PIN >> PIN_TDO) & 1))
							ok = 0;
					}
				}
				if (v == 0xff) continue;
				CDC_Device_SendByte(&VirtualSerial_CDC_Interface, ok);
				break;
		}		
		
	
		CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
		USB_USBTask();
	}
}

/** Configures the board hardware and chip peripherals for the demo's functionality. */
void SetupHardware(void)
{
#if (ARCH == ARCH_AVR8)
	/* Disable watchdog if enabled by bootloader/fuses */
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	/* Disable clock division */
	clock_prescale_set(clock_div_1);
#endif

	/* Hardware Initialization */
	LEDs_Init();
	USB_Init();
}

/** Event handler for the library USB Connection event. */
void EVENT_USB_Device_Connect(void)
{
	LEDs_SetAllLEDs(LEDMASK_USB_ENUMERATING);
}

/** Event handler for the library USB Disconnection event. */
void EVENT_USB_Device_Disconnect(void)
{
	LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
}

/** Event handler for the library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	bool ConfigSuccess = true;

	ConfigSuccess &= CDC_Device_ConfigureEndpoints(&VirtualSerial_CDC_Interface);

	LEDs_SetAllLEDs(ConfigSuccess ? LEDMASK_USB_READY : LEDMASK_USB_ERROR);
}

/** Event handler for the library USB Control Request reception event. */
void EVENT_USB_Device_ControlRequest(void)
{
	CDC_Device_ProcessControlRequest(&VirtualSerial_CDC_Interface);
}

/** Event handler for the CDC Class driver Line Encoding Changed event.
 *
 *  \param[in] CDCInterfaceInfo  Pointer to the CDC class interface configuration structure being referenced
 */
void EVENT_CDC_Device_LineEncodingChanged(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo)
{
}


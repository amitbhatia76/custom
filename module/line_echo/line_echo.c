
/*****************************************************************************/
/*                              Legal                                        */
/*****************************************************************************/

/*
** Copyright ©2015-2017. Lantronix, Inc. All Rights Reserved.
** By using this software, you are agreeing to the terms of the Software
** Development Kit (SDK) License Agreement included in the distribution package
** for this software (the “License Agreement”).
** Under the License Agreement, this software may be used solely to create
** custom applications for use on the Lantronix xPico Wi-Fi product.
** THIS SOFTWARE AND ANY ACCOMPANYING DOCUMENTATION IS PROVIDED "AS IS".
** LANTRONIX SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED
** TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, NON-INFRINGEMENT AND FITNESS
** FOR A PARTICULAR PURPOSE.
** LANTRONIX HAS NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES,
** ENHANCEMENTS, OR MODIFICATIONS TO THIS SOFTWARE.
** IN NO EVENT SHALL LANTRONIX BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
** SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST PROFITS,
** ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF
** LANTRONIX HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*****************************************************************************/
/*                           Documentation                                   */
/*****************************************************************************/

/*!
** \addtogroup example
** @{
*/

/*!
** \defgroup line_echo line_echo
** @{
**
** The \b line_echo module implements a rudimentary "Line Protocol".
** When this protocol is chosen by a Line, it echoes back to the Line
** whatever it receives on the Line.
**
** Build it from project "echoDemo".
*/

//#include "line_echo.h"

//#include "wifi_scan.h"

//#include "usb_impl.h"

/*****************************************************************************/
/*                             Includes                                      */
/*****************************************************************************/
#include <string.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


#include "line_echo_module_defs.h" /* Automatically generated by make. */
#include "ltrx_network.h" /* Delivered with SDK. */
#include "ltrx_compile_defines.h" /* Delivered with SDK. */
#include "ltrx_line.h" /* Delivered with SDK. */
#include "ltrx_stream.h" /* Delivered with SDK. */
#include "ltrx_tlog.h" /* Delivered with SDK. */

// Configurable PIN
#include "ltrx_cpm.h" /* Delivered with SDK. */

// SPI
#include "ltrx_spi.h" /* Delivered with SDK. */



#include "CP2120.h"
//#include <i2c_mux.h>
/*****************************************************************************/
/*                              Defines                                      */
/*****************************************************************************/

//#define INCLUDE_SPI_CODE

#define CLI_SCAN_LINE_LENGTH 77

#define NETWORK_NAME_LENGTH 32

#define MAC_ADDRESS_LENGTH 6

/* SPI */
#ifdef INCLUDE_SPI_CODE
#define MAX_TRANSFER_LENGTH 32
#endif 




/*****************************************************************************/
/*                         Local Constants                                   */
/*****************************************************************************/

static const uint32_t s_delayTimeInMilliseconds = 20000;

//static struct ltrx_trigger spiTrigger;

enum thread_type {
	LineEcho,
	SPI,
	spiIntHandler
};  
/*****************************************************************************/
/*                             Structs                                       */
/*****************************************************************************/


typedef struct threadNode_t threadNode;



struct threadNode_t {
	void *threadInfo;
	enum thread_type t;
	threadNode *n;
};

struct thread_info
{
    uint32_t zeroBasedIndex;
    bool isRunning;
    struct ltrx_trigger eventTrigger;
    struct output_stream_to_uart ostu;
    struct input_stream_from_uart isfu;
	struct output_stream_to_file ostusb;
};

/*****************************************************************************/
/*                             Structs                                       */
/*****************************************************************************/

struct output_stream_for_capture_scan
{
    bool passedHeaderDashedLine;
    bool scanCompleted;
    char scanLine[CLI_SCAN_LINE_LENGTH + 1];
    struct output_stream outStream;
	struct thread_info *tid;
};


/*****************************************************************************/
/*                            Prototypes                                     */
/*****************************************************************************/

bool StartLineProtocol(uint16_t zeroBasedIndex);

void StopLineProtocol(uint16_t zeroBasedIndex);

void signal_spi_trigger(void);

/*****************************************************************************/
/*                         Local Constants                                   */
/*****************************************************************************/

static const struct ltrx_line_protocol s_lineProtocol =
{
    .protocolName = "Echo",
    .helpHtml = "SDK example.",
    .startProtocol = StartLineProtocol,
    .stopProtocol = StopLineProtocol
};

#ifdef INCLUDE_SPI_CODE
static const struct ltrx_spi_protocol s_spiProtocol = {
	.protocolName  = "SPI_ADC",
    .helpHtml = "SDK xPico IPS.",
	.startProtocol = StartSpiLog,
	.stopProtocol  = StopSpiLog
};
#endif

/*****************************************************************************/
/*                         Local Variables                                   */
/*****************************************************************************/

static struct thread_info *s_threadInfo[MAX_LOGICAL_SERIAL_LINES];
static struct ltrx_thread *s_threadForLine[MAX_LOGICAL_SERIAL_LINES];

/* SPI */
#ifdef INCLUDE_SPI_CODE
static struct ltrx_thread *s_threadForSpi[MAX_SPI_EXTERNAL];
static struct ltrx_thread *s_threadForSpiInt[MAX_SPI_EXTERNAL];
#endif



/*****************************************************************************/
/*                          CLI Display                                      */
/*****************************************************************************/

inline void display_menu(struct thread_info *ti) ;

/*****************************************************************************/
/*                               Code                                        */
/*****************************************************************************/
#define INCLUDE_WIFI_SCAN 1
#define INCLUDE_SPI_TRANSFER 0

#if INCLUDE_WIFI_SCAN

static void processScanResults(
    const char *ssid,
    uint8_t *bssid,
    uint8_t channel,
    int rssi,
    const char *securitySuite,
	struct thread_info *tid
)
{
	char mybuf[512];
    TLOG(TLOG_SEVERITY_LEVEL__DEBUG, "SSID: %s", ssid);
    TLOG(
        TLOG_SEVERITY_LEVEL__DEBUG,
        "  BSSID: %02X:%02X:%02X:%02X:%02X:%02X",
        bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]
    );
    TLOG(TLOG_SEVERITY_LEVEL__DEBUG, "  Channel: %u", channel);
    TLOG(TLOG_SEVERITY_LEVEL__DEBUG, "  RSSI: %d", rssi);
    TLOG(TLOG_SEVERITY_LEVEL__DEBUG, "  Security Suite: %s", securitySuite);
	
	snprintf(mybuf, 512,
#if 1
	"%s\t, %02X:%02X:%02X:%02X:%02X:%02X\t, %u\t, %04d, %s\t",
#else	
//	"SSID: %s, BSSID: %02X:%02X:%02X:%02X:%02X:%02X Channel: %u, RSSI: %d, Security Suite: %s",
#endif
	ssid,
	bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],
	channel,
	rssi,
	securitySuite
	);

	ltrx_output_stream_write_line(&tid->ostu.outStream, mybuf);
}

static void parseScanLine(const char *scanLine, struct thread_info *tid)
{
    char ssid[NETWORK_NAME_LENGTH + 1] = "";
    uint8_t bssid[MAC_ADDRESS_LENGTH];
    uint8_t channel;
    int rssi;
    char securitySuite[15];
    sscanf(
        scanLine,
        "%32c %hhx:%hhx:%hhx:%hhx:%hhx:%hhx %hhu %d dBm %14s",
        ssid,
        &bssid[0],
        &bssid[1],
        &bssid[2],
        &bssid[3],
        &bssid[4],
        &bssid[5],
        &channel,
        &rssi,
        securitySuite
    );
    /* Trim trailing blanks from ssid. */
    size_t ssidLength;
    for(
        ssidLength = strlen(ssid);
        ssidLength > 0 && ssid[ssidLength - 1] == ' ';
        --ssidLength
    )
    {
        ssid[ssidLength - 1] = 0;
    }
    processScanResults(ssid, bssid, channel, rssi, securitySuite, tid);
}

static bool writeData(
    struct output_stream *outStream,
    const char *data, size_t length
)
{
    struct output_stream_for_capture_scan *osfcs;
    ASSIGN_STRUCT_POINTER_FROM_MEMBER_POINTER(
        osfcs, struct output_stream_for_capture_scan, outStream, outStream
    );
    size_t scannedLength = strlen(osfcs->scanLine);
    size_t writeLength = MINIMUM(length, CLI_SCAN_LINE_LENGTH - scannedLength);
    memcpy(osfcs->scanLine + scannedLength, data, writeLength);
    osfcs->scanLine[scannedLength + writeLength] = 0;
    ltrx_thread_yield();
    return true;
}

static bool writeNewline(struct output_stream *outStream)
{
    struct output_stream_for_capture_scan *osfcs;
    ASSIGN_STRUCT_POINTER_FROM_MEMBER_POINTER(
        osfcs, struct output_stream_for_capture_scan, outStream, outStream
    );
    if(osfcs->passedHeaderDashedLine)
    {
        if(! osfcs->scanCompleted)
        {
            if(osfcs->scanLine[0])
            {
                parseScanLine(osfcs->scanLine, osfcs->tid);
            }
            else
            {
                osfcs->scanCompleted = true;
            }
        }
    }
    else
    {
        size_t scannedLength = strlen(osfcs->scanLine);
        if(
            scannedLength >= NETWORK_NAME_LENGTH &&
            osfcs->scanLine[scannedLength - 1] == '-'
        )
        {
            osfcs->passedHeaderDashedLine = true;
        }
    }
    osfcs->scanLine[0] = 0;
    ltrx_thread_yield();
    return true;
}

static bool flushData(struct output_stream *outStream)
{
    (void)outStream;
    return true;
}

static bool outputClose(struct output_stream *outStream)
{
    (void)outStream;
    return true;
}
#endif






#ifdef INCLUDE_SPI_CODE


#ifdef INCLUDE_SPI_INT

/*
9.5.2.1.5 Data Ready (DRDY)
DRDY indicates when a new conversion result is ready for retrieval. When DRDY transitions from high to low,
new conversion data are ready. The DRDY signal remains low for the duration of the data frame and returns high
either when CS returns high (signaling the completion of the frame), or prior to new data being available. The
high-to-low DRDY transition occurs at the set data rate regardless of the CS state. If data are not completely
shifted out when new data are ready, the DRDY signal toggles high for a duration of 0.5 × tMOD and back low,
issuing the F_DRDY bit and indicating that the DOUT output shift register is not updated with the new conversion
result. Figure 58 shows an example of new data being ready before previous data are shifted out, causing the
new conversion result to be lost. The DRDY pin is always actively driven, even when CS is high.
*/
void my_spi_interrupt(void) {
	uint16_t roleIndex = ltrx_cp_role_index_get(s_role.name);
	bool cpIsActive = true;
	struct thread_info *ti;
	if(roleIndex >= CPM_ROLE_MAX)
	{
		TLOG(
			TLOG_SEVERITY_LEVEL__ALERT,
			"%s role was not registered.", s_role.name
		);
		return;
	}
	ltrx_cp_read(roleIndex, &cpIsActive);
	if (cpIsActive) {
		TLOG(TLOG_SEVERITY_LEVEL__INFORMATIONAL, " my_spi_interrupt: Pin High means no data ");	
		return;
	}
	if(recv_interrupt) 
	{
		TLOG(TLOG_SEVERITY_LEVEL__INFORMATIONAL, " my_spi_interrupt: handling interrupt, skip this ");		
		return;
	}
	recv_interrupt = true;
    ti = s_threadInfo[zIndex];
	TLOG(TLOG_SEVERITY_LEVEL__INFORMATIONAL, " my_spi_interrupt: Recieved %d", spi_done);
	if(spi_done) {	
		if(cpIsActive == false) 
		{
			uint8_t dummy_tx[36]={0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x4E,
					0xC3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x4E, 0xC3, 0};
			memcpy(txBuffer, dummy_tx,36);
			lsd.data_bytes=36;
			lsd.mosi_buf = (void *)txBuffer;
			lsd.miso_buf = rxBuffer;
			ltrx_spi_transfer(ti->zeroBasedIndex, &lsd);
			TLOG(TLOG_SEVERITY_LEVEL__INFORMATIONAL, " my_spi_interrupt: Sent \"%s\"", txBuffer);
			TLOG_HEXDUMP(TLOG_SEVERITY_LEVEL__INFORMATIONAL, rxBuffer, lsd.data_bytes, NULL);
 		} else {
			TLOG(TLOG_SEVERITY_LEVEL__INFORMATIONAL, " my_spi_interrupt: Pin High means no data ");	
		}			
	} 		
	recv_interrupt =false;
	return;
}
#endif


/*
Send command using Lantronix SPI API
*/
//Send a command to both chained A04 devices when the word length is 1 with each word having a size of 3 bytes.
static void sendCommand(struct thread_info *ti, int16_t command) {
    memcpy(txBuffer, &command,sizeof(int16_t));
    lsd.data_bytes=2;
	lsd.mosi_buf = (void *)txBuffer;
	lsd.miso_buf = rxBuffer;
    ltrx_spi_transfer(ti->zeroBasedIndex, &lsd);
    TLOG(TLOG_SEVERITY_LEVEL__INFORMATIONAL, "Sent \"%s\"", txBuffer);
    TLOG_HEXDUMP(TLOG_SEVERITY_LEVEL__INFORMATIONAL, rxBuffer, lsd.data_bytes, NULL);
    return ;
}


//Send a command to both chained A04 devices when the word length is 6 with each word having a size of 3-bytes.
static void sendCommand_6words(struct thread_info *ti, int16_t command) {
	TLOG(TLOG_SEVERITY_LEVEL__INFORMATIONAL,"SPI: sendCommand_6words((%x))", command);
    memcpy(txBuffer,&command,sizeof(int16_t));
    lsd.data_bytes=2;
	lsd.mosi_buf = (void *)txBuffer;
	lsd.miso_buf = rxBuffer;
    ltrx_spi_transfer(ti->zeroBasedIndex, &lsd);
    TLOG(TLOG_SEVERITY_LEVEL__INFORMATIONAL, "Sent \"%s\"", txBuffer);
    TLOG_HEXDUMP(TLOG_SEVERITY_LEVEL__INFORMATIONAL, rxBuffer, lsd.data_bytes, NULL);
    return ;
}

/*
 *  Snippet from Mail from Ralph, Sub: xPico SDK on Jan 2nd
 * Ok, once you get to the software, we can have a conversation on that.
 * The xPico Wi-Fi doesn’t have user interrupts. It’s a cooperative (non preemptive) multithreaded system.
 * We do have a way to register a callback function to when one of the CPs toggles. 
 * We also have some advanced triggering functionality so that you can set a thread to block waiting for a trigger. 
 * Then the callback function could message the trigger, which unblocks the thread that does the SPI transfer.
 * So in short, you can connect the ADC Data Ready to any available CP and use it as an “interrupt”.
 *
 * AB: For BIT, we can just wait in a finite loop and then declare Fail if no response recieved after sending a message?
 *
 * */
/*
 *
 * ADC Interface Configuration
 *  - M0 = IOVDD = Asynchronous interrupt mode
 *   - M1 = GND = 24 bit device word
 *   - M2 = GND = Hamming code word validation off
 *
 *   - ADC Frame - 12 bytes
 *      | status| ADC 1 | ADC 2 | ADC 3 |
 *      | 24bit | 24bit | 24bit | 24bit |
 * 	status is in the upper 16 bits in 24bit device word size
 *
 * */
static void spiLoop(struct thread_info *ti)
{

    while(ti->isRunning)
    {
		if (setup_spi == false) {
			uint16_t roleIndex = ltrx_cp_role_index_get(s_role.name);
			bool drdy;
			ltrx_cp_read(roleIndex, &drdy);
			/***    Wait for DRDY high to low   ***/
			if (drdy == 0) {
				while(drdy == 0) {
					ltrx_cp_read(roleIndex, &drdy);
				};
			}
			while(digitalRead(7) == 1);
		
			setup_spi = true;
	     	ltrx_thread_sleep(15000);
			//Send Unlock Command
			TLOG(TLOG_SEVERITY_LEVEL__INFORMATIONAL,"SPI: Send Unlock Command(%x)", ADS131A04_UNCLOCK_COMMAND);
			sendCommand(ti, ADS131A04_UNCLOCK_COMMAND);

			//Send Wake-up Command
			TLOG(TLOG_SEVERITY_LEVEL__INFORMATIONAL,"SPI: Send Wakeup Command(%x)", ADS131A04_WAKEUP_COMMAND);	
			sendCommand(ti, ADS131A04_WAKEUP_COMMAND);

			//Set Iclk divider to 2
			TLOG(TLOG_SEVERITY_LEVEL__INFORMATIONAL,"SPI: Set Iclk divider to 2 Command(%x)", WRITE_REGISTER_COMMAND(CLK1, CLK_DIV_2));	
			sendCommand(ti, WRITE_REGISTER_COMMAND(CLK1, CLK_DIV_2));


			//high resolution mode, NO negative charge pump, internal reference; BIT5 is set because the User Guide mentions to always write 1 when writing to this register.
			TLOG(TLOG_SEVERITY_LEVEL__INFORMATIONAL,"SPI: high resolution mode, NO negative charge pump, internal reference; BIT5 is set(%x)", (WRITE_REGISTER_COMMAND(A_SYS_CFG, HRM | INT_REFEN | BIT5)));
			sendCommand(ti, WRITE_REGISTER_COMMAND(A_SYS_CFG, HRM | INT_REFEN| BIT5));
	 
			//This is m	ainly to have everything have a fixed frame size from now on.
			TLOG(TLOG_SEVERITY_LEVEL__INFORMATIONAL,"SPI: fixed frame size (%x)", WRITE_REGISTER_COMMAND(D_SYS_CFG, HIZDLY_12ns | DNDLY_12ns | FIXED));
			sendCommand(ti, WRITE_REGISTER_COMMAND(D_SYS_CFG, HIZDLY_12ns | DNDLY_12ns | FIXED));
	 
			//Enable All ADCs
			TLOG(TLOG_SEVERITY_LEVEL__INFORMATIONAL,"SPI: Enable All ADCs(%x)", WRITE_REGISTER_COMMAND(ADC_ENA, ADC_ENA_ENABLE_ALL_CHANNELS));
			sendCommand_6words(ti, WRITE_REGISTER_COMMAND(ADC_ENA, ADC_ENA_ENABLE_ALL_CHANNELS));
	 
			//CRC is valid on all bits received and transmitted, 12 ns after assert DONE when LSB shifted out, CRC enabled
			//12 ns time that the device asserts Hi-Z on DOUT after the LSB of the data frame is shifted out.
			TLOG(TLOG_SEVERITY_LEVEL__INFORMATIONAL,"SPI: CRC is valid on all bits received and transmitted, 12 ns after assert DONE when LSB shifted out, CRC enabled (%x)",
						WRITE_REGISTER_COMMAND(D_SYS_CFG, HIZDLY_12ns | DNDLY_12ns | CRC_MODE | CRC_EN | FIXED));
			sendCommand_6words(ti, WRITE_REGISTER_COMMAND(D_SYS_CFG, HIZDLY_12ns | DNDLY_12ns | CRC_MODE | CRC_EN | FIXED ));
			
			//Send Null Command
			TLOG(TLOG_SEVERITY_LEVEL__INFORMATIONAL,"SPI: Send Null Command(%x)", ADS131A04_NULL_COMMAND);
			sendCommand(ti, ADS131A04_NULL_COMMAND);
			
			
			//Read Register command
			TLOG(TLOG_SEVERITY_LEVEL__INFORMATIONAL,"SPI: Read Register Commandd(%x)", READ_REGISTER_COMMAND(ID_MSB, 1) );
			sendCommand(ti, READ_REGISTER_COMMAND(ID_MSB, 1) );
			
			spi_done = true;
		} else {
			if (recv_interrupt) {
				recv_interrupt= false;
				uint8_t dummy_tx[36]={0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x4E,
						0xC3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x4E, 0xC3, 0};
				memcpy(txBuffer, dummy_tx,36);
				lsd.data_bytes=36;
				lsd.mosi_buf = (void *)txBuffer;
				lsd.miso_buf = rxBuffer;
				ltrx_spi_transfer(ti->zeroBasedIndex, &lsd);
				TLOG(TLOG_SEVERITY_LEVEL__INFORMATIONAL, " my_spi_interrupt: Sent \"%s\"", txBuffer);
				TLOG(TLOG_SEVERITY_LEVEL__INFORMATIONAL, " my_spi_interrupt: rxBuffer \"%s\"", txBuffer);
				TLOG_HEXDUMP(TLOG_SEVERITY_LEVEL__INFORMATIONAL, rxBuffer, lsd.data_bytes, NULL);		
			} else {
				TLOG(TLOG_SEVERITY_LEVEL__INFORMATIONAL, "Nothing to be done");
			}
			//TLOG(TLOG_SEVERITY_LEVEL__INFORMATIONAL, "SPI is set rxBuffer =  \"%s\"", rxBuffer);
		}

		ltrx_thread_sleep(15000);
    }
} // end spiLoop

static void spiThread(void *opaque)
{
    uint16_t zeroBasedIndex = (uint32_t)opaque;
    uint16_t spi = zeroBasedIndex + 1;
    bool loggedStartMessage = false;
    struct thread_info tiSpi =
    {
        .zeroBasedIndex = zeroBasedIndex,
        .isRunning = true
    };
    s_threadInfo[zeroBasedIndex] = &tiSpi;
    while(
        tiSpi.isRunning &&
        ! ltrx_spi_open(zeroBasedIndex, 1000)
    );
    if(tiSpi.isRunning)
    {
        TLOG(
            TLOG_SEVERITY_LEVEL__INFORMATIONAL,
            "Spi ()%s started on SPI %u",
            /*s_spiProtocol*/s_lineProtocol.protocolName,
            spi
        );
        loggedStartMessage = true;
    }
    if(tiSpi.isRunning)
    {
        spiLoop(&tiSpi);
    }
    if(loggedStartMessage)
    {
        TLOG(
            TLOG_SEVERITY_LEVEL__INFORMATIONAL,
            "%s stopped on SPI %u",
            /*s_spiProtocol*/s_lineProtocol.protocolName,
            spi
        );
    }
    ltrx_spi_close(zeroBasedIndex);
    s_threadInfo[zeroBasedIndex] = NULL;
    s_threadForSpi[zeroBasedIndex] = 0;
}
#endif // INCLUDE_SPI_CODE


/*****************************************************************************/
/*                               Code                                        */
/*****************************************************************************/

void line_echo_module_registration(void)
{
	/*
	Register your module. 
	Register your module in its initialization, before running any of your code. 
	Parameters
	[in]
	lmi
	Registration information. Refer to structure generated by make: &g_<module_name>ModuleInfo
	Where <module_name> is the name of your module. 	
	*/	
    ltrx_module_register(&g_line_echoModuleInfo);
    ltrx_line_register_protocol(&s_lineProtocol);
#ifdef INCLUDE_SPI_CODE	
    ltrx_spi_register_protocol(&s_spiProtocol);
#ifdef INCLUDE_SPI_INT	
	// should call register role over here
	ltrx_cp_register_role(&s_role);
#endif	

#ifdef INCLUDE_SPI_I2C_INT
	ltrx_cp_register_role(&s_role_i2c);
#endif
#endif
	
//	ltrx_cp_register_role(&s_role_pwr_dwn);
//	ltrx_cp_register_role(&s_role_select_adc);	
	
}



char current_choice = 0;
#define STR_BUF_SIZE 64
char buf[STR_BUF_SIZE];
int cnt = 0;

void line_echo_module_startup(void)
{
}

inline void display_menu(struct thread_info *ti) 
{
	
	ltrx_output_stream_write_line(&ti->ostu.outStream, "Please enter your choice");
	ltrx_output_stream_write_line(&ti->ostu.outStream, "1) Run all test cases ");
	ltrx_output_stream_write_line(&ti->ostu.outStream, "2) Test USB Rx and Tx ");
	ltrx_output_stream_write_line(&ti->ostu.outStream, "3) Test WiFi Rx and Tx ");
	ltrx_output_stream_write_line(&ti->ostu.outStream, "4) Read ADC Value");
	ltrx_output_stream_write_line(&ti->ostu.outStream, "5) Read ACCL Value");
	ltrx_output_stream_write_line(&ti->ostu.outStream, "6) Toggle LED");

	ltrx_output_stream_write_without_ending_line (&ti->ostu.outStream, "Please enter a choice : " );
}



static void echoData(struct thread_info *ti)
{	
	
    while(ltrx_input_stream_peek(&ti->isfu.inStream) >= 0)
    {
        char c = ltrx_input_stream_read(&ti->isfu.inStream);
        {
					if(c == '\r')
					{
					switch(current_choice) {
					case '1':
						ltrx_output_stream_write_line(&ti->ostu.outStream, "\r\n Running All testcases");
						break;
					case '2':
						ltrx_output_stream_write_line(&ti->ostu.outStream, "\r\n Running USB testcases");
						cnt =snprintf(buf, STR_BUF_SIZE, "Running USB Rx/Tx case 1=%d", current_choice);
						// create a FileDescriptor for Line USB CDC ACM
						//int fd;
						//fd = fopen("/dev/line_CDC_ACM",
						//ssize_t write(int fd, const void *buf, size_t count);
						//write(fd, buf, cnt);
						
						int filedesc = open("/dev/line_CDC_ACM", O_WRONLY | O_APPEND);
						if(filedesc < 0) {
							cnt =snprintf(buf, STR_BUF_SIZE, "\r\nCan't open USB\n");
							ltrx_output_stream_write_line(&ti->ostu.outStream, "\r\n Test Case Failed");
							//return ;
						} 
						//if(write(filedesc,"This will be output to testfile.txt\n", 36) != 36)
						if(write(filedesc,"This will be output to USB\r\n", 28) != 28)
						{
							cnt =snprintf(buf, STR_BUF_SIZE, "Error writing to USB CDC ACM");
							//write(2,"There was an error writing to testfile.txt\n");    // strictly not an error, it is allowable for fewer characters than requested to be written.
							//return ;
						}
											
						// file puts to send some data
						// file gets to recieve data on the USB
						// tess this with ubuntu
						// Print Pass or fail on the console
						ltrx_output_stream_write_line(&ti->ostu.outStream, buf );
						
					break;
					case '3': // READ
						cnt =snprintf(buf, STR_BUF_SIZE, "\r\n Your choice is %d. \r\n Running WiFi Rx/Tx case\r\n ", current_choice);
						ltrx_output_stream_write_line(&ti->ostu.outStream, buf );
						TLOG(TLOG_SEVERITY_LEVEL__DEBUG, "Running WiFi Rx/Tx case");
						//bool running = true;
						while(ltrx_ip_address_state(NETS_WLAN_START) == 0) {
							ltrx_thread_sleep(1000); /* wlan0 not up yet. */
						}
						struct input_stream_from_const_char isfcc;
						struct output_stream_for_capture_scan osfcs =
						{
							.outStream.writeData = writeData,
							.outStream.writeNewline = writeNewline,
							.outStream.flushData = flushData,
							.outStream.outputClose = outputClose,
							.tid = ti 
						};
						//struct output_stream_for_capture_scan osfcs =
						//{
						//	.outStream = ti->ostu.outStream
						//};
						TLOG(TLOG_SEVERITY_LEVEL__DEBUG, "Writing to input stream of constant char\n");
						ltrx_output_stream_write_line(&ti->ostu.outStream, "wlan scan\n" );
						if(ltrx_input_stream_init_from_const_char(&isfcc, "wlan scan\rexit\r"))
						{
							bool running = true;
							TLOG(TLOG_SEVERITY_LEVEL__DEBUG, "Starting scan.");
							ltrx_cli_command_loop(
								&isfcc.inStream, &osfcs.outStream, &running, true
							);
							TLOG(TLOG_SEVERITY_LEVEL__DEBUG, "Scan completed.");
						}						
						
						//ltrx_output_stream_write_line(&ti->ostu.outStream, " BEGIN: Trying to write to outSteam from osfcs  to ostu \r\n" );
						//ltrx_output_stream_write_line(&ti->ostu.outStream, osfcs.outStream->data );
						//ltrx_output_stream_write_line(&ti->ostu.outStream, "END : Trying to write to outSteam from osfcs  to ostu \r\n" );
						ltrx_input_stream_close(&isfcc.inStream);
						//ltrx_thread_sleep(s_delayTimeInMilliseconds);
						ltrx_thread_sleep(1000);
						ltrx_output_stream_write_line(&ti->ostu.outStream, " Done with wlan scan\n\n" );
					break;
					case '4': // READ_ADC_CHIPSET
						cnt =snprintf(buf, STR_BUF_SIZE, "Running ADC Test Case =%d", current_choice);
						ltrx_output_stream_write_line(&ti->ostu.outStream, buf );
						// Send Data to SPI Bridge and then 
						// Set the Trigger waiting for the return values
						TLOG(TLOG_SEVERITY_LEVEL__DEBUG, "Toggle CP1 to enable communication with the ADC chipset");
						TLOG(TLOG_SEVERITY_LEVEL__DEBUG, "Configure ADC chipset using SPI Interface/Commands");	
						TLOG(TLOG_SEVERITY_LEVEL__DEBUG, "Wait for toggle on Interrupt Pin after every command");
						TLOG(TLOG_SEVERITY_LEVEL__DEBUG, "Read Response from ADC Chipset");
						TLOG(TLOG_SEVERITY_LEVEL__DEBUG, "Done with ADC Testing");	
						//LTRX_TRIGGER_WAIT(&spiTrigger, 5000);
					case '5': // READ_UID_VALUE 
						cnt =snprintf(buf, STR_BUF_SIZE, "\r\n Test Case #%2d: Read UID Values  ", current_choice);
						ltrx_output_stream_write_line(&ti->ostu.outStream, buf );					
					break;
					default:
						cnt =snprintf(buf, STR_BUF_SIZE, "Invalid command c=%d", current_choice);
						ltrx_output_stream_write_line(&ti->ostu.outStream, buf );
				}
				
				(void) display_menu(ti);

				
                //ltrx_output_stream_write_line(&ti->ostu.outStream, "");
            }
            else
            {
                /* Echo. */
                ltrx_output_stream_write_binary(&ti->ostu.outStream, &c, 1);
				current_choice = c;

            }
        }
    }
}

static void lineLoop(struct thread_info *ti)
{
    ltrx_input_stream_init_from_uart(&ti->isfu, ti->zeroBasedIndex);
    ltrx_output_stream_init_to_uart(&ti->ostu, ti->zeroBasedIndex);
    ltrx_output_stream_write_line(&ti->ostu.outStream, "");
    ltrx_output_stream_write_line(
        &ti->ostu.outStream, s_lineProtocol.protocolName
    );
    ltrx_output_stream_write_line(&ti->ostu.outStream, "CVR Testing Ready");
	
	ltrx_output_stream_write_line(&ti->ostu.outStream, "Please enter your choice");
	ltrx_output_stream_write_line(&ti->ostu.outStream, "1) Run all test cases ");
	ltrx_output_stream_write_line(&ti->ostu.outStream, "2) Test USB Rx and Tx ");
	ltrx_output_stream_write_line(&ti->ostu.outStream, "3) Test WiFi Rx and Tx ");
	ltrx_output_stream_write_line(&ti->ostu.outStream, "4) Read ADC");
	ltrx_output_stream_write_line(&ti->ostu.outStream, "5) Toggle CP");
	
	ltrx_output_stream_write_without_ending_line (&ti->ostu.outStream, "Please enter a choice : " );
    while(ti->isRunning)
    {
        ltrx_trigger_clear(&ti->eventTrigger);
        if(
            ltrx_line_read_bytes_available(
                ti->zeroBasedIndex, &ti->eventTrigger
            )
        )
        {
            echoData(ti);
        }
        else
        {
            LTRX_TRIGGER_WAIT(&ti->eventTrigger, TIME_WAIT_FOREVER);
        }
    }
}

static void lineThread(void *opaque)
{
    uint16_t zeroBasedIndex = (uint32_t)opaque;
    uint16_t line = zeroBasedIndex + 1;
    bool loggedStartMessage = false;
    struct thread_info ti =
    {
        .zeroBasedIndex = zeroBasedIndex,
        .isRunning = true
    };
    if(! ltrx_trigger_create(&ti.eventTrigger, s_lineProtocol.protocolName))
    {
        return;
    }

#if 0	
    // Create spiTrigger
    if(! ltrx_trigger_create(&spiTrigger, "SpiTrigger"))
    {
        return;
    }
#endif	

    s_threadInfo[zeroBasedIndex] = &ti;
    while(
        ti.isRunning &&
        ! ltrx_line_open(zeroBasedIndex, 1000)
    );
    if(ti.isRunning)
    {
        TLOG(
            TLOG_SEVERITY_LEVEL__INFORMATIONAL,
            "%s started on Line %u",
            s_lineProtocol.protocolName,
            line
        );
        loggedStartMessage = true;
        ltrx_line_set_dtr(zeroBasedIndex, true);
        lineLoop(&ti);
    }
    if(loggedStartMessage)
    {
        TLOG(
            TLOG_SEVERITY_LEVEL__INFORMATIONAL,
            "%s stopped on Line %u",
            s_lineProtocol.protocolName,
            line
        );
    }
    ltrx_line_close(zeroBasedIndex);
    s_threadInfo[zeroBasedIndex] = NULL;
    ltrx_trigger_destroy(&ti.eventTrigger);
	
    //ltrx_trigger_destroy(&spiTrigger);
    
    s_threadForLine[zeroBasedIndex] = 0;
}

#if 0
void signal_spi_trigger(void) {
    //ltrx_trigger_signal(&spiTrigger); 
}


/* spiInterruptThread */
static void spiInterruptThread(void *opaque)
{
    (void)opaque;
    /* No role defined for the interruption thread 
        uint16_t roleIndex = ltrx_cp_role_index_get(s_role.name);
    */
	uint16_t spi2i2c_interruppt_cp1 = 1; // 1 or 45??
	uint16_t WM_PWRDWN_REQ_cp5 = 5; // 5 or 16??
    uint8_t i = 0;
	bool pwrDwn = false;
	bool interrupt = false;
	
    /* No role defined for the isr.
    if(roleIndex >= CPM_ROLE_MAX)
    {
        TLOG(
            TLOG_SEVERITY_LEVEL__ALERT,
            "%s role was not registered.", s_role.name
        );
        return;
    }
    TLOG(
        TLOG_SEVERITY_LEVEL__INFORMATIONAL,
        "%s role has logical index %u.", s_role.name, roleIndex
    );
	*/

    // Following code does the testing of the WM_PWRDWN_REQ on CP5 
    // Connect to a digital in to read the output of this pin    
    while(i < 10)
    {
        ltrx_cp_write(WM_PWRDWN_REQ_cp5, pwrDwn);
        pwrDwn = ! pwrDwn;
        ltrx_thread_sleep(s_delayTimeInMilliseconds);
		i++;
    }
     ltrx_cp_write(WM_PWRDWN_REQ_cp5, true);
    // In this loop thread keeps on the loop to check for the Interrup Trigger on the CP1
    // Once the
	while(true) {
		ltrx_cp_read(spi2i2c_interruppt_cp1, &interrupt);
		if (interrupt) {
            
			//set a global value
            // or call a global function
            signal_spi_trigger();
			ltrx_cp_write(WM_PWRDWN_REQ_cp5, true);
			TLOG(
				TLOG_SEVERITY_LEVEL__INFORMATIONAL,
					"Interrupt recieved on %d ",interrupt
			);
			ltrx_thread_sleep(s_delayTimeInMilliseconds);
			//set a global value
			ltrx_cp_write(WM_PWRDWN_REQ_cp5, false);
            
		}
		ltrx_thread_sleep(10);
	}
}

#endif
bool StartLineProtocol(uint16_t zeroBasedIndex)
{
    uint16_t line = zeroBasedIndex + 1;
  
	TLOG(TLOG_SEVERITY_LEVEL__INFORMATIONAL, "StartLineProtocol ");  
    if(s_threadInfo[zeroBasedIndex] || s_threadForLine[zeroBasedIndex])
    {
        TLOG(
            TLOG_SEVERITY_LEVEL__ERROR,
            "%s thread already running for Line %u",
            s_lineProtocol.protocolName,
            line
        );
        return false;
    }
    s_threadForLine[zeroBasedIndex] = ltrx_thread_create(
        s_lineProtocol.protocolName,
        lineThread,
        (void *)(uint32_t)zeroBasedIndex,
        5000
    );
    if(! s_threadForLine[zeroBasedIndex])
    {
        TLOG(
            TLOG_SEVERITY_LEVEL__ERROR,
            "Failed to create %s thread for Line %u",
            s_lineProtocol.protocolName,
            line
        );
        return false;
    }
  
#ifdef INCLUDE_SPI_CODE    
	// Create Thread for the SPI
    s_threadForSpi[zeroBasedIndex] = ltrx_thread_create(
        "SPI", /*s_spiProtocol.protocolName,*/
        spiThread,
        (void *)(uint32_t)zeroBasedIndex,
        3000
    );    
#endif // INCLUDE_SPI_CODE    

/*
    
	// Create a Thread for the interrupt 
    ltrx_thread_create(
        "SPI_2_I2C_INT",
        spiInterruptThread,
        NULL,
        STACK_SIZE_GREEN_FROM_MAX_OBSERVED_STACK_USED(247)
    );	
*/	

    return true;
}

void StopLineProtocol(uint16_t zeroBasedIndex)
{
    bool wasRunning = false;
    struct thread_info *ti;
	
    //struct thread_info *tiSpi;
    //ltrx_preemption_block();
    //tiSpi=s_threadInfo[zeroBasedIndex];
    //ltrx_preemption_unblock();   
    
    
    ltrx_preemption_block();
    ti = s_threadInfo[zeroBasedIndex];
    if(ti && ti->isRunning)
    {
        wasRunning = true;
        ti->isRunning = false;
        ltrx_trigger_signal(&ti->eventTrigger);
    }
    ltrx_preemption_unblock();
    if(wasRunning)
    {
        struct ltrx_thread *lt;
        uint32_t tm = ltrx_timemark();
        while(
            (lt = s_threadForLine[zeroBasedIndex]) != NULL &&
            lt != ltrx_thread_id() &&
            ltrx_elapsed_time_current_ms(tm) < 2000
        )
        {
            if(ltrx_elapsed_time_current_ms(tm) >= 500)
            {
                ltrx_line_purge(zeroBasedIndex);
            }
            ltrx_thread_sleep(100);
        }
    }
}

/*!
** @}
*/

/*!
** @}
*/

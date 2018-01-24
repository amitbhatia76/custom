
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

/*****************************************************************************/
/*                             Includes                                      */
/*****************************************************************************/
#include <string.h>
#include <stdio.h>

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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "CP2120.h"
//#include <i2c_mux.h>
/*****************************************************************************/
/*                              Defines                                      */
/*****************************************************************************/

#define INCLUDE_SPI_CODE

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
static struct ltrx_trigger spiTrigger;

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
	thread_type t;
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
static void spiLoop(struct thread_info *tiSpi)
{
    unsigned long count = 0;
    while(tiSpi->isRunning)
    {
        char txBuffer[MAX_TRANSFER_LENGTH];
        uint8_t rxBuffer[MAX_TRANSFER_LENGTH];
		
        struct ltrx_spi_descriptor lsd =
        {
            .data_bytes = snprintf(txBuffer, sizeof(txBuffer), "Hello IPS %08lx", ++count),
            .mosi_buf = (void *)txBuffer,
            .miso_buf = rxBuffer
        };
		
		// Reserve a 
		// Chip select the ADC line.
		
		// Initialize the ADC sensor
		
		// read from the ADC sensor.
		
		//
		
        ltrx_spi_transfer(tiSpi->zeroBasedIndex, &lsd);
        
		TLOG(TLOG_SEVERITY_LEVEL__INFORMATIONAL, "Sent \"%s\"", txBuffer);
        TLOG_HEXDUMP(TLOG_SEVERITY_LEVEL__INFORMATIONAL, rxBuffer, lsd.data_bytes, NULL);
        ltrx_thread_sleep(5000);
    }
}

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
    ltrx_module_register(&g_line_echoModuleInfo);
    ltrx_line_register_protocol(&s_lineProtocol);
}

char current_choice = 0;
#define STR_BUF_SIZE 64
char buf[STR_BUF_SIZE];
int cnt = 0;
void line_echo_module_startup(void)
{
}


#if 0
// SPI
static void spiLoop(struct thread_info *ti)
{
    unsigned long count = 0;
	bool spi_run = 1;
    while(spi_run)
    {
        char txBuffer[MAX_TRANSFER_LENGTH];
        uint8_t rxBuffer[MAX_TRANSFER_LENGTH];
		
        struct ltrx_spi_descriptor lsd =
        {
            .data_bytes = snprintf(txBuffer, sizeof(txBuffer), "ADS SPI %08lx", ++count),
            .mosi_buf = (void *)txBuffer,
            .miso_buf = rxBuffer
        };
				
        ltrx_spi_transfer(ti->zeroBasedIndex, &lsd);
        
	TLOG(TLOG_SEVERITY_LEVEL__INFORMATIONAL, "Sent \"%s\"", txBuffer);
        TLOG_HEXDUMP(TLOG_SEVERITY_LEVEL__INFORMATIONAL, rxBuffer, lsd.data_bytes, NULL);
        //ltrx_thread_sleep(5000);
	ltrx_thread_sleep(100);
	// Reserve a 
	// Configure ADC chipset

	// Configure continuous stream of data

	// Display the rxBuffer

	//
	spi_run = 0;
    }
}
#endif


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
						ltrx_output_stream_write_line(&ti->ostu.outStream, "\n Running All testcases");
						break;
					case '2':
						ltrx_output_stream_write_line(&ti->ostu.outStream, "\n Running USB testcases");
						cnt =snprintf(buf, STR_BUF_SIZE, "Running USB Rx/Tx case 1=%d", current_choice);
						// create a FileDescriptor for Line USB CDC ACM
						//int fd;
						//fd = fopen("/dev/line_CDC_ACM",
						//ssize_t write(int fd, const void *buf, size_t count);
						//write(fd, buf, cnt);
						
						int filedesc = open("/dev/line_CDC_ACM", O_WRONLY | O_APPEND);
						if(filedesc < 0) {
							cnt =snprintf(buf, STR_BUF_SIZE, "Can't open USB\n");
							//return ;
						}
						//if(write(filedesc,"This will be output to testfile.txt\n", 36) != 36)
						if(write(filedesc,"This will be output to USB\n", 27) != 27)
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
					break;
					default:
						cnt =snprintf(buf, STR_BUF_SIZE, "Invalid command c=%d", current_choice);
						ltrx_output_stream_write_line(&ti->ostu.outStream, buf );
				}
				ltrx_output_stream_write_line(&ti->ostu.outStream, "Please enter your choice");
				ltrx_output_stream_write_line(&ti->ostu.outStream, "1) Run all test cases ");
				ltrx_output_stream_write_line(&ti->ostu.outStream, "2) Test USB Rx and Tx ");
				ltrx_output_stream_write_line(&ti->ostu.outStream, "3) Test WiFi Rx and Tx ");
				ltrx_output_stream_write_line(&ti->ostu.outStream, "4) Read ADC Value");
				ltrx_output_stream_write_line(&ti->ostu.outStream, "5) Read ACCL Value");
				ltrx_output_stream_write_line(&ti->ostu.outStream, "6) Toggle LED");
				
				ltrx_output_stream_write_without_ending_line (&ti->ostu.outStream, "Please enter a choice : " );
				
				
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
    // Create spiTrigger
    if(! ltrx_trigger_create(&spiTrigger, "SpiTrigger"))
    {
        return;
    }
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
    ltrx_trigger_destroy(&spiTrigger);
    
    s_threadForLine[zeroBasedIndex] = 0;
}

void signal_spi_trigger(void) {
    ltrx_trigger_signal(&spiTrigger); 
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
    
	// Create a Thread for the interrupt 
    spInt=ltrx_thread_create(
        "SPI_2_I2C_INT",
        spiInterruptThread,
        NULL,
        STACK_SIZE_GREEN_FROM_MAX_OBSERVED_STACK_USED(247)
    );	
	

    return true;
}

void StopLineProtocol(uint16_t zeroBasedIndex)
{
    bool wasRunning = false;
    struct thread_info *ti;
    struct thread_info *tiSpi
	    
    ltrx_preemption_block();
    tiSpi=s_threadInfo[zeroBasedIndex];
    ltrx_preemption_unblock();   
    
    
    
    
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

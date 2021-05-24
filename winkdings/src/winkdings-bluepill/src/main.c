/* 
 * Based on https://github.com/hathach/tinyusb/tree/master/examples/device/webusb_serial
 *
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

/* Uses WebUSB with browser with WebUSB support (e.g Chrome).
 * After enumerated successfully, browser will pop-up notification
 * with URL to landing page, click on it to test
 *  - Click "Connect" and select device, When connected the on-board LED will litted up.
 *  - Any charters received from either webusb/Serial will be echo back to webusb and Serial
 * - On Linux/macOS, udev permission may need to be updated by
 *   - copying '/examples/device/99-tinyusb.rules' file to /etc/udev/rules.d/ then
 *   - run 'sudo udevadm control --reload-rules && sudo udevadm trigger'
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include "serialout.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 100 ms  : device not mounted
 * - 500 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum  {
  BLINK_NOT_MOUNTED = 100,
  BLINK_MOUNTED     = 500,
  BLINK_SUSPENDED   = 2500,

  BLINK_ALWAYS_ON   = UINT32_MAX,
  BLINK_ALWAYS_OFF  = 0
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

#define URL  "www.andreaskahler.com/winkdings/"

const tusb_desc_webusb_url_t desc_url =
{
  .bLength         = 3 + sizeof(URL) - 1,
  .bDescriptorType = 3, // WEBUSB URL type
  .bScheme         = 0, // 0: http, 1: https
  .url             = URL
};

static bool web_serial_connected = false;

uint8_t rxbuffer[1024];
uint32_t rxbuffer_pos = 0;

//------------- prototypes -------------//
void led_blinking_task(void);
void cdc_task(void);
void webserial_task(void);
void echo_all(uint8_t* buf, uint32_t count);
void echo_string(const char* buf, uint32_t count);
#define ECHO_STR(x) echo_string(x, sizeof(x));
int32_t hexchar2int(uint8_t c);
uint32_t header_bytecount(uint8_t* buf);

/*------------- MAIN -------------*/
int main(void)
{
  board_init();
  tusb_init();
  serial_init();

  while (1)
  {
    tud_task(); // tinyusb device task
    cdc_task();
    webserial_task();
    led_blinking_task();
  }

  return 0;
}

void handle_line(uint8_t* buffer, uint32_t count)
{
  echo_all(buffer, count);
  ECHO_STR("\nOK\n");

  serial_send(buffer, count);
  serial_send((uint8_t*)"\n", 1);
}

void on_line_read(uint32_t count)
{
  /* Expected format
   * #xxxx#ssssssssss\n
   * #                    -> header start
   *  xxxx                -> byte count c, given in hex, at least 1
   *      #               -> header end
   *       ssssssssss     -> payload, c bytes long
   *                 \n   -> closing newline
   */
  if (count > 0)
  {
    // debug: echo back
    //echo_all(rxbuffer, count);
    //ECHO_STR("\n");
    
    if (count < 7)
    {
      ECHO_STR("FAIL too short\n");
      return;
    }
    if (rxbuffer[0] != '#' || rxbuffer[5] != '#')
    {
      ECHO_STR("FAIL invalid format\n");
      return;
    }
    uint32_t header_bytes = header_bytecount(rxbuffer);
    if (header_bytes < 1)
    {
      ECHO_STR("FAIL invalid header byte count\n");
      return;
    }
    if (header_bytes+6 > count)
    {
      ECHO_STR("FAIL shorter than specified\n");
      return;
    }
    handle_line(rxbuffer+6, header_bytes);
  }
}

void webserial_task(void)
{
  if ( web_serial_connected )
  {
    if ( tud_vendor_available() )
    {
      uint32_t maxread = sizeof(rxbuffer) - rxbuffer_pos ;
      uint32_t count = tud_vendor_read(rxbuffer+rxbuffer_pos, maxread);
      for (uint32_t pos=rxbuffer_pos; pos<rxbuffer_pos+count; ++pos)
      {
        if (rxbuffer[pos] == '\n')
        {
          // have complete line
          on_line_read(pos);
          // reset
          rxbuffer_pos = 0;
          return;
        }
      }
      // no newline found
      rxbuffer_pos += count;
      if (rxbuffer_pos >= sizeof(rxbuffer))
      {
        // message exeeds buffer length
        tud_vendor_read(rxbuffer, sizeof(rxbuffer)); // try to read rest of message
        rxbuffer_pos = 0;
        ECHO_STR("FAIL too long\n");
        return;
      }
    }
  }
}




// send characters to both CDC and WebUSB
void echo_string(const char* buf, uint32_t count)
{
    echo_all((uint8_t*)buf, count);
}

void echo_all(uint8_t* buf, uint32_t count)
{
  // echo to web serial
  if ( web_serial_connected )
  {
    tud_vendor_write(buf, count);
  }
}
//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

//--------------------------------------------------------------------+
// WebUSB use vendor class
//--------------------------------------------------------------------+

// Invoked when a control transfer occurred on an interface of this class
// Driver response accordingly to the request and the transfer stage (setup/data/ack)
// return false to stall control endpoint (e.g unsupported request)
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request)
{
  // nothing to with DATA & ACK stage
  if (stage != CONTROL_STAGE_SETUP) return true;

  switch (request->bmRequestType_bit.type)
  {
    case TUSB_REQ_TYPE_VENDOR:
      switch (request->bRequest)
      {
        case VENDOR_REQUEST_WEBUSB:
          // match vendor request in BOS descriptor
          // Get landing page url
          return tud_control_xfer(rhport, request, (void*) &desc_url, desc_url.bLength);

        case VENDOR_REQUEST_MICROSOFT:
          if ( request->wIndex == 7 )
          {
            // Get Microsoft OS 2.0 compatible descriptor
            uint16_t total_len;
            memcpy(&total_len, desc_ms_os_20+8, 2);

            return tud_control_xfer(rhport, request, (void*) desc_ms_os_20, total_len);
          }
          else
          {
            return false;
          }

        default: break;
      }
    break;

    case TUSB_REQ_TYPE_CLASS:
      if (request->bRequest == 0x22)
      {
        // Webserial simulate the CDC_REQUEST_SET_CONTROL_LINE_STATE (0x22) to connect and disconnect.
        web_serial_connected = (request->wValue != 0);

        // Always lit LED if connected
        if ( web_serial_connected )
        {
          board_led_write(true);
          blink_interval_ms = BLINK_ALWAYS_ON;

          tud_vendor_write_str("\r\nWinkdings BluePill WebUSB device\r\n");
        }else
        {
          blink_interval_ms = BLINK_MOUNTED;
        }

        // response with status OK
        return tud_control_status(rhport, request);
      }
    break;

    default: break;
  }

  // stall unknown request
  return false;
}



//--------------------------------------------------------------------+
// USB CDC
//--------------------------------------------------------------------+
void cdc_task(void)
{
  // do nothing
}

// Invoked when cdc when line state changed e.g connected/disconnected
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
  (void) itf;

  // connected
  if ( dtr && rts )
  {
    // print initial message when connected
    tud_cdc_write_str("\r\nWinkdings BluePill WebUSB device (CDC)\r\n");
  }
}

// Invoked when CDC interface received data from host
void tud_cdc_rx_cb(uint8_t itf)
{
  (void) itf;
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
  static uint32_t start_ms = 0;
  static bool led_state = false;

  // Blink every interval ms
  if ( board_millis() - start_ms < blink_interval_ms) return; // not enough time
  start_ms += blink_interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}


//--------------------------------------------------------------------+
// HELPER
//--------------------------------------------------------------------+

int32_t hexchar2int(uint8_t c)
{
  if (c>='0' && c<='9')
    return c-'0';
  if (c>='A' && c<='F')
    return 10+c-'A';
  if (c>='a' && c<='f')
    return 10+c-'a';
  return -1;
}

uint32_t header_bytecount(uint8_t* buf)
{
  int32_t d1 = hexchar2int(buf[1]);
  int32_t d2 = hexchar2int(buf[2]);
  int32_t d3 = hexchar2int(buf[3]);
  int32_t d4 = hexchar2int(buf[4]);
  if (d1<0 || d2<0|| d3<0|| d4<0)
    return 0;
  return d4 + 16*d3 + 16*16*d2 + 16*16*16*d1;
}
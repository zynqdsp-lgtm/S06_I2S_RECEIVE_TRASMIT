
#include "platform.h"
#include "ring_buffer_lib.h"
#include "xaudioformatter.h"
#include "xi2srx.h"
#include "xi2stx.h"
#include "xil_cache.h"
#include "xil_printf.h"
#include "xinterrupt_wrap.h"
#include "xparameters.h"
#include <stdio.h>

#define AF_FS 32 /* kHz */
#define AF_MCLK (256 * AF_FS)

#define I2S_TX_FS 32 /* kHz */
#define I2S_TX_MCLK (256 * I2S_TX_FS)

#define I2S_RX_FS 32 /* kHz */
#define I2S_RX_MCLK (256 * I2S_RX_FS)

XAudioFormatterHwParams af_tx_hw_params;
XAudioFormatterHwParams af_rx_hw_params;
XAudioFormatter XAudioFormatterInst;
XI2s_Tx XI2sTxInst;
XI2s_Rx XI2sRxInst;

u32 MM2SAFIntrReceived;
u32 S2MMAFIntrReceived;

void XMM2SAFCallback(void *data);
void XS2MMAFCallback(void *data);

int32_t audio_tx_buf[4096];
int32_t audio_rx_buf[4096];

UINTPTR audio_tx_buf_addr = (UINTPTR)audio_tx_buf;
UINTPTR first_half_tx_audio_buf_addr = (UINTPTR)audio_tx_buf;
UINTPTR second_half_tx_audio_buf_addr = (UINTPTR)(audio_tx_buf + 2048);

UINTPTR audio_rx_buf_addr = (UINTPTR)audio_rx_buf;
UINTPTR first_half_rx_audio_buf_addr = (UINTPTR)audio_rx_buf;
UINTPTR second_half_rx_audio_buf_addr = (UINTPTR)(audio_rx_buf + 2048);

ring_buffer_t ring_buf_inst;

int32_t ring_buff[8192];

int32_t tx_ring_buf[2048];
int32_t rx_ring_buf[2048];

void init(void) {

  int Status;

  /********** XAudioFormatter Initialize **********/

  af_tx_hw_params.buf_addr = audio_tx_buf_addr;
  af_tx_hw_params.active_ch = 2;
  af_tx_hw_params.bits_per_sample = BIT_DEPTH_24;
  af_tx_hw_params.periods = 2;
  af_tx_hw_params.bytes_per_period = 8192;

  af_rx_hw_params.buf_addr = audio_rx_buf_addr;
  af_rx_hw_params.active_ch = 2;
  af_rx_hw_params.bits_per_sample = BIT_DEPTH_24;
  af_rx_hw_params.periods = 2;
  af_rx_hw_params.bytes_per_period = 8192;

  Status = XAudioFormatter_Initialize(&XAudioFormatterInst,
                                      XPAR_XAUDIO_FORMATTER_0_BASEADDR);
  if (Status != XST_SUCCESS) {
    printf("ERROR: XAudioFormatter_Initialize \r\n");
  }

  // Initialize for Transmitter

  XAudioFormatterInst.ChannelId = XAudioFormatter_MM2S;

  XAudioFormatter_SetMM2SCallback(&XAudioFormatterInst,
                                  XAudioFormatter_IOC_Handler, XMM2SAFCallback,
                                  (void *)&XAudioFormatterInst);

  XAudioFormatterSetFsMultiplier(&XAudioFormatterInst, AF_MCLK, AF_FS);

  XAudioFormatterSetHwParams(&XAudioFormatterInst, &af_tx_hw_params);

  XAudioFormatter_InterruptEnable(&XAudioFormatterInst, XAUD_CTRL_IOC_IRQ_MASK);

  Status = XSetupInterruptSystem(
      &XAudioFormatterInst, &XAudioFormatterMM2SIntrHandler,
      XPAR_AUDIO_FORMATTER_0_INTERRUPTS, XAudioFormatterInst.Config.IntrParent,
      XINTERRUPT_DEFAULT_PRIORITY);
  if (Status == XST_FAILURE) {
    xil_printf("IRQ init failed.\n\r\r");
  }

  // Initialize for Receiver
  XAudioFormatterInst.ChannelId = XAudioFormatter_S2MM;

  XAudioFormatter_SetS2MMCallback(&XAudioFormatterInst,
                                  XAudioFormatter_IOC_Handler, XS2MMAFCallback,
                                  (void *)&XAudioFormatterInst);

  XAudioFormatterSetHwParams(&XAudioFormatterInst, &af_rx_hw_params);

  XAudioFormatter_InterruptEnable(&XAudioFormatterInst, XAUD_CTRL_IOC_IRQ_MASK);

  Status = XSetupInterruptSystem(
      &XAudioFormatterInst, &XAudioFormatterS2MMIntrHandler,
      XPAR_AUDIO_FORMATTER_0_INTERRUPTS_1,
      XAudioFormatterInst.Config.IntrParent, XINTERRUPT_DEFAULT_PRIORITY);
  if (Status == XST_FAILURE) {
    xil_printf("IRQ init failed.\n\r\r");
  }

  /********** XI2s_Tx Initialize **********/
  XI2stx_Config *XI2stx_ConfigPtr;

  XI2stx_ConfigPtr = XI2s_Tx_LookupConfig(XPAR_XI2STX_0_BASEADDR);
  if (XI2stx_ConfigPtr == NULL) {
    xil_printf("XI2s_Tx_LookupConfig failed! terminating\r\n");
  }

  Status = XI2s_Tx_CfgInitialize(&XI2sTxInst, XI2stx_ConfigPtr,
                                 XI2stx_ConfigPtr->BaseAddress);
  if (Status != XST_SUCCESS) {
    printf("ERROR: XI2s_Tx_CfgInitialize \r\n");
  }

  XI2s_Tx_Enable(&XI2sTxInst, 0x0);

  XI2s_Tx_SetSclkOutDiv(&XI2sTxInst, I2S_TX_MCLK, I2S_TX_FS);

  XI2s_Tx_SetChMux(&XI2sTxInst, 0, XI2S_TX_CHMUX_AXIS_01);

  /********** XI2s_Rx Initialize **********/

  XI2srx_Config *XI2srx_ConfigPtr;

  XI2srx_ConfigPtr = XI2s_Rx_LookupConfig(XPAR_XI2SRX_0_BASEADDR);
  if (XI2srx_ConfigPtr == NULL) {
    xil_printf("XI2s_Rx_LookupConfig failed! terminating\r\n");
  }

  Status = XI2s_Rx_CfgInitialize(&XI2sRxInst, XI2srx_ConfigPtr,
                                 XI2srx_ConfigPtr->BaseAddress);
  if (Status != XST_SUCCESS) {
    printf("ERROR: XI2s_Rx_CfgInitialize \r\n");
  }

  XI2s_Rx_Enable(&XI2sRxInst, 0x0);

  XI2s_Rx_SetSclkOutDiv(&XI2sRxInst, I2S_RX_MCLK, I2S_RX_FS);

  XI2s_Rx_SetChMux(&XI2sRxInst, 0, XI2S_RX_CHMUX_XI2S_01);
}

void XMM2SAFCallback(void *data) {

  XAudioFormatter *AFInstancePtr = (XAudioFormatter *)data;
  AFInstancePtr->ChannelId = XAudioFormatter_MM2S;
  XAudioFormatter_InterruptClear(AFInstancePtr, XAUD_STS_IOC_IRQ_MASK);

  MM2SAFIntrReceived = 1;
}

void XS2MMAFCallback(void *data) {

  XAudioFormatter *AFInstancePtr = (XAudioFormatter *)data;
  AFInstancePtr->ChannelId = XAudioFormatter_S2MM;
  XAudioFormatter_InterruptClear(AFInstancePtr, XAUD_STS_IOC_IRQ_MASK);

  S2MMAFIntrReceived = 1;
}

void play_start(void) {

  XAudioFormatterInst.ChannelId = XAudioFormatter_MM2S;

  XAudioFormatterDMAStart(&XAudioFormatterInst);

  XAudioFormatterInst.ChannelId = XAudioFormatter_S2MM;

  XAudioFormatterDMAStart(&XAudioFormatterInst);

  XI2s_Tx_Enable(&XI2sTxInst, 0x1);

  XI2s_Rx_Enable(&XI2sRxInst, 0x1);
}

void play_stop(void) {

  XAudioFormatterInst.ChannelId = XAudioFormatter_MM2S;

  XAudioFormatterDMAStop(&XAudioFormatterInst);

  XAudioFormatterInst.ChannelId = XAudioFormatter_S2MM;

  XAudioFormatterDMAStop(&XAudioFormatterInst);

  XI2s_Tx_Enable(&XI2sTxInst, 0x0);

  XI2s_Rx_Enable(&XI2sRxInst, 0x0);
}

int main() {
  init_platform();

  Xil_DCacheEnable();

  print("Start\n\r");

  ring_buffer_init(&ring_buf_inst, (uint8_t *)ring_buff,
                   8192 * sizeof(int32_t));

  init();

  play_start();

  u8 tx_ping_pong = 0;
  u8 rx_ping_pong = 0;
  uint16_t tx_num;

  while (1) {
    if (MM2SAFIntrReceived == 1) {
      MM2SAFIntrReceived = 0;

      if (tx_ping_pong == 0) {
        tx_ping_pong = 1;

        tx_num = ring_buffer_pop(&ring_buf_inst, (uint8_t *)tx_ring_buf,
                                 2048 * sizeof(int32_t));
        if (tx_num != 2048 * sizeof(int32_t)) {
          for (int i = 0; i < 2048; i++) {

            audio_tx_buf[i] = 0;
          }
        } else {
          for (int i = 0; i < 2048; i++) {

            audio_tx_buf[i] = tx_ring_buf[i];
          }
        }

        Xil_DCacheFlushRange(first_half_tx_audio_buf_addr,
                             2048 * sizeof(int32_t));
      } else {
        tx_ping_pong = 0;

        tx_num = ring_buffer_pop(&ring_buf_inst, (uint8_t *)tx_ring_buf,
                                 2048 * sizeof(int32_t));
        if (tx_num != 2048 * sizeof(int32_t)) {
          for (int i = 2048; i < 4096; i++) {

            audio_tx_buf[i] = 0;
          }
        } else {
          for (int i = 2048; i < 4096; i++) {

            audio_tx_buf[i] = tx_ring_buf[i - 2048];
          }
        }

        Xil_DCacheFlushRange(second_half_tx_audio_buf_addr,
                             2048 * sizeof(int32_t));
      }
    }

    if (S2MMAFIntrReceived == 1) {
      S2MMAFIntrReceived = 0;

      if (rx_ping_pong == 0) {
        rx_ping_pong = 1;

        Xil_DCacheInvalidateRange(first_half_rx_audio_buf_addr,
                                  2048 * sizeof(int32_t));

        for (int i = 0; i < 2048; i++) {
          // Extending a 24-bit signed integer to 32 bits (value << 8) >> 8
          rx_ring_buf[i] = (audio_rx_buf[i] << 8) >> 8;
        }
        ring_buffer_push_ovr(&ring_buf_inst, (uint8_t *)rx_ring_buf,
                             2048 * sizeof(int32_t));

      } else {
        rx_ping_pong = 0;

        Xil_DCacheInvalidateRange(second_half_rx_audio_buf_addr,
                                  2048 * sizeof(int32_t));

        for (int i = 2048; i < 4096; i++) {

          rx_ring_buf[i - 2048] = (audio_rx_buf[i] << 8) >> 8;
        }
        ring_buffer_push_ovr(&ring_buf_inst, (uint8_t *)rx_ring_buf,
                             2048 * sizeof(int32_t));
      }
    }
  }

  cleanup_platform();
  return 0;
}

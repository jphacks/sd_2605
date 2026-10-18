/* SPDX-License-Identifier: Apache-2.0 */
#include "cybsp.h"
#include "cy_scb_i2c.h"
#include <stdio.h>
#include <string.h>
static cy_stc_scb_i2c_context_t ctx;
static void halt(void) { __disable_irq(); for (;;) {} }
int main(void) {
 if (cybsp_init()!=CY_RSLT_SUCCESS) halt();
 if (Cy_SCB_UART_Init(DEBUG_UART_HW,&DEBUG_UART_config,NULL)!=CY_SCB_UART_SUCCESS) halt();
 Cy_SCB_UART_Enable(DEBUG_UART_HW);
 if(Cy_SCB_I2C_Init(STEP3A_I2C_HW,&STEP3A_I2C_config,&ctx)!=CY_SCB_I2C_SUCCESS) halt();
 uint32_t clk=Cy_SysClk_PeriPclkGetFrequency(PCLK_SCB0_CLOCK_SCB_EN,CY_SYSCLK_DIV_16_BIT,0);
 uint32_t rate=Cy_SCB_I2C_SetDataRate(SCB0,100000,clk);
 Cy_SCB_I2C_Enable(SCB0); __enable_irq();
 char log[180];
 snprintf(log,sizeof(log),"[STEP3A] controller SCB0 SDA=P9.2 SCL=P9.0 clock=%lu rate=%lu\r\n",(unsigned long)clk,(unsigned long)rate);
 Cy_SCB_UART_PutString(DEBUG_UART_HW,log);
 unsigned streak=0,pass=0,fail=0,seq=0;
 const uint8_t expected[]={0xA5,0x01,0x30,0x5A};
 for(;;){
  Cy_SysLib_Delay(100);
  unsigned scl=Cy_GPIO_Read(STEP3A_SCL_PORT,STEP3A_SCL_PIN), sda=Cy_GPIO_Read(STEP3A_SDA_PORT,STEP3A_SDA_PIN);
  uint8_t data[4]={0};
  cy_en_scb_i2c_status_t status=Cy_SCB_I2C_MasterSendStart(SCB0,0x30,CY_SCB_I2C_READ_XFER,100,&ctx);
  for(unsigned i=0; i<4 && status==CY_SCB_I2C_SUCCESS; ++i)
   status=Cy_SCB_I2C_MasterReadByte(SCB0,i==3?CY_SCB_I2C_NAK:CY_SCB_I2C_ACK,&data[i],100,&ctx);
  cy_en_scb_i2c_status_t stop=Cy_SCB_I2C_MasterSendStop(SCB0,100,&ctx);
  int ok=status==CY_SCB_I2C_SUCCESS && stop==CY_SCB_I2C_SUCCESS && memcmp(data,expected,4)==0;
  if(ok){++streak;++pass;}else{streak=0;++fail;}
  snprintf(log,sizeof(log),"[STEP3A] n=%u %s data=%02X %02X %02X %02X status=%lu stop=%lu streak=%u pass=%u fail=%u scl=%u sda=%u clk=%lu rate=%lu\r\n",++seq,ok?"OK":"FAIL",data[0],data[1],data[2],data[3],(unsigned long)status,(unsigned long)stop,streak,pass,fail,scl,sda,(unsigned long)clk,(unsigned long)rate);
  Cy_SCB_UART_PutString(DEBUG_UART_HW,log);
  if(!ok) {
   Cy_SCB_I2C_Disable(SCB0,&ctx);
   if(Cy_SCB_I2C_Init(SCB0,&STEP3A_I2C_config,&ctx)!=CY_SCB_I2C_SUCCESS) halt();
   Cy_SCB_I2C_SetDataRate(SCB0,100000,clk);
   Cy_SCB_I2C_Enable(SCB0);
  }
 }
}

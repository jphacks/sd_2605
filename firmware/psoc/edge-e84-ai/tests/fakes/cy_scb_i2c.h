#pragma once
#include "cybsp.h"
typedef struct { int unused; } cy_stc_scb_i2c_context_t;
typedef struct {
    int i2cMode;
    uint8_t slaveAddress, slaveAddressMask;
    bool acceptAddrInFifo, ackGeneralAddr, useTxFifo, useRxFifo;
} cy_stc_scb_i2c_config_t;
typedef enum { CY_SCB_I2C_SUCCESS = 0 } cy_en_scb_i2c_status_t;
#define CY_SCB_I2C_SLAVE 1
#define CY_SCB_I2C_SLAVE_READ_EVENT 1u
#define CY_SCB_I2C_SLAVE_WRITE_EVENT 2u
#define CYBSP_I2C_CAM_CONTROLLER_HW ((void *)5)
#define CYBSP_I2C_CAM_CONTROLLER_IRQ 5
extern const cy_stc_scb_i2c_config_t CYBSP_I2C_CAM_CONTROLLER_config;
cy_en_scb_i2c_status_t Cy_SCB_I2C_Init(void *, const cy_stc_scb_i2c_config_t *, cy_stc_scb_i2c_context_t *);
void Cy_SCB_I2C_SlaveConfigReadBuf(void *, uint8_t *, uint32_t, cy_stc_scb_i2c_context_t *);
void Cy_SCB_I2C_SlaveConfigWriteBuf(void *, uint8_t *, uint32_t, cy_stc_scb_i2c_context_t *);
void Cy_SCB_I2C_Interrupt(void *, cy_stc_scb_i2c_context_t *);
void Cy_SCB_I2C_RegisterEventCallback(void *, void (*)(uint32_t), cy_stc_scb_i2c_context_t *);
void Cy_SCB_I2C_Enable(void *);

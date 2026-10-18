#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef uint32_t cy_rslt_t;
#define CY_RSLT_SUCCESS 0u
typedef int IRQn_Type;
typedef struct { IRQn_Type intrSrc; uint32_t intrPriority; } cy_stc_sysint_t;
typedef enum { CY_SYSINT_SUCCESS = 0 } cy_en_sysint_status_t;
uint32_t Cy_SysLib_EnterCriticalSection(void);
void Cy_SysLib_ExitCriticalSection(uint32_t key);
cy_en_sysint_status_t Cy_SysInt_Init(const cy_stc_sysint_t *config, void (*handler)(void));
void NVIC_ClearPendingIRQ(IRQn_Type irq);
void NVIC_EnableIRQ(IRQn_Type irq);

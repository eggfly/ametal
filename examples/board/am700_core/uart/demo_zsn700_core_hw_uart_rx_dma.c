/*******************************************************************************
*                                 AMetal
*                       ----------------------------
*                       innovating embedded platform
*
* Copyright (c) 2001-2018 Guangzhou ZHIYUAN Electronics Co., Ltd.
* All rights reserved.
*
* Contact information:
* web site:    http://www.zlg.cn/
*******************************************************************************/

/**
 * \file
 * \brief UART DMA 接收例程，通过 HW 层接口实现
 *
 * - 操作步骤：
 *   1. 通过上位机串口一次性向 MCU 发送 5 个字符。
 *
 * - 实验现象：
 *   1. 串口打印出接收到的数据；
 *   2. 然后串口打印出 "DMA transfer done!"。
 *
 * \note
 *    1. 如需观察串口打印的调试信息，需要将 PIOA_10 引脚连接 PC 串口的 TXD，
 *       PIOA_9 引脚连接 PC 串口的 RXD；
 *    2. 如果调试串口使用与本例程相同，则不应在后续继续使用调试信息输出函数
 *      （如：AM_DBG_INFO()）。
 *
 * \par 源代码
 * \snippet demo_zsn700_hw_uart_rx_dma.c src_zsn700_hw_uart_rx_dma
 *
 *
 * \internal
 * \par Modification History
 * - 1.00 19-09-23  zp, first implementation
 * \endinternal
 */

/**
 * \addtogroup demo_if_zsn700_hw_uart_rx_dma
 * \copydoc demo_zsn700_hw_uart_rx_dma.c
 */

/** [src_zsn700_hw_uart_rx_dma] */
#include "ametal.h"
#include "am_zsn700.h"
#include "demo_zlg_entries.h"
#include "am_zsn700_inst_init.h"
#include "demo_am700_core_entries.h"

/**
 * \brief 例程入口
 */
void demo_zsn700_core_hw_uart_rx_dma_entry (void)
{
    AM_DBG_INFO("demo am700_core hw uart rx dma!\r\n");

    /* 等待发送数据完成 */
    am_mdelay(100);

    /* 初始化引脚 */
    am_gpio_pin_cfg(PIOA_2, PIOA_2_UART1_TXD | PIOA_2_OUT_PP );
    am_gpio_pin_cfg(PIOA_3, PIOA_3_UART1_RXD | PIOA_10_INPUT_FLOAT);

    /* 使能时钟 */
    am_clk_enable(CLK_UART1);

    demo_zsn700_hw_uart_rx_dma_entry(ZSN700_UART1,
                                     am_clk_rate_get(CLK_UART1),
                                     DMA_CHAN_1,
                                     ZSN700_DMA_SRC_TYPE_UART1_RX);
}

/** [src_zsn700_hw_uart_rx_dma] */

/* end of file */

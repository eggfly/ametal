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
 * \brief bootloader ���̣���demo����Ϊ˫�� bootloader ��Ӧ�ó���
 *
 * - �������裺
 *   �ο�AMmetal-AM845-Core-bootloader�����ֲ�
 *
 *
 * \par Դ����
 * \snippet demo_am845_core_armcc_application.c src_demo_am845_core_armcc_application
 *
 * \internal
 * \par Modification History
 * - 1.00 19-03-25  yrh, first implementation
 * \endinternal
 */

/**
 * \addtogroup demo_if_am845_core_armcc_application
 * \copydoc demo_am845_core_armcc_application.c
 */
#include "ametal.h"
#include "am_board.h"
#include "am_vdebug.h"
#include "am_boot.h"
#include "am_uart.h"
#include "am_softimer.h"
#include "am_boot_firmware.h"
#include "am_boot.h"
#include "am_double_app_conf_lpc845.h"
#include "am_lpc84x_inst_init.h"
#include <string.h>

/**
 * \name ���ݽ���״̬
 * @{
 */
 
/** \brief �����������ݽ׶� */
#define  COMMAND_PHASE                   0

/** \brief ���չ̼����ݽ׶� */
#define  FIRMWARE_PHASE                  1

/** \brief ���չ̼�ͷ�����ݽ׶� */
#define  FIRMWARE_HEAD                   2
/** @} */

/** \brief �̼����ܳ�ʱʱ�� */
#define  FIRMWARE_RECEIVE_TIMEOUT_VALUE  15000

/** \brief ���ڽ��ջ�������С*/
#define UART_CALLBACK_BUF_SIZE           4096
/** \brief ���ڲ����� */
#define UART_BAUD                        9600

#define RECEIVE_BUF_SIZE                 64

/** \brief �û������ */
#define USER_COMMAND_LEN                 5

/** \brief �û����Զ��������, ������0x5a,0xa6��Ϊ֡ͷ��������0x0d��β�������Զ����м���������ַ�  */
static char user_command[5] = {0x5a, 0xa6, 0x11, 0x66, 0x0d};

/** \brief ������Ž��յ����� */
static char command_rec[6] = {0};

/** \brief ���ݽ���״̬ */
static uint8_t int_state = COMMAND_PHASE;
/** \brief �������ݽ��ջ�����  */
static uint8_t uart_callback_buffer[UART_CALLBACK_BUF_SIZE] = {0};

static uint8_t index_t = 0, command_error = 0, firmware_timeout = 0;

/** \brief �̼���Ų������  */
static am_boot_firmware_handle_t firmware_handle;

/** \brief flash�������  */
static am_boot_flash_handle_t    flash_handle;

/** \brief led������ʱ���ṹ��  */
static am_softimer_t             led_timer;

/** \brief ���ݽ��ճ�ʱ������ʱ���ṹ��  */
static am_softimer_t             receive_callback_timer;

static uint32_t  write_offset = 0, read_offset = 0;

am_uart_handle_t uart_handle;
/**
 * \brief ���ڽ��ջص�����
 */
void ___pfn_uart_rec_callback(void *p_arg, char inchar)
{
    if(int_state == COMMAND_PHASE) {
        if(index_t < sizeof(command_rec) ) {
            command_rec[index_t++] = inchar;
            am_softimer_start(&receive_callback_timer, 500);
        }
    } else {
        uart_callback_buffer[write_offset++] = (uint8_t)inchar;
        write_offset &= UART_CALLBACK_BUF_SIZE - 1;
    }
}

static void __delay(void)
{
    volatile int i = 1000000;
    while(i--);
}

/**
 * \brief  �Ӵ��ڽ��ջ�����������
 */
static int __read_data(uint8_t *p_buffer, uint32_t byte_count)
{
    uint32_t current_bytes_read = 0;
    am_softimer_start(&receive_callback_timer, FIRMWARE_RECEIVE_TIMEOUT_VALUE);
    while (current_bytes_read != byte_count) {
        if(firmware_timeout == 1) {
            firmware_timeout = 0;
            am_softimer_stop(&receive_callback_timer);
            return AM_ERROR;
        }

        if(read_offset != write_offset) {
            p_buffer[current_bytes_read++] = uart_callback_buffer[read_offset++];
            read_offset &= UART_CALLBACK_BUF_SIZE - 1;
        }
    }
    firmware_timeout = 0;
    am_softimer_stop(&receive_callback_timer);
    return AM_OK;
}
/**
 * \brief led������ʱ���ص�����
 * */
static void __led_timer_handle (void *p_arg)
{
    am_led_toggle(LED0);
}
/**
 * \brief ���ڽ�������������ʱ���ص�����
 */
static void __receive_callback_timer_handle(void *p_arg)
{
    int i;
    if(int_state == COMMAND_PHASE) {
        /* \brief ���յ������ַ���������  */
        if(index_t != USER_COMMAND_LEN) {
            command_error = 1;
        } else {
            for(i = 0; i < USER_COMMAND_LEN; i++) {
                if(user_command[i] != command_rec[i]) {
                    command_error = 1;
                    break;
                }
            }
            /* ���յ��û���������ȷ �������̼�����״̬ */
            if(i == USER_COMMAND_LEN){
                int_state = FIRMWARE_PHASE;
            }
        }
        memset(command_rec, 0, sizeof(command_rec));
        am_softimer_stop(&receive_callback_timer);
        index_t = 0;
    } else {
        firmware_timeout = 1;
    }
}

/**
 * \brief �̼�����ʧ�ܴ���
 */
static void __firmware_transmit_fail_handle(void)
{
    int_state = COMMAND_PHASE;
    am_softimer_start(&led_timer, 1000);
}

/**
 * \brief ����Ƿ��й̼����͹���
 */
static void __detect_firmware_receive(void)
{
    am_boot_firmware_verify_info_t firmware_head;
    uint32_t count, i;
    int ret;
    /* ��������� */
    if(command_error == 1) {
        AM_DBG_INFO("application : input command error! still execute previous application\r\n");
        command_error = 0;
        return;
    }
    /* �̼�����  */
    if(int_state == FIRMWARE_PHASE) {

        am_kprintf("application : update init...\r\n");
        /* �̼���ű�׼�ӿڳ�ʼ�� */
        firmware_handle = am_lpc845_boot_firmware_flash(flash_handle);
        /* ��ʼ�̼����  */
        am_boot_firmware_store_start(firmware_handle, 26 * 1024);
        am_softimer_start(&led_timer, 100);
        memset(uart_callback_buffer, 0, sizeof(uart_callback_buffer));
        am_kprintf("application : firmware transmission is ready\r\n");

        /* ��ȡ�̼�ͷ��  */
        if(AM_OK != __read_data((uint8_t *)&firmware_head, sizeof(firmware_head))) {
            __firmware_transmit_fail_handle();
            am_kprintf("application : firmware transmission is timeout, still execute previous application\r\n");
            return;
        }

         count = firmware_head.len / RECEIVE_BUF_SIZE;

         uint8_t buf_t[RECEIVE_BUF_SIZE] = {0};
				 am_kprintf("application �� receiving firmware...\r\n");
         for(i = 0; i < count; i++) {         
             ret = __read_data((uint8_t *)buf_t, sizeof(buf_t));
             if(AM_OK != ret) {
                 am_kprintf("application : firmware transmission is timeout, still execute previous application!\r\n");
                 break;
             }

             ret = am_boot_firmware_store_bytes(firmware_handle, (uint8_t *)buf_t, sizeof(buf_t));
             if(ret != AM_OK) {
                 am_kprintf("application : firmware store is error, still execute previous application!\r\n");
                 break;
             }
         }
         if(AM_OK != ret) {
					  __firmware_transmit_fail_handle();
            return;
         }

         if (firmware_head.len & (RECEIVE_BUF_SIZE - 1)) {
             if(AM_OK != __read_data((uint8_t *)buf_t, firmware_head.len & (RECEIVE_BUF_SIZE - 1))) {
                 am_kprintf("application : firmware transmission is error, still execute previous application!\r\n");
							   __firmware_transmit_fail_handle();
							   return;
             }

             if(AM_OK != am_boot_firmware_store_bytes (firmware_handle,
                                                      (uint8_t *)buf_t,
                                                      firmware_head.len & (RECEIVE_BUF_SIZE - 1))) {
                 am_kprintf("application : firmware store is error, still execute previous application!\r\n");
								 __firmware_transmit_fail_handle();
                 return;
             }
         }
       /* �̼���Ž���  */
       am_boot_firmware_store_final(firmware_handle);
       am_kprintf("application : firmware receive successful\r\n");
       /* �̼�У��  */
       ret = am_boot_firmware_verify(firmware_handle, &firmware_head);
       if(ret != AM_OK) {
           am_kprintf("application : firmware verify error\r\n");
				 	 __firmware_transmit_fail_handle();
           return;
				 
       } else {
           am_kprintf("application : firmware verify successful\r\n");
       }
       /* ����������־Ϊ��������Ч  */
       am_boot_update_flag_set(AM_BOOTLOADER_FLAG_UPDATE);
       am_kprintf("application : device will restart...\r\n");
		 
       /*���豸����֮ǰ��ý���һС�ε���ʱ���ô�����ʱ�佫���������ݴ�����*/
       __delay();
//			 am_lpc84x_usart0_inst_deinit(uart_handle);
//       am_lpc84x_clk_inst_deinit();
       /* �����豸 */
       am_boot_reset();
    }
}

void demo_am845_core_application_entry (void)
{
	  uart_handle = am_lpc84x_usart0_inst_init();
	  am_debug_init(uart_handle, UART_BAUD);
	
    am_kprintf("application : am845_core_bootloader_double_application start up successful!\r\n");

    am_softimer_init(&led_timer, __led_timer_handle, NULL);
    am_softimer_start(&led_timer, 1000);
    /* ���ڹ̼����յĳ�ʱ�Ķ�ʱ����ʼ�� */
    am_softimer_init(&receive_callback_timer, __receive_callback_timer_handle, NULL);
    /* flash��ʼ��  */
    flash_handle = am_lpc845_boot_flash_inst_init();
    /* bootloader ��׼�ӿڳ�ʼ��  */
    am_lpc845_std_boot_inst_init(flash_handle);

    
    /* ʹ�ܴ����ж�ģʽ */
    am_uart_ioctl(uart_handle, AM_UART_MODE_SET, (void *)AM_UART_MODE_INT);
    /* ע�ᷢ�ͻص����� */
    am_uart_callback_set(uart_handle, AM_UART_CALLBACK_RXCHAR_PUT, ___pfn_uart_rec_callback, NULL);

    while (1) {
        /* ����Ƿ��з��͹̼����������� */
        __detect_firmware_receive();

        /* �û�Ӧ�ó���  */
    }
}
/* end of file */
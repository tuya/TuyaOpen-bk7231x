/**
 ****************************************************************************************
 *
 * @file app_bk.c
 *
 *
 * Copyright (C) Beken Corp 2011-2016
 *
 ****************************************************************************************
 */
#include "include.h"
#include "mem_pub.h"
#include "rwnx_config.h"
#include "app.h"

#if (NX_POWERSAVE)
#include "ps.h"
#endif //(NX_POWERSAVE)

#include "sa_ap.h"
#include "sa_station.h"
#include "main_none.h"
#include "sm.h"
#include "uart_pub.h"

#include "rtos_pub.h"
#include "rtos_error.h"
#include "param_config.h"
#include "rxl_cntrl.h"
#include "lwip/pbuf.h"
#include "rw_pub.h"
#include "rw_msg_rx.h"
#include "hostapd_intf_pub.h"
#include "wlan_ui_pub.h"
#include "ps_debug_pub.h"
#include "power_save_pub.h"
#include "mcu_ps_pub.h"
#include "rw_msdu.h"
#include "txu_cntrl.h"

#if CFG_SUPPORT_ALIOS
#include "ll.h"
#elif (!CFG_SUPPORT_RTT)
#include "wlan_cli_pub.h"
#endif

#include "app_music_pub.h"
#include "bk7011_cal_pub.h"

#if (CFG_SUPPORT_ALIOS || CFG_SUPPORT_RTT)
beken_thread_t  init_thread_handle;
beken_thread_t  app_thread_handle;
#else
xTaskHandle  init_thread_handle;
xTaskHandle  app_thread_handle;
#endif
#define  INIT_STACK_SIZE   2000
#define  APP_STACK_SIZE    3072

beken_semaphore_t app_sema = NULL;
WIFI_CORE_T g_wifi_core = {0};
volatile int32_t bmsg_rx_count = 0;

extern void net_wlan_initial(void);
extern void wpas_thread_start(void);

void bk_app_init(void)
{
#if (!CFG_SUPPORT_RTT)
    //net_wlan_initial();
#endif
    wpas_thread_start();
}

void app_set_sema(void)
{
    OSStatus ret;
    ret = rtos_set_semaphore(&app_sema);

    (void)ret;
}

static void kmsg_bk_thread_main( void *arg )
{
    OSStatus ret;

    mr_kmsg_init();
    while(1)
    {
        ret = rtos_get_semaphore(&app_sema, BEKEN_WAIT_FOREVER);
        ASSERT(kNoErr == ret);

        rwnx_recv_msg();
        ke_evt_none_core_scheduler();
    }
}

static void init_thread_main( void *arg )
{
    GLOBAL_INTERRUPT_START();

    bk_app_init();
    os_printf("app_init finished\r\n");

    rtos_delete_thread( NULL );
}

/** @brief  When in dtim rf off mode,user can manual wakeup before dtim wakeup time.
 *          this function must be called in "core_thread" context
 */
int bmsg_ps_handler_rf_ps_mode_real_wakeup(void)
{
#if CFG_USE_STA_PS
    power_save_rf_dtim_manual_do_wakeup();
#endif
    return 0;
}

void bmsg_rx_handler(BUS_MSG_T *msg)
{
    GLOBAL_INT_DECLARATION();

    GLOBAL_INT_DISABLE();
    if(bmsg_rx_count > 0)
    {
        bmsg_rx_count -= 1;
    }
    GLOBAL_INT_RESTORE();

    rxl_cntrl_evt((int)msg->arg);
}

void bmsg_skt_tx_handler(BUS_MSG_T *msg)
{
    hapd_intf_ke_rx_handle(msg->arg);
}

void bmsg_tx_handler(BUS_MSG_T *msg)
{
    struct pbuf *p = (struct pbuf *)msg->arg;
    struct pbuf *q = p;
    uint8_t vif_idx = (uint8_t)msg->len;

    if(p->next)
    {
        q = pbuf_coalesce(p, PBUF_RAW);
        if(q == p)
        {
            // must be out of memory
            goto tx_handler_exit;
        }
    }

    ps_set_data_prevent();
#if CFG_USE_STA_PS
    bmsg_ps_handler_rf_ps_mode_real_wakeup();
    bk_wlan_dtim_rf_ps_mode_do_wakeup();
#endif
    rwm_transfer(vif_idx, q->payload, q->len, 0, 0);
tx_handler_exit:

    pbuf_free(q);
}

void bmsg_tx_raw_cb_handler(BUS_MSG_T *msg)
{
	rwm_raw_frame_with_cb((uint8_t *)msg->arg, msg->len, msg->cb, msg->param);
}

int bmsg_tx_raw_cb_sender(uint8_t *buffer, int length, void *cb, void *param)
{
	OSStatus ret;
	BUS_MSG_T msg;

	msg.type = BMSG_TX_RAW_CB_TYPE;
	msg.arg = (uint32_t)buffer;
	msg.len = length;
	msg.sema = NULL;
	msg.cb = cb;
	msg.param = param;

	ret = rtos_push_to_queue(&g_wifi_core.io_queue, &msg, 1*SECONDS);
	if(ret != kNoErr) 
	{
		APP_PRT("bmsg_tx_sender failed\r\n");
	}

	return ret;
}

static void tx_raw_data(uint8_t *pkt, int16_t len)
{
    MSDU_NODE_T *node;
    UINT8 *content_ptr;
    UINT32 queue_idx = AC_VI;
    struct txdesc *txdesc_new;
    struct umacdesc *umac;

    node = rwm_tx_node_alloc(len);
    if (node == NULL) {
        goto exit;
    }

    rwm_tx_msdu_renew(pkt, len, node->msdu_ptr);
    content_ptr = rwm_get_msdu_content_ptr(node);

    txdesc_new = tx_txdesc_prepare(queue_idx);
    if(txdesc_new == NULL || TXDESC_STA_USED == txdesc_new->status) {
        rwm_node_free(node);
        goto exit;
    }

    txdesc_new->status = TXDESC_STA_USED;
    txdesc_new->host.flags = TXU_CNTRL_MGMT;
    txdesc_new->host.msdu_node = (void *)node;
    txdesc_new->host.orig_addr = (UINT32)node->msdu_ptr;
    txdesc_new->host.packet_addr = (UINT32)content_ptr;
    txdesc_new->host.packet_len = len;
    txdesc_new->host.status_desc_addr = (UINT32)content_ptr;
    txdesc_new->host.tid = 0xff;

    umac = &txdesc_new->umac;
    umac->payl_len = len;
    umac->head_len = 0;
    umac->tail_len = 0;
    umac->hdr_len_802_2 = 0;

    umac->buf_control = &txl_buffer_control_24G;

    txdesc_new->lmac.agg_desc = NULL;
    txdesc_new->lmac.hw_desc->cfm.status = 0;

    ps_set_data_prevent();
#if CFG_USE_STA_PS
    bmsg_ps_handler_rf_ps_mode_real_wakeup();
    bk_wlan_dtim_rf_ps_mode_do_wakeup();
#endif

    txl_cntrl_push(txdesc_new, queue_idx);

exit:
    os_free(pkt);
}

static void tx_raw_beacon(int8_t chan, uint8_t *ssid, int8_t ssid_len)
{
    const int8_t *hdr = (const int8_t *)"\x80\x00\x00\x00""\xff\xff\xff\xff\xff\xff""\x00\x00\x00\x00\x00\x00""\x00\x00\x00\x00\x00\x00""\x00\x00";
    const int8_t *ie  = (const int8_t *)"\x01\x08\x82\x84\x8b\x96\x0c\x12\x18\x24"/*supported rates*/\
                        "\x03\x01\x06"/*DS*/\
                        "\x05\x04\x00\x02\x00\x06"/*TIM*/\
                        "\x2a\x01\x00"/*ERP*/\
                        "\x32\x04\x30\x48\x60\x6c"/*Ext supported rates*/\
                        "\x3b\x02\x51\x00"/*SOC*/\
                        "\xdd\x18\x00\x50\xf2\x02\x01\x01\x00\x00\x03\xa4\x00\x00\x27\xa4\x00\x00\x42\x43\x5e\x00\x62\x32\x2f\x00";/*WMM*/
    uint8_t *ptr;
    int32_t  bcn_len, ie_len = 58;
    struct bcn_frame *bcn = NULL;

    if ((NULL == ssid) || (0 == ssid_len))
	return;

    bcn_len  = sizeof(struct bcn_frame) + (2 + ssid_len) + ie_len;
    bcn_len -= 4; /*bcn_frame variable[]*/

    bcn = (struct bcn_frame *)os_zalloc(bcn_len);
    if (NULL != bcn) {
        memcpy(&bcn->h, hdr, sizeof(bcn->h));
        wifi_get_mac_address((char *)&bcn->h.addr2, CONFIG_ROLE_AP);
        memcpy(&bcn->h.addr3, &bcn->h.addr2, 6);
        bcn->bcnint = 0x0064;
        bcn->capa   = 0x0421;

        ptr    = bcn->variable;
        *ptr++ = 0x00;
        *ptr++ = ssid_len;
        memcpy(ptr, ssid, ssid_len);
        ptr += ssid_len;
        memcpy(ptr, ie, ie_len);
        bcn->variable[12+ssid_len+2] = chan;
        tx_raw_data((uint8_t*)bcn, bcn_len);
    }
}

static void bmsg_tx_beacon_handler(BUS_MSG_T *msg)
{
    BUS_MSG_PARAM_T *bcn_param;

    bcn_param = (BUS_MSG_PARAM_T*)msg->arg;
    if (bcn_param) {
        tx_raw_beacon(bcn_param->channel, bcn_param->ssid, strlen((char*)bcn_param->ssid));
        os_free(bcn_param);
    }
}

static void bmsg_tx_raw_handler(BUS_MSG_T *msg)
{
    uint8_t *pkt = (uint8_t *)msg->arg;
    uint16_t len = msg->len;

    tx_raw_data(pkt, len);
}

#if CFG_SUPPORT_ALIOS
void bmsg_rx_lsig_handler(BUS_MSG_T *msg)
{
	lsig_input((msg->arg&0xFFFF0000)>>16, msg->arg&0xFF, msg->len);
}
#endif

void bmsg_ioctl_handler(BUS_MSG_T *msg)
{
    ke_msg_send((void *)msg->arg);
}

void bmsg_music_handler(BUS_MSG_T *msg)
{
#if (CONFIG_APP_MP3PLAYER == 1)
    media_msg_sender((void *)msg->arg);
#endif
}

void bmsg_skt_tx_sender(void *arg)
{
    OSStatus ret;
    BUS_MSG_T msg;

    msg.type = BMSG_SKT_TX_TYPE;
    msg.arg = (uint32_t)arg;
    msg.len = 0;
    msg.sema = NULL;

    ret = rtos_push_to_queue(&g_wifi_core.io_queue, &msg, BEKEN_NO_WAIT);
    if(kNoErr != ret)
    {
        os_printf("bmsg_rx_sender_failed\r\n");
    }
}

extern void power_save_wait_timer_real_handler(void *data);
extern void power_save_wait_timer_start(void);

void ps_msg_process(UINT8 ps_msg)
{
    switch(ps_msg)
    {
#if CFG_USE_STA_PS
    case PS_BMSG_IOCTL_RF_ENABLE:
        power_save_dtim_enable();
        break;

    case PS_BMSG_IOCTL_RF_USER_WKUP:
        bmsg_ps_handler_rf_ps_mode_real_wakeup();
        break;

    case PS_BMSG_IOCTL_RF_DISANABLE:
        bmsg_ps_handler_rf_ps_mode_real_wakeup();
        power_save_dtim_disable();
        break;
#endif
#if CFG_USE_MCU_PS
    case PS_BMSG_IOCTL_MCU_ENABLE:
        mcu_ps_init();
        break;

    case PS_BMSG_IOCTL_MCU_DISANABLE:
        mcu_ps_exit();
        break;
#endif
#if CFG_USE_STA_PS
    case PS_BMSG_IOCTL_RF_TD_SET:
        power_save_td_ck_timer_set();
        break;

        case PS_BMSG_IOCTL_RF_TD_HANDLER:
            power_save_td_ck_timer_real_handler();
            break;

        case PS_BMSG_IOCTL_RF_KP_HANDLER:
            power_save_keep_timer_real_handler();
            break;

        case PS_BMSG_IOCTL_RF_KP_SET:
            power_save_keep_timer_set();
            break;

        case PS_BMSG_IOCTL_RF_KP_STOP:
            power_save_keep_timer_stop();
            break;
			
        case PS_BMSG_IOCTL_WAIT_TM_HANDLER:
            power_save_wait_timer_real_handler(NULL);
            break;
			
        case PS_BMSG_IOCTL_WAIT_TM_SET:
            power_save_wait_timer_start();
            break;
			
        case PS_BMSG_IOCTL_RF_PS_TIMER_INIT:
            power_save_set_keep_timer_time(20);
            break; 
			
        case PS_BMSG_IOCTL_RF_PS_TIMER_DEINIT:
            power_save_set_keep_timer_time(0);
            break; 
#endif

        default:
            break;
    }
}

void bmsg_null_sender(void)
{
    OSStatus ret;
    BUS_MSG_T msg;

    msg.type = BMSG_NULL_TYPE;
    msg.arg = 0;
    msg.len = 0;
    msg.sema = NULL;

    if(!rtos_is_queue_empty(&g_wifi_core.io_queue))
    {
        return;
    }

    ret = rtos_push_to_queue(&g_wifi_core.io_queue, &msg, BEKEN_NO_WAIT);
    if(kNoErr != ret)
    {
        os_printf("bmsg_null_sender_failed\r\n");
    }
}

void bmsg_rx_sender(void *arg)
{
    OSStatus ret;
    BUS_MSG_T msg;
    GLOBAL_INT_DECLARATION();

    msg.type = BMSG_RX_TYPE;
    msg.arg = (uint32_t)arg;
    msg.len = 0;
    msg.sema = NULL;

    GLOBAL_INT_DISABLE();
    if(bmsg_rx_count >= 2)
    {
        GLOBAL_INT_RESTORE();
        return;
    }

    bmsg_rx_count += 1;
    GLOBAL_INT_RESTORE();

    ret = rtos_push_to_queue(&g_wifi_core.io_queue, &msg, BEKEN_NO_WAIT);
    if(kNoErr != ret)
    {
        APP_PRT("bmsg_rx_sender_failed\r\n");
    }
}

int bmsg_tx_sender(struct pbuf *p, uint32_t vif_idx)
{
    OSStatus ret;
    BUS_MSG_T msg;

    msg.type = BMSG_TX_TYPE;
    msg.arg = (uint32_t)p;
    msg.len = vif_idx;
    msg.sema = NULL;

    pbuf_ref(p);
    ret = rtos_push_to_queue(&g_wifi_core.io_queue, &msg, 1 * SECONDS);
    if(kNoErr != ret)
    {
        APP_PRT("bmsg_tx_sender failed\r\n");
        pbuf_free(p);
    }

    return ret;
}

int bmsg_tx_raw_sender(uint8_t *payload, uint16_t length)
{
	OSStatus ret;
	BUS_MSG_T msg;

	msg.type = BMSG_TX_RAW_TYPE;
	msg.arg = (uint32_t)payload;
	msg.len = length;
	msg.sema = NULL;

	ret = rtos_push_to_queue(&g_wifi_core.io_queue, &msg, 1*SECONDS);

	if(ret != kNoErr) 
	{
		APP_PRT("bmsg_tx_sender failed\r\n");
		os_free(payload);
	}

	return ret;
}

int bmsg_tx_beacon_sender(BUS_MSG_PARAM_T *bcn_param)
{
	OSStatus ret;
	BUS_MSG_T msg;

	msg.type = BMSG_TX_BCN_TYPE;
	msg.arg  = (uint32_t)bcn_param;
	msg.len  = sizeof(BUS_MSG_PARAM_T);
	msg.sema = NULL;

	ret = rtos_push_to_queue(&g_wifi_core.io_queue, &msg, 1*SECONDS);

	if(ret != kNoErr)
	{
		APP_PRT("bmsg_tx_bcn_sender failed\r\n");
		os_free(bcn_param);
	}

	return ret;
}

#if CFG_SUPPORT_ALIOS
void bmsg_rx_lsig(uint16_t len, uint8_t rssi)
{
	BUS_MSG_T msg;

	msg.type = BMSG_RX_LSIG;
	msg.arg = (uint32_t)((len << 16) | rssi);
	msg.len = rtos_get_time();
	msg.sema = NULL;
	rtos_push_to_queue(&g_wifi_core.io_queue, &msg, BEKEN_NO_WAIT);
}
#endif

int bmsg_ioctl_sender(void *arg)
{
    OSStatus ret;
    BUS_MSG_T msg;

    msg.type = BMSG_IOCTL_TYPE;
    msg.arg = (uint32_t)arg;
    msg.len = 0;
    msg.sema = NULL;

    ret = rtos_push_to_queue(&g_wifi_core.io_queue, &msg, BEKEN_NO_WAIT);
    if(kNoErr != ret)
    {
        APP_PRT("bmsg_ioctl_sender_failed\r\n");
    }
    else
    {
        APP_PRT("bmsg_ioctl_sender\r\n");
    }

    return ret;
}

void bmsg_music_sender(void *arg)
{
    OSStatus ret;
    BUS_MSG_T msg;

    msg.type = BMSG_MEDIA_TYPE;
    msg.arg = (uint32_t)arg;
    msg.len = 0;
    msg.sema = NULL;

    ret = rtos_push_to_queue(&g_wifi_core.io_queue, &msg, BEKEN_NO_WAIT);
    if(kNoErr != ret)
    {
        APP_PRT("bmsg_media_sender_failed\r\n");
    }
}

#if CFG_USE_AP_PS
void bmsg_txing_sender(uint8_t sta_idx)
{
    OSStatus ret;
    BUS_MSG_T msg;

    msg.type = BMSG_TXING_TYPE;
    msg.arg = (uint32_t)sta_idx;
    msg.len = 0;
    msg.sema = NULL;

    ret = rtos_push_to_queue(&g_wifi_core.io_queue, &msg, BEKEN_NO_WAIT);
    if(kNoErr != ret)
    {
        APP_PRT("bmsg_txing_sender failed\r\n");
    }
}

void bmsg_txing_handler(BUS_MSG_T *msg)
{
    OSStatus ret;
    UINT8 sta_idx = (UINT8)msg->arg;

    rwm_msdu_send_txing_node(sta_idx);
}
#endif

void bmsg_ps_sender(uint8_t arg)
{
    OSStatus ret;
    BUS_MSG_T msg;
    if(g_wifi_core.io_queue)
    {
        msg.type = BMSG_STA_PS_TYPE;
        msg.arg = (uint32_t)arg;
        msg.len = 0;
        msg.sema = NULL;

        ret = rtos_push_to_queue(&g_wifi_core.io_queue, &msg, BEKEN_NO_WAIT);
        if(kNoErr != ret)
        {
            os_printf("bmsg_ps_sender failed\r\n");
        }
    }
    else
    {
        os_printf("g_wifi_core.io_queue null\r\n");
    }
}
#if CFG_USE_STA_PS

void bmsg_ps_handler(BUS_MSG_T *msg)
{
    UINT8 arg;

    arg = (UINT8)msg->arg;
    ps_msg_process(arg);
}
#endif
static void core_thread_main( void *arg )
{
    OSStatus ret;
    BUS_MSG_T msg;
    uint8_t ke_skip = 0;
    uint8_t ps_flag = 0;

    while(1)
    {
        ret = rtos_pop_from_queue(&g_wifi_core.io_queue, &msg, BEKEN_WAIT_FOREVER);
        if(kNoErr == ret)
        {
            switch(msg.type)
            {
#if CFG_USE_STA_PS
            case BMSG_STA_PS_TYPE:
                if(msg.arg == PS_BMSG_IOCTL_RF_DISANABLE)
                {
                    bmsg_ps_handler(&msg);
                }
                else
                {
                    ps_flag = 1;
                }
                break;
#endif

            case BMSG_RX_TYPE:
                APP_PRT("bmsg_rx_handler\r\n");
                bmsg_rx_handler(&msg);
                break;

            case BMSG_TX_TYPE:
                APP_PRT("bmsg_tx_handler\r\n");
                bmsg_tx_handler(&msg);
                break;

            case BMSG_SKT_TX_TYPE:
                APP_PRT("bmsg_skt_tx_handler\r\n");
                bmsg_skt_tx_handler(&msg);
                break;

            case BMSG_IOCTL_TYPE:
                APP_PRT("bmsg_ioctl_handler\r\n");
                bmsg_ioctl_handler(&msg);
                break;
            case BMSG_MEDIA_TYPE:
                ke_skip = 1;
                bmsg_music_handler(&msg);
                break;

#if CFG_USE_AP_PS
            case BMSG_TXING_TYPE:
                bmsg_txing_handler(&msg);
                break;
#endif

			case BMSG_TX_RAW_TYPE:
				bmsg_tx_raw_handler(&msg);
				break;
				
            case BMSG_TX_RAW_CB_TYPE:
                bmsg_tx_raw_cb_handler(&msg);
                break;

            case BMSG_TX_BCN_TYPE:
                bmsg_tx_beacon_handler(&msg);
                break;

#if CFG_SUPPORT_ALIOS					
				case BMSG_RX_LSIG:
					bmsg_rx_lsig_handler(&msg);
					break;
					
#endif
            default:
                APP_PRT("unknown_msg\r\n");
                break;
            }

            if (msg.sema != NULL)
            {
                rtos_set_semaphore(&msg.sema);
            }
            if(!ke_skip)
                ke_evt_core_scheduler();
            else
                ke_skip = 0;
        }

#if CFG_USE_STA_PS
        if(ps_flag == 1)
        {
            bmsg_ps_handler(&msg);
            ps_flag = 0;
        }
        power_save_rf_sleep_check();
#endif

    }
}

beken_thread_t  core_thread_handle;

void core_thread_init(void)
{
    OSStatus ret;

    g_wifi_core.queue_item_count = CORE_QITEM_COUNT;
    g_wifi_core.stack_size = CORE_STACK_SIZE;

    ret = rtos_init_queue(&g_wifi_core.io_queue,
                          "core_queue",
                          sizeof(BUS_MSG_T),
                          g_wifi_core.queue_item_count);
    if (kNoErr != ret)
    {
        os_printf("Create io queue failed\r\n");
        goto fail;
    }

    ret = rtos_create_thread(&g_wifi_core.handle,
                             THD_CORE_PRIORITY,
                             "core_thread",
                             (beken_thread_function_t)core_thread_main,
                             (unsigned short)g_wifi_core.stack_size,
                             (beken_thread_arg_t)0);
    if (kNoErr != ret)
    {
        os_printf("Create core thread failed\r\n");
        goto fail;
    }

    core_thread_handle = g_wifi_core.handle;
    return;

fail:
    core_thread_uninit();

    return;
}

void core_thread_uninit(void)
{
    if(g_wifi_core.handle)
    {
        rtos_delete_thread(&g_wifi_core.handle);
        g_wifi_core.handle = 0;
    }

    if(g_wifi_core.io_queue)
    {
        rtos_deinit_queue(&g_wifi_core.io_queue);
        g_wifi_core.io_queue = 0;
    }

    g_wifi_core.queue_item_count = 0;
    g_wifi_core.stack_size = 0;
}

#if (!CFG_SUPPORT_ALIOS && !CFG_SUPPORT_RTT)
extern void  user_main(void);
void __attribute__((weak)) user_main(void)
{
	
}

static void init_app_thread( void *arg )
{
    user_main();
	
    rtos_delete_thread( NULL );
}
#endif

void app_pre_start(void)
{
    OSStatus ret;

#if CFG_SUPPORT_ALIOS
    ret = rtos_init_semaphore(&app_sema, 0);
#else
    ret = rtos_init_semaphore(&app_sema, 1);
#endif
    ASSERT(kNoErr == ret);

    ret = rtos_create_thread(&app_thread_handle,
                             THD_APPLICATION_PRIORITY,
                             "kmsgbk",
                             (beken_thread_function_t)kmsg_bk_thread_main,
                             (unsigned short)APP_STACK_SIZE,
                             (beken_thread_arg_t)0);
    ASSERT(kNoErr == ret);

    ret = rtos_create_thread(&init_thread_handle,
                             THD_INIT_PRIORITY,
                             "init_thread",
                             (beken_thread_function_t)init_thread_main,
                             (unsigned short)INIT_STACK_SIZE,
                             (beken_thread_arg_t)0);
    ASSERT(kNoErr == ret);

    core_thread_init();

#if (CONFIG_APP_MP3PLAYER == 1)
    key_init();
    media_thread_init();
#endif
}

#if CFG_USE_TUYA_CCA_TEST
beken_timer_t test_timer;
static void test_timer_handler(void *data)
{
    LinkStatusTypeDef linkStatus;
    
    bk_wlan_get_link_status(&linkStatus);

    os_printf("rssi:%d\r\n", linkStatus.wifi_strength);
}
#endif

void app_start(void)
{
    app_pre_start();

#if CFG_UART2_CLI
	cli_init();
#endif

#if defined(SUPPORT_MIDEA_BLE)
    if(!get_ate_mode_state())
    {
	    bk_wlan_start_ble();
    }
#endif

#if CFG_USE_TUYA_CCA_TEST
	rtos_init_timer(&test_timer, 
						1000, 
						test_timer_handler, 
						(void *)0);

	rtos_start_timer(&test_timer);
#endif

#if (0)//(CFG_SUPPORT_BLE)
    extern void ble_entry(void);
    ble_entry();
#endif
}

#define FLASH_OTP_DEMO		0
#if ((CFG_USE_FLASH_OTP) && FLASH_OTP_DEMO)
#include "flash_bypass.h"
uint8_t otp_ram_tab[] = "abcdefghijklnmopqrstuvwxyz\r\n";

void beken_otp_test(void)
{
    UINT8 i = 0;
    otp_ctrl_t otp_ctrl     = {0};
    otp_ctrl.otp_index      = 1;  // "1 or 2 or 3"
    otp_ctrl.write_data_len = sizeof(otp_ram_tab);
    otp_ctrl.write_data     = (uint8_t *)otp_ram_tab;
    otp_ctrl.read_data_len  = sizeof(otp_ram_tab);
    otp_ctrl.read_data      = (uint8_t *)os_malloc(otp_ctrl.read_data_len);

    if(!otp_ctrl.read_data) {
        bk_printf("no memory for %s to malloc\r\n", __func__);
        return;
    }
    // flash_bypass_otp_operation(CMD_OTP_LOCK, &otp_ctrl);

    flash_bypass_otp_operation(CMD_OTP_READ, &otp_ctrl);

	for(uint8_t i = 0; i < otp_ctrl.read_data_len; i++)
		bk_printf("%c", otp_ctrl.read_data[i]);
	bk_printf("\r\n");

    do {
        flash_bypass_otp_operation(CMD_OTP_WRITE, &otp_ctrl);
        flash_bypass_otp_operation(CMD_OTP_READ, &otp_ctrl);

		for(uint8_t i = 0; i < otp_ctrl.write_data_len; i++)
			bk_printf("%c", otp_ctrl.write_data[i]);
		bk_printf("\r\n");

		for(uint8_t i = 0; i < otp_ctrl.read_data_len; i++)
			bk_printf("%c", otp_ctrl.read_data[i]);
		bk_printf("\r\n");
	        bk_printf("otp write retry %d\n", i);
    } while((os_memcmp(otp_ctrl.write_data, otp_ctrl.read_data, otp_ctrl.read_data_len) != 0) && (i++ < 3));
    os_free(otp_ctrl.read_data);
}
#endif

extern int manual_cal_rfcali_status(void);
#define OTP_FLASH_DATA_SIZE        1024
#define OTP_FLASH_RFDATA_SIZE       512
#define PARTITION_SIZE         (1 << 12) /* 4KB */

#include "drv_model_pub.h"
#include "flash_pub.h"
#include "flash_bypass.h"
#include "BkDriverFlash.h"

static void __read_otp_flash_rfcali_data(uint8_t *otp_data, uint16_t len)
{
    otp_ctrl_t otp_ctrl     = {0};
    otp_ctrl.otp_index      = 1;  // "1 or 2 or 3"
    otp_ctrl.write_data_len = 0;
    otp_ctrl.write_data     = NULL;
    otp_ctrl.read_data_len  = len;
    otp_ctrl.read_data      = otp_data;

    flash_bypass_otp_operation(CMD_OTP_READ, &otp_ctrl);
}

static int __check_otp_flash_rfcali_data(uint8_t *otp_data, uint16_t len)
{
    struct txpwr_elem_st
    {
        UINT32 type;
        UINT32 len;
    } *head;

    head = (struct txpwr_elem_st *)otp_data;
    if (head->type != BK_FLASH_OPT_TLV_HEADER) {
        bk_printf("otp flash data type error %x\n", head->type);
        return -1;
    }
    return 0; 
}

void backup_rfcali_data(void)
{
    DD_HANDLE flash_handle;
    UINT32 status;
    uint32_t addr;
    uint8_t *dst;
    uint32_t size;

    dst = os_malloc(OTP_FLASH_DATA_SIZE);
    if (dst == NULL) {
        bk_printf("malloc rfcali data failed\n");
        return;
    }

    memset(dst, 0, OTP_FLASH_DATA_SIZE);

    otp_ctrl_t otp_ctrl     = {0};
    otp_ctrl.otp_index      = 1;  // "1 or 2 or 3"
    otp_ctrl.write_data_len = 0;
    otp_ctrl.write_data     = NULL;
    otp_ctrl.read_data_len  = OTP_FLASH_DATA_SIZE;
    otp_ctrl.read_data      = dst;
    flash_bypass_otp_operation(CMD_OTP_READ, &otp_ctrl);

    /* TODO: need to consider whether to use locks at the TKL layer*/
    hal_flash_lock();

    flash_handle = ddev_open(FLASH_DEV_NAME, &status, 0);
	bk_logic_partition_t *pt = bk_flash_get_info(BK_PARTITION_RF_FIRMWARE);
    addr = pt->partition_start_addr;
    size = OTP_FLASH_RFDATA_SIZE;
    ddev_read(flash_handle, (char *)dst, size, addr);
    ddev_close(flash_handle);

    /* TODO: need to consider whether to use locks at the TKL layer*/
    hal_flash_unlock();

    memset(&otp_ctrl, 0, sizeof(otp_ctrl_t));
    otp_ctrl.otp_index      = 1;  // "1 or 2 or 3"
    otp_ctrl.write_data_len = OTP_FLASH_DATA_SIZE;
    otp_ctrl.write_data     = dst;
    otp_ctrl.read_data_len  = 0;
    otp_ctrl.read_data      = NULL;

    flash_bypass_otp_operation(CMD_OTP_WRITE, &otp_ctrl);    
   
    if (dst)
        os_free(dst);
    
    bk_printf("backup rfcali data success\n");
}
static unsigned int __uni_flash_is_protect_all(void)
{
    DD_HANDLE flash_handle;
    unsigned int status;
    unsigned int param;

    flash_handle = ddev_open(FLASH_DEV_NAME, &status, 0);
    ddev_control(flash_handle, CMD_FLASH_GET_PROTECT, (void *)&param);
    ddev_close(flash_handle);

    return (FLASH_PROTECT_ALL == param);
}
static void __recovery_rfcali_data(uint8_t *otp_data, uint16_t len)
{
    DD_HANDLE flash_handle;
    UINT32 status;
    unsigned int  param;
    unsigned int protect_flag;
    unsigned int sector_addr;

    uint32_t addr;
    uint8_t *dst;

	bk_logic_partition_t *pt = bk_flash_get_info(BK_PARTITION_RF_FIRMWARE);
    addr = pt->partition_start_addr;

    /* TODO: need to consider whether to use locks at the TKL layer*/
    hal_flash_lock();

    flash_handle = ddev_open(FLASH_DEV_NAME, &status, 0);

    protect_flag = __uni_flash_is_protect_all();
    if (protect_flag) {
        param = FLASH_PROTECT_HALF;
        ddev_control(flash_handle, CMD_FLASH_SET_PROTECT, (void *)&param);
    }

    sector_addr = addr;
    ddev_control(flash_handle, CMD_FLASH_ERASE_SECTOR, (void *)(&sector_addr));

    ddev_write(flash_handle, (char *)otp_data, OTP_FLASH_RFDATA_SIZE, addr);

    ddev_close(flash_handle);

    /* TODO: need to consider whether to use locks at the TKL layer*/
    hal_flash_unlock();

    bk_printf("recovery rfcali data success\n");
}
static int user_recovery_rfcali_data(void)
{
    if(manual_cal_rfcali_status()) {
        // rfcali data exist
        return 0;
    }

    bk_printf("[NOTE]: rfcali data isn't exist\n");

    uint8_t *otp_data = os_malloc(OTP_FLASH_DATA_SIZE);
    if(otp_data == NULL) {
        bk_printf("malloc rfcali data failed\n");
        return -1;
    }
    memset(otp_data, 0, OTP_FLASH_DATA_SIZE);
    __read_otp_flash_rfcali_data(otp_data, OTP_FLASH_DATA_SIZE);
    
    if (__check_otp_flash_rfcali_data(otp_data, OTP_FLASH_DATA_SIZE) < 0) {
        bk_printf("check rfcali data failed\n");
    } else {
        bk_printf("check rfcali data success\n");
        __recovery_rfcali_data(otp_data, OTP_FLASH_RFDATA_SIZE);
    }
    os_free(otp_data);

    return 0;
}

void user_main_entry(void)
{
    user_recovery_rfcali_data();

    __asm("BL __libc_init_array");

    extern void tuya_app_main(void);
    tuya_app_main();
    /*
	rtos_create_thread(NULL,
					   THD_INIT_PRIORITY,
					   "app",
					   (beken_thread_function_t)init_app_thread,
					   APP_STACK_SIZE,
					   (beken_thread_arg_t)0);
					   */
}

int bmsg_is_empty(void)
{
    if(!rtos_is_queue_empty(&g_wifi_core.io_queue))
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

// eof


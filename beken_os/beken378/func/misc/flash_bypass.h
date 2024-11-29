#ifndef __FLASH_BYPASS_H__
#define __FLASH_BYPASS_H__

#ifdef __cplusplus
extern "C" {
#endif

#define CURRENT_ROUTINE_TEXT_SIZE            (0x400)

/*Write Enable for Volatile Status Register*/
#define FLASH_CMD_WR_EN_VSR                 (0x50)
/*Write Status Register*/
#define FLASH_CMD_WR_SR                     (0x01)

#define SPI_VAL_TAKE_CS                     (0x02)
#define SPI_VAL_RELEASE_CS                  (0x03)

#if (CFG_SOC_NAME == SOC_BK7238) || (CFG_SOC_NAME == SOC_BK7231N)
/*Write Enable, sets the write enable latch bit*/
#define FLASH_CMD_WR_EN                     (0x06)
/*Erase Security Registers, Erase security registers*/
#define FLASH_CMD_OTP_EARSE                 (0x44)
/*Program Security Registers, Program security registers*/
#define FLASH_CMD_OTP_WRITE                 (0x42)
/*Read Security Registers, Read value of security register*/
#define FLASH_CMD_OTP_READ                  (0x48)
/*Read statue register, bit[17:8]*/
#define FLASH_CMD_OTP_STATE_READ_H          (0x35)
/*Read statue register, bit[7:0]*/
#define FLASH_CMD_OTP_STATE_READ_L          (0x05)
/*Write statue register, bit[17:8]*/
#define FLASH_CMD_OTP_STATE_WRITE_H         (0x31)
/*Write statue register, bit[7:0]*/
#define FLASH_CMD_OTP_STATE_WRITE_L         (0x01)

#define FLASH_OTP_IDX_MAX                   (3)
#define FLASH_OTP_IDX_MIN                   (1)
#define FLASH_OTP_WRITE_BYTE_LEN_MAX        (128)

#define FLASH_OTP_WRITE_HEAD_LEN            (4)
#define FLASH_OTP_EARSE_TASK_LEN            (4)
#define FLASH_OTP_READ_TASK_LEN             (5)
#define FLASH_OTP_READ_STATE_TASK_LEN       (1)
#define FLASH_OTP_WRITE_STATE_TASK_LEN      (2)

// otp1 addr: 0x1000
// otp2 addr: 0x2000
// otp3 addr: 0x3000
#define FLASH_OTP_IDX_X_ADDR(x)             ((x) << 12)
#define FLASH_OTP_STATE_OFFSET              (2)

#define OTP_OPERATION_SUCCESS               (0)
#define OTP_OPERATION_FAILURE               (1)

typedef enum
{
	CMD_OTP_READ = 1,
	CMD_OTP_EARSE,
	CMD_OTP_WRITE,
	CMD_OTP_LOCK,
	CMD_OTP_READ_XBYTE,
} OTP_CMD_CODE;

/*
 * brief: otp control structure
 */
typedef struct {
	uint32_t byte_offset;     /*!<  the destination OTP byte offset 0~255  */
	uint32_t write_data_len;  /*!<  the data length(1~256) you want to save in otp  */
	uint32_t read_data_len;   /*!<  the data length(1~256) you want to read from otp  */
	uint8_t  otp_index;       /*!<  the destination OTP index 1/2/3  */
	uint8_t  *write_data;     /*!<  the data you want to save in otp  */
	uint8_t  *read_data;      /*!<  the data you want to read from otp  */
} otp_ctrl_t;
#endif

extern uint32_t flash_bypass_operate_sr_init(void);
extern int flash_bypass_op_read(uint8_t *tx_buf, uint32_t tx_len, uint8_t *rx_buf, uint32_t rx_len);
extern int flash_bypass_op_write(uint8_t *op_code, uint8_t *tx_buf, uint32_t tx_len);
#if (CFG_SOC_NAME == SOC_BK7238) || (CFG_SOC_NAME == SOC_BK7231N)
extern int flash_bypass_otp_operation(OTP_CMD_CODE cmd, void *parm);
#endif

#ifdef __cplusplus
}
#endif
#endif //__FLASH_BYPASS_H__
// eof


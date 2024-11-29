#include "include.h"
#include "arm_arch.h"
#include "flash_bypass.h"
#include "gpio_pub.h"
#include "gpio.h"
#include "icu_pub.h"
#include "flash_pub.h"
#include "spi_pub.h"
#include "sys_ctrl.h"
#if (CFG_SOC_NAME == SOC_BK7221U)
#include "spi.h"
#elif(CFG_SOC_NAME == SOC_BK7238) || (CFG_SOC_NAME == SOC_BK7231N)
#include "spi_bk7231n.h"
#include "mem_pub.h"
#include "target_util_pub.h"
#include "flash.h"
#endif
#include "icu.h"

#if ((CFG_SOC_NAME == SOC_BK7221U))
extern void spi_gpio_configuration(void);
extern void bk_spi_configure(UINT32 rate, UINT32 mode);

int flash_bypass_wr_sr_cb(uint32_t larg, uint32_t rarg)
{
	volatile int32_t delay_count, j;
	uint8_t sr_width = (uint8_t)larg;
	uint16_t sr_val = (uint16_t)rarg;
	uint32_t intc_enable_reg, reg;
	char *text_ptr, temp_buf = 0;
	uint32_t i, reg_ctrl, reg_dat;
	uint32_t reg_stat, reg_sctrl;
	int exceptional_flag = 0;
	UINT32 ret, mode, is_spi_mode;

	if (0 == flash_is_support_0x50h_cmd()
		|| (sr_width > 2))
		return exceptional_flag;

	/*step 0, disable interrupt*/
	intc_enable_reg = REG_READ(ICU_GLOBAL_INT_EN);
	reg = intc_enable_reg & (~(GINTR_FIQ_EN | GINTR_IRQ_EN));
	REG_WRITE(ICU_GLOBAL_INT_EN, reg);

	/*step 1, save spi register configuration*/
	reg_ctrl = REG_READ(SPI_CTRL);
	reg_stat = REG_READ(SPI_STAT);
	reg_dat  = REG_READ(SPI_DAT);
	reg_sctrl = REG_READ(SPI_SLAVE_CTRL);

	/*step 2, resident cache*/
	REG_WRITE(SPI_CTRL, 0);
	do {
		text_ptr = (char *)flash_bypass_wr_sr_cb;
		for (i = 0; i < CURRENT_ROUTINE_TEXT_SIZE; i ++)
			temp_buf += text_ptr[i];

		REG_WRITE(SPI_STAT, temp_buf);
	} while (0);

	/*step 3, config spi master*/
	/*     3.1 clear spi fifo content*/
	reg = REG_READ(SPI_STAT);
	while ((reg & RXFIFO_EMPTY) == 0) {
		REG_READ(SPI_DAT);
		reg = REG_READ(SPI_STAT);
	}

	/*     3.2 disable spi block*/
	reg = REG_READ(ICU_PERI_CLK_PWD);
	reg |= (PWD_SPI_CLK_BIT);
	REG_WRITE(ICU_PERI_CLK_PWD, reg);

	/*     3.3 clear spi status*/
	REG_WRITE(SPI_CTRL, 0);
	reg = REG_READ(SPI_STAT);
	REG_WRITE(SPI_STAT, reg);

	reg = REG_READ(SPI_SLAVE_CTRL);
	REG_WRITE(SPI_SLAVE_CTRL, reg);

	/*     3.4 save the previous setting status*/
	ret = gpio_get_config(GPIO14, &mode);
	is_spi_mode = 0;
	if ((GPIO_SUCCESS == ret) && (PERIAL_MODE_2 == mode))
		is_spi_mode = 1;

	/*     3.5 set the spi master mode*/
	bk_spi_configure(SPI_DEF_CLK_HZ, SPI_DEF_MODE);

	/*step 4, gpio(14/15/16/17) are set as high-impedance state or input state ,
	          for spi mux with them*/
	gpio_config(GPIO14, GMODE_SET_HIGH_IMPENDANCE);
	gpio_config(GPIO15, GMODE_SET_HIGH_IMPENDANCE);
	gpio_config(GPIO16, GMODE_SET_HIGH_IMPENDANCE);
	gpio_config(GPIO17, GMODE_SET_HIGH_IMPENDANCE);

	/*step 5, switch flash interface to spi:sctrl_flash_select_spi_controller()
	 *        Pay attention to prefetch instruction destination, the text can not
	 *        fetch from flash space after this timepoint.
	 */
	reg = REG_READ(SCTRL_CONTROL);
	reg |= FLASH_SPI_MUX_BIT;
	REG_WRITE(SCTRL_CONTROL, reg);
	while (1) { /* double check the bit:FLASH_SPI_MUX_BIT. The bit cannot be set successfully, if cpu is fetching instruction*/
		reg = REG_READ(SCTRL_CONTROL);
		if (reg & FLASH_SPI_MUX_BIT)
			break;
	}

	/*step 6, write enable for volatile status register: 50H*/
	/*      6.1:take cs*/
	reg = REG_READ(SPI_CTRL);
	reg &= ~(NSSMD_MASK << NSSMD_POSI);
	reg |= (SPI_VAL_TAKE_CS << NSSMD_POSI);
	REG_WRITE(SPI_CTRL, reg);

	/*      6.2:write tx fifo*/
	reg = REG_READ(SPI_STAT);
	if ((reg & TXFIFO_FULL) == 0)
		REG_WRITE(SPI_DAT, FLASH_CMD_WR_EN_VSR);
	else {
		exceptional_flag = -1;
		goto wr_exceptional;
	}

	/*      6.3:waiting for TXFIFO_EMPTY interrupt*/
	while (1) {
		reg = REG_READ(SPI_STAT);
		if (reg & TXFIFO_EMPTY)
			break;
	}

	while (1) {
		reg = REG_READ(SPI_STAT);
		if (reg & SPIBUSY)
			break;
	}

	/*delay 1us--5us. the bits of 2 clock do not send over, if the spi status is TXFIFO_EMPTY*/
	for (delay_count = 0; delay_count < 1; delay_count ++) {
		for (j = 0; j < 4; j ++)
			;
	}

	/*      6.4:release cs*/
	reg = REG_READ(SPI_CTRL);
	reg &= ~(NSSMD_MASK << NSSMD_POSI);
	reg |= (SPI_VAL_RELEASE_CS << NSSMD_POSI);
	REG_WRITE(SPI_CTRL, reg);

	/*step 7, write flash command:01H and sr*/
	/*      7.1:take cs*/
	reg = REG_READ(SPI_CTRL);
	reg &= ~(NSSMD_MASK << NSSMD_POSI);
	reg |= (SPI_VAL_TAKE_CS << NSSMD_POSI);
	REG_WRITE(SPI_CTRL, reg);

	/*      7.2:write tx fifo*/
	reg = REG_READ(SPI_STAT);
	if ((reg & TXFIFO_FULL) == 0) {
		REG_WRITE(SPI_DAT, FLASH_CMD_WR_SR);
		for (int i = 0; i < sr_width; i ++)
			REG_WRITE(SPI_DAT, (sr_val >> (i * 8)) & 0xff);
	} else {
		exceptional_flag = -2;
		goto wr_exceptional;
	}

	/*      7.3:waiting for TXFIFO_EMPTY interrupt*/
	while (1) {
		reg = REG_READ(SPI_STAT);
		if (reg & TXFIFO_EMPTY)
			break;
	}

	while (1) {
		reg = REG_READ(SPI_STAT);
		if (reg & SPIBUSY)
			break;
	}

	/*delay 1us--5us. the bits of 2 clock do not send over, if the spi status is TXFIFO_EMPTY*/
	for (delay_count = 0; delay_count < 1; delay_count ++) {
		for (j = 0; j < 4; j ++)
			;
	}

	/*      7.4:release cs*/
	reg = REG_READ(SPI_CTRL);
	reg &= ~(NSSMD_MASK << NSSMD_POSI);
	reg |= (SPI_VAL_RELEASE_CS << NSSMD_POSI);
	REG_WRITE(SPI_CTRL, reg);

	exceptional_flag = 1;

wr_exceptional:
	/*step 8, flash interface select: flash controller-->sctrl_flash_select_flash_controller()*/
	reg = REG_READ(SCTRL_CONTROL);
	reg &= ~(FLASH_SPI_MUX_BIT);
	REG_WRITE(SCTRL_CONTROL, reg);

	/*step 9, gpio(14/15/16/17) second function*/
	if (is_spi_mode)
		spi_gpio_configuration();

	/*step 10, restore spi register configuration*/
	REG_WRITE(SPI_CTRL, reg_ctrl);
	REG_WRITE(SPI_STAT, reg_stat);
	REG_WRITE(SPI_DAT, reg_dat);
	REG_WRITE(SPI_SLAVE_CTRL, reg_sctrl);

	/*step 11, enable interrupt*/
	REG_WRITE(ICU_GLOBAL_INT_EN, intc_enable_reg);

	if (exceptional_flag < 0)
		os_printf("fb_wr_sr failed %d\r\n", exceptional_flag);

	return exceptional_flag;
}
#elif (CFG_SOC_NAME == SOC_BK7238) || (CFG_SOC_NAME == SOC_BK7231N)
int flash_bypass_op_read(uint8_t *tx_buf, uint32_t tx_len, uint8_t *rx_buf, uint32_t rx_len)
{
	uint32_t intc_enable_reg, reg;
	uint32_t reg_ctrl, reg_dat;
	uint32_t reg_stat, reg_config, reg_icu_clk, reg_icu_pwd;
	uint32_t gpio14_status, gpio15_status, gpio16_status, gpio17_status;
	uint32_t gpio14_15_cfg, gpio16_17_cfg;
	int exceptional_flag = 0, use_less = 0, k = 0, wait = 0;

	while(REG_READ(REG_FLASH_OPERATE_SW) & BUSY_SW)
	{
		;
	}

	/*step 0, disable interrupt*/
	intc_enable_reg = REG_READ(ICU_GLOBAL_INT_EN);
	reg = intc_enable_reg & (~(GINTR_FIQ_EN | GINTR_IRQ_EN));
	REG_WRITE(ICU_GLOBAL_INT_EN, reg);

	/*step 1, save spi register configuration*/
	reg_ctrl = REG_READ(SPI_CTRL);
	reg_config = REG_READ(SPI_CONFIG);
	reg_stat = REG_READ(SPI_STAT);
	reg_dat  = REG_READ(SPI_DAT);

	/*step 2, resident cache*/
	// link this function to itcm on bk7238

	/*step 3, config spi master*/
	/*     3.1 clear spi fifo content*/
	reg = REG_READ(SPI_STAT);
	while (reg & RXFIFO_RD_READ) {
		REG_READ(SPI_DAT);
		reg = REG_READ(SPI_STAT);
	}

	/*     3.2 disable spi block, and backup */
	reg_icu_pwd = reg = REG_READ(ICU_PERI_CLK_PWD);
	reg |= (PWD_SPI_CLK_BIT);
	REG_WRITE(ICU_PERI_CLK_PWD, reg);

	/*     3.3 clear spi status*/
	REG_WRITE(SPI_CTRL, 0);
	REG_WRITE(SPI_CONFIG, 0);
	reg = REG_READ(SPI_STAT);
	REG_WRITE(SPI_STAT, reg);

	/*     3.4 save the previous setting status*/
	gpio14_status = REG_READ(REG_GPIO_CFG_BASE_ADDR + GPIO14*4);
	gpio15_status = REG_READ(REG_GPIO_CFG_BASE_ADDR + GPIO15*4);
	gpio16_status = REG_READ(REG_GPIO_CFG_BASE_ADDR + GPIO16*4);
	gpio17_status = REG_READ(REG_GPIO_CFG_BASE_ADDR + GPIO17*4);
	gpio14_15_cfg = REG_READ(REG_GPIO_FUNC_CFG);
	gpio16_17_cfg = REG_READ(REG_GPIO_FUNC_CFG_2);

	/*     3.5 set the spi master mode*/
	// open clock, and backup
	reg = reg_icu_clk = REG_READ(ICU_PWM_CLK_MUX);
	reg |= PCLK_POSI_SPI;
	REG_WRITE(ICU_PWM_CLK_MUX, reg);
	// select 26M
	reg = REG_READ(ICU_PERI_CLK_PWD);
	reg &= ~(PWD_SPI_CLK_BIT);
	REG_WRITE(ICU_PERI_CLK_PWD, reg);

	// set to spi config directly
	reg = 0xC00100; // spien  msten  spi_clk=1---13M
	REG_WRITE(SPI_CTRL, reg);

	/*step 4, gpio(14/15/16/17) are set as high-impedance state or input state , for spi mux with them*/
	gpio_config(GPIO14, GMODE_SET_HIGH_IMPENDANCE);
	gpio_config(GPIO15, GMODE_SET_HIGH_IMPENDANCE);
	gpio_config(GPIO16, GMODE_SET_HIGH_IMPENDANCE);
	gpio_config(GPIO17, GMODE_SET_HIGH_IMPENDANCE);

	/*step 5, switch flash interface to spi:sctrl_flash_select_spi_controller()
	*         Pay attention to prefetch instruction destination, the text can not
	*         fetch from flash space after this timepoint.
	*/
	reg = REG_READ(SCTRL_CONTROL);
	reg |= FLASH_SPI_MUX_BIT;
	REG_WRITE(SCTRL_CONTROL, reg);
	while (1) { /* double check the bit:FLASH_SPI_MUX_BIT. The bit cannot be set successfully, if cpu is fetching instruction*/
		reg = REG_READ(SCTRL_CONTROL);
		if (reg & FLASH_SPI_MUX_BIT)
			break;
	}

	/*step 7, write flash command:01H and sr*/
	/*      7.1:take cs*/
	reg = REG_READ(SPI_CONFIG);
	reg &= ~(0xffffU << SPI_TX_TRAHS_LEN_POSI);
	reg &= ~(0xffffU << SPI_RX_TRAHS_LEN_POSI);
	k = (tx_len + rx_len);
	reg |= ((k & 0xffffU) << SPI_TX_TRAHS_LEN_POSI);
	reg |= ((k & 0xffffU) << SPI_RX_TRAHS_LEN_POSI);
	reg |= (SPI_TX_EN| SPI_RX_EN | SPI_TX_FINISH_EN);
	REG_WRITE(SPI_CONFIG, reg);

	/*      7.2:write tx fifo*/
	// write tx first
	use_less = 0;
	k = 0;
	wait = 0;
	for (int i = 0; i < tx_len;){
		reg = REG_READ(SPI_STAT);
		if ((reg & TXFIFO_WR_READ) == 0) {
			for(int j=0; j<500; j++);
			wait++;
			if(wait > 100) {
				exceptional_flag = -1;
				goto wr_exceptional;
			}
		} else {
			wait = 0;
			REG_WRITE(SPI_DAT, tx_buf[i]);
			i++;
		}

		reg = REG_READ(SPI_STAT);
		if ((reg & RXFIFO_RD_READ)) {
			if(use_less < tx_len) {
				REG_READ(SPI_DAT);
				use_less++;
			}
			else if(k < rx_len) {
				rx_buf[k] = REG_READ(SPI_DAT);
				k++;
			}
			else {
				REG_READ(SPI_DAT);
			}
		}
	}

	// read rx, send 0xff
	wait = 0;
	for (int i = 0; i < rx_len; )
	{
		reg = REG_READ(SPI_STAT);
		if ((reg & TXFIFO_WR_READ) == 0) {
			for(int j=0; j<500; j++);
			wait++;
			if(wait > 100)
			{
				exceptional_flag = -2;
				goto wr_exceptional;
			}
		}
		else {
			wait = 0;
			REG_WRITE(SPI_DAT, 0xff);
			i++;
		}

		reg = REG_READ(SPI_STAT);
		if ((reg & RXFIFO_RD_READ)) {
			if(use_less < tx_len) {
				REG_READ(SPI_DAT);
				use_less++;
			}
			else if(k < rx_len) {
				rx_buf[k] = REG_READ(SPI_DAT);
				k++;
			}
			else {
				REG_READ(SPI_DAT);
			}
		}
	}

	/*      7.3:waiting for TXFIFO_EMPTY interrupt*/
	while (1) {
		reg = REG_READ(SPI_STAT);
		if (reg & TX_FINISH_INT)
			break;
		else if ((reg & RXFIFO_RD_READ)) {
			if(use_less < tx_len) {
				REG_READ(SPI_DAT);
				use_less++;
			}
			else if(k < rx_len) {
				rx_buf[k] = REG_READ(SPI_DAT);
				k++;
			}
		}
	}

	wait = 0;
	while(k < rx_len) {
		reg = REG_READ(SPI_STAT);
		if ((reg & RXFIFO_RD_READ)) {
			if(use_less < tx_len) {
				REG_READ(SPI_DAT);
				use_less++;
			}
			else if(k < rx_len) {
				rx_buf[k] = REG_READ(SPI_DAT);
				k++;
			}
			wait = 0;
		} else {
			for(int j=0; j<500; j++);
			wait++;
			if(wait > 100) {
				break;
			}
		}
	}
	exceptional_flag = k;

	/*      7.4:release cs*/
	reg = REG_READ(SPI_CONFIG);
	reg &= ~(0xffffU << SPI_TX_TRAHS_LEN_POSI);
	//reg &= ~(0xffffU << SPI_RX_TRAHS_LEN_POSI);
	reg &= ~(SPI_TX_EN | SPI_TX_FINISH_EN);
	REG_WRITE(SPI_CONFIG, reg);

wr_exceptional:
	reg = 0;
	REG_WRITE(SPI_CONFIG, reg);

	// cler stat and fifo
	reg = REG_READ(SPI_STAT);
	while (reg & RXFIFO_RD_READ) {
		REG_READ(SPI_DAT);
		reg = REG_READ(SPI_STAT);
	}
	reg = REG_READ(SPI_STAT);
	REG_WRITE(SPI_STAT, reg);

	/*step 8, flash interface select: flash controller-->sctrl_flash_select_flash_controller()*/
	reg = REG_READ(SCTRL_CONTROL);
	reg &= ~(FLASH_SPI_MUX_BIT);
	REG_WRITE(SCTRL_CONTROL, reg);
	// recover icu and powerdown bits for spi
	REG_WRITE(ICU_PWM_CLK_MUX, reg_icu_clk);
	REG_WRITE(ICU_PERI_CLK_PWD, reg_icu_pwd);

	/*step 9, gpio(14/15/16/17) second function*/
	REG_WRITE(REG_GPIO_CFG_BASE_ADDR + GPIO14*4, gpio14_status);
	REG_WRITE(REG_GPIO_CFG_BASE_ADDR + GPIO15*4, gpio15_status);
	REG_WRITE(REG_GPIO_CFG_BASE_ADDR + GPIO16*4, gpio16_status);
	REG_WRITE(REG_GPIO_CFG_BASE_ADDR + GPIO17*4, gpio17_status);
	REG_WRITE(REG_GPIO_FUNC_CFG, gpio14_15_cfg);
	REG_WRITE(REG_GPIO_FUNC_CFG_2, gpio16_17_cfg);

	/*step 10, restore spi register configuration*/
	REG_WRITE(SPI_CTRL, reg_ctrl);
	REG_WRITE(SPI_CONFIG, reg_config);
	REG_WRITE(SPI_STAT, reg_stat);
	REG_WRITE(SPI_DAT, reg_dat);

	/*step 11, enable interrupt*/
	REG_WRITE(ICU_GLOBAL_INT_EN, intc_enable_reg);

	return exceptional_flag;
}

int flash_bypass_op_write(uint8_t *op_code, uint8_t *tx_buf, uint32_t tx_len)
{
	uint32_t intc_enable_reg, reg;
	uint32_t reg_ctrl, reg_dat;
	uint32_t reg_stat, reg_config, reg_icu_clk, reg_icu_pwd;
	uint32_t gpio14_status, gpio15_status, gpio16_status, gpio17_status;
	uint32_t gpio14_15_cfg, gpio16_17_cfg;
	int exceptional_flag = 0;

	while(REG_READ(REG_FLASH_OPERATE_SW) & BUSY_SW)
	{
		;
	}

	/*step 0, disable interrupt*/
	intc_enable_reg = REG_READ(ICU_GLOBAL_INT_EN);
	reg = intc_enable_reg & (~(GINTR_FIQ_EN | GINTR_IRQ_EN));
	REG_WRITE(ICU_GLOBAL_INT_EN, reg);

	/*step 1, save spi register configuration*/
	reg_ctrl = REG_READ(SPI_CTRL);
	reg_config = REG_READ(SPI_CONFIG);
	reg_stat = REG_READ(SPI_STAT);
	reg_dat  = REG_READ(SPI_DAT);

	/*step 2, resident cache*/
	// link this function to itcm on bk7238

	/*step 3, config spi master*/
	/*     3.1 clear spi fifo content*/
	reg = REG_READ(SPI_STAT);
	while (reg & RXFIFO_RD_READ) {
		REG_READ(SPI_DAT);
		reg = REG_READ(SPI_STAT);
	}

	/*     3.2 disable spi block, and backup */
	reg_icu_pwd = reg = REG_READ(ICU_PERI_CLK_PWD);
	reg |= (PWD_SPI_CLK_BIT);
	REG_WRITE(ICU_PERI_CLK_PWD, reg);

	/*     3.3 clear spi status*/
	REG_WRITE(SPI_CTRL, 0);
	REG_WRITE(SPI_CONFIG, 0);
	reg = REG_READ(SPI_STAT);
	REG_WRITE(SPI_STAT, reg);

	/*     3.4 save the previous setting status*/
	gpio14_status = REG_READ(REG_GPIO_CFG_BASE_ADDR + GPIO14*4);
	gpio15_status = REG_READ(REG_GPIO_CFG_BASE_ADDR + GPIO15*4);
	gpio16_status = REG_READ(REG_GPIO_CFG_BASE_ADDR + GPIO16*4);
	gpio17_status = REG_READ(REG_GPIO_CFG_BASE_ADDR + GPIO17*4);
	gpio14_15_cfg = REG_READ(REG_GPIO_FUNC_CFG);
	gpio16_17_cfg = REG_READ(REG_GPIO_FUNC_CFG_2);

	/*     3.5 set the spi master mode*/
	// open clock, and backup
	reg = reg_icu_clk = REG_READ(ICU_PWM_CLK_MUX);
	reg |= PCLK_POSI_SPI;
	REG_WRITE(ICU_PWM_CLK_MUX, reg);
	// select 26M
	reg = REG_READ(ICU_PERI_CLK_PWD);
	reg &= ~(PWD_SPI_CLK_BIT);
	REG_WRITE(ICU_PERI_CLK_PWD, reg);

	// set to spi config directly
	reg = 0xC00100; // spien  msten  spi_clk=1---13M
	REG_WRITE(SPI_CTRL, reg);

	/*step 4, gpio(14/15/16/17) are set as high-impedance state or input state ,
				for spi mux with them*/
	gpio_config(GPIO14, GMODE_SET_HIGH_IMPENDANCE);
	gpio_config(GPIO15, GMODE_SET_HIGH_IMPENDANCE);
	gpio_config(GPIO16, GMODE_SET_HIGH_IMPENDANCE);
	gpio_config(GPIO17, GMODE_SET_HIGH_IMPENDANCE);

	/*step 5, switch flash interface to spi:sctrl_flash_select_spi_controller()
		*        Pay attention to prefetch instruction destination, the text can not
		*        fetch from flash space after this timepoint.
		*/
	reg = REG_READ(SCTRL_CONTROL);
	reg |= FLASH_SPI_MUX_BIT;
	REG_WRITE(SCTRL_CONTROL, reg);
	while (1) { /* double check the bit:FLASH_SPI_MUX_BIT. The bit cannot be set successfully, if cpu is fetching instruction*/
		reg = REG_READ(SCTRL_CONTROL);
		if (reg & FLASH_SPI_MUX_BIT)
			break;
	}

	if(op_code != NULL)
	{
		/*step 6, write enable for volatile status register: 50H*/
		/*      6.1:take cs*/
		reg = REG_READ(SPI_CONFIG);
		reg &= ~(0xffff << SPI_TX_TRAHS_LEN_POSI);
		reg |= (1 << SPI_TX_TRAHS_LEN_POSI);
		reg |= (SPI_TX_EN | SPI_TX_FINISH_EN);
		REG_WRITE(SPI_CONFIG, reg);

		/*      6.2:write tx fifo*/
		reg = REG_READ(SPI_STAT);
		if (reg & TXFIFO_WR_READ)
			REG_WRITE(SPI_DAT, *op_code);
		else {
			exceptional_flag = -1;
			goto wr_exceptional;
		}

		/*      6.3:waiting for TXFIFO_EMPTY interrupt*/
		while (1) {
			reg = REG_READ(SPI_STAT);
			if (reg & TX_FINISH_INT)
				break;
		}

		/*      6.4:release cs*/
		reg = REG_READ(SPI_CONFIG);
		reg &= ~(0xffff << SPI_TX_TRAHS_LEN_POSI);
		reg &= ~(SPI_TX_EN | SPI_TX_FINISH_EN);
		REG_WRITE(SPI_CONFIG, reg);

		// cler stat and fifo
		reg = REG_READ(SPI_STAT);
		while (reg & RXFIFO_RD_READ) {
			REG_READ(SPI_DAT);
			reg = REG_READ(SPI_STAT);
		}
		reg = REG_READ(SPI_STAT);
		REG_WRITE(SPI_STAT, reg);
	}

	/*step 7, write flash command:01H and sr*/
	/*      7.1:take cs*/
	reg = REG_READ(SPI_CONFIG);
	reg &= ~(0xffffU << SPI_TX_TRAHS_LEN_POSI);
	reg &= ~(0xffffU << SPI_RX_TRAHS_LEN_POSI);
	reg |= ((tx_len & 0xffffU) << SPI_TX_TRAHS_LEN_POSI);
	reg |= (SPI_TX_EN | SPI_TX_FINISH_EN);
	REG_WRITE(SPI_CONFIG, reg);

	/*      7.2:write tx fifo*/
	// write tx first
	for (int i = 0, wait = 0; i < tx_len; ) {
		reg = REG_READ(SPI_STAT);
		if ((reg & TXFIFO_WR_READ) == 0) {
			for(int j=0; j<500; j++);
			wait++;
			if(wait > 100) {
				exceptional_flag = -2;
				goto wr_exceptional;
			}
		} else {
			wait = 0;
			REG_WRITE(SPI_DAT, tx_buf[i]);
			i++;
		}
	}

	/*      7.3:waiting for TXFIFO_EMPTY interrupt*/
	while (1) {
		reg = REG_READ(SPI_STAT);
		if (reg & TX_FINISH_INT)
			break;
	}

	/*      7.4:release cs*/
	reg = REG_READ(SPI_CONFIG);
	reg &= ~(0xffffU << SPI_TX_TRAHS_LEN_POSI);
	reg &= ~(0xffffU << SPI_RX_TRAHS_LEN_POSI);
	reg &= ~(SPI_TX_EN | SPI_RX_EN | SPI_TX_FINISH_EN);
	REG_WRITE(SPI_CONFIG, reg);

	// cler stat and fifo
	reg = REG_READ(SPI_STAT);
	while (reg & RXFIFO_RD_READ) {
		REG_READ(SPI_DAT);
		reg = REG_READ(SPI_STAT);
	}
	reg = REG_READ(SPI_STAT);
	REG_WRITE(SPI_STAT, reg);
	exceptional_flag = 0;

wr_exceptional:
	/*step 8, flash interface select: flash controller-->sctrl_flash_select_flash_controller()*/
	reg = REG_READ(SCTRL_CONTROL);
	reg &= ~(FLASH_SPI_MUX_BIT);
	REG_WRITE(SCTRL_CONTROL, reg);
	// recover icu and powerdown bits for spi
	REG_WRITE(ICU_PWM_CLK_MUX, reg_icu_clk);
	REG_WRITE(ICU_PERI_CLK_PWD, reg_icu_pwd);

	/*step 9, gpio(14/15/16/17) second function*/
	REG_WRITE(REG_GPIO_CFG_BASE_ADDR + GPIO14*4, gpio14_status);
	REG_WRITE(REG_GPIO_CFG_BASE_ADDR + GPIO15*4, gpio15_status);
	REG_WRITE(REG_GPIO_CFG_BASE_ADDR + GPIO16*4, gpio16_status);
	REG_WRITE(REG_GPIO_CFG_BASE_ADDR + GPIO17*4, gpio17_status);
	REG_WRITE(REG_GPIO_FUNC_CFG, gpio14_15_cfg);
	REG_WRITE(REG_GPIO_FUNC_CFG_2, gpio16_17_cfg);

	/*step 10, restore spi register configuration*/
	REG_WRITE(SPI_CTRL, reg_ctrl);
	REG_WRITE(SPI_CONFIG, reg_config);
	REG_WRITE(SPI_STAT, reg_stat);
	REG_WRITE(SPI_DAT, reg_dat);

	/*step 11, enable interrupt*/
	REG_WRITE(ICU_GLOBAL_INT_EN, intc_enable_reg);

	return exceptional_flag;
}

int flash_bypass_wr_sr_cb(uint32_t larg, uint32_t rarg)
{
	uint8_t sr_width = (uint8_t)larg;
	uint8_t sr_val[3];
	int ret = 0;

	if (0 == flash_is_support_0x50h_cmd() || (sr_width > 2))
		return 0;

	uint8_t op_code = FLASH_CMD_WR_EN_VSR;
	sr_val[0] = FLASH_CMD_WR_SR;
	sr_val[1] = rarg & 0xff;
	sr_val[2] = ((rarg>>8) & 0xff);

	ret = flash_bypass_op_write(&op_code, sr_val, sr_width+1);
	if(ret == 0)// success
	{
		ret = 1;
	}

	return ret;
}

#if (CFG_SOC_NAME == SOC_BK7238) || (CFG_SOC_NAME == SOC_BK7231N)
#if CFG_USE_FLASH_OTP
uint32_t otp_test_flag = 0;
int flash_bypass_otp_operation(OTP_CMD_CODE cmd, void *parm)
{
	uint8_t ret = OTP_OPERATION_SUCCESS;
	otp_ctrl_t *otp_cfg = (otp_ctrl_t *)parm;
	uint8_t otp_code = FLASH_CMD_WR_EN;
	uint32_t flash_id;

	flash_id = flash_get_id();
    // zg_th25q16b || gd_25q16c
    if ((0xEB6015 == flash_id) || (0xc84015 == flash_id))
	{
		if (otp_cfg->otp_index > 1)
			return -1;
    }

	switch(cmd) {
		case CMD_OTP_READ: {
			if(otp_test_flag) {
				break;
			}
			otp_test_flag = 1;

			uint8_t  tx_val[FLASH_OTP_READ_TASK_LEN] = {0};
			uint8_t  tx_len  = FLASH_OTP_READ_TASK_LEN;
			uint32_t tx_addr = otp_cfg->byte_offset;
			// puya flash
			if((0x854215 == flash_id) || (0x856015 == flash_id) || (0x852015 == flash_id))
				tx_addr += FLASH_OTP_IDX_X_ADDR(otp_cfg->otp_index);

			tx_val[0] = FLASH_CMD_OTP_READ;
			tx_val[1] = (tx_addr >> 16) & 0xff;
			tx_val[2] = (tx_addr >>  8) & 0xff;
			tx_val[3] = (tx_addr >>  0) & 0xff;

			uint8_t *rx_val = (uint8_t *) os_malloc(otp_cfg->read_data_len);
			uint32_t rx_len = otp_cfg->read_data_len;

			null_prf("%d\n", __LINE__);
			delay_ms(40);
			flash_bypass_op_read(tx_val, tx_len, rx_val, rx_len);
			delay_ms(20);

			// uint8_t printf_div = 16;
			// for (uint32_t i = 0; i < rx_len; i++)
			// {
			// 	bk_printf("%02x,", rx_val[i]);
			// 	if ((i + 1) % printf_div == 0) {
			// 		bk_printf("\n");
			// 	}
			// }
			// bk_printf("\n");

			os_memcpy(otp_cfg->read_data, rx_val, rx_len);

			os_free(rx_val);
			otp_test_flag = 0;
			break;
		}

		case CMD_OTP_EARSE: {
			if(otp_test_flag) {
				break;
			}
			otp_test_flag = 1;

			/* step1 earse the otp, make sure the otp can be writed */
			uint8_t  earse_cmd[FLASH_OTP_EARSE_TASK_LEN] = {0};
			uint8_t  earse_cmd_len  = FLASH_OTP_EARSE_TASK_LEN;
			uint32_t earse_addr = 0;
			// puya flash
			if((0x854215 == flash_id) || (0x856015 == flash_id) || (0x852015 == flash_id))
				earse_addr += FLASH_OTP_IDX_X_ADDR(otp_cfg->otp_index);

			earse_cmd[0] = FLASH_CMD_OTP_EARSE;
			earse_cmd[1] = (earse_addr >> 16) & 0xff;
			earse_cmd[2] = (earse_addr >>  8) & 0xff;
			earse_cmd[3] = (earse_addr >>  0) & 0xff;

			null_prf("%d\n", __LINE__);
			delay_ms(40);
			flash_bypass_op_write(&otp_code, earse_cmd, earse_cmd_len);
			delay_ms(500);
			flash_bypass_op_write(&otp_code, earse_cmd, earse_cmd_len);
			delay_ms(500);

			otp_test_flag = 0;
			break;
		}

		case CMD_OTP_WRITE: {
			if(otp_test_flag) {
				break;
			}
			otp_test_flag = 1;

			/* step1 earse the otp, make sure the otp can be writed */
			uint8_t  earse_cmd[FLASH_OTP_EARSE_TASK_LEN] = {0};
			uint8_t  earse_cmd_len  = FLASH_OTP_EARSE_TASK_LEN;
			uint32_t earse_addr = 0;
			// puya flash
			if((0x854215 == flash_id) || (0x856015 == flash_id) || (0x852015 == flash_id))
				earse_addr += FLASH_OTP_IDX_X_ADDR(otp_cfg->otp_index);

			earse_cmd[0] = FLASH_CMD_OTP_EARSE;
			earse_cmd[1] = (earse_addr >> 16) & 0xff;
			earse_cmd[2] = (earse_addr >>  8) & 0xff;
			earse_cmd[3] = (earse_addr >>  0) & 0xff;

			null_prf("%d\n", __LINE__);
			delay_ms(40);
			flash_bypass_op_write(&otp_code, earse_cmd, earse_cmd_len);
			delay_ms(500);
			flash_bypass_op_write(&otp_code, earse_cmd, earse_cmd_len);
			delay_ms(500);

			/* step2 write the new read_data to otp */
			uint8_t *write_cmd = (uint8_t *) os_malloc(FLASH_OTP_WRITE_HEAD_LEN + FLASH_OTP_WRITE_BYTE_LEN_MAX);
			uint32_t write_cmd_len  = FLASH_OTP_WRITE_HEAD_LEN + FLASH_OTP_WRITE_BYTE_LEN_MAX;

			if (otp_cfg->write_data_len > FLASH_OTP_WRITE_BYTE_LEN_MAX)
			{
				for (uint32 i = 0; i < otp_cfg->write_data_len / FLASH_OTP_WRITE_BYTE_LEN_MAX; i ++)
				{
					uint32_t write_addr = otp_cfg->byte_offset + FLASH_OTP_WRITE_BYTE_LEN_MAX * i;

					// puya flash
					if((0x854215 == flash_id) || (0x856015 == flash_id) || (0x852015 == flash_id))
						write_addr += FLASH_OTP_IDX_X_ADDR(otp_cfg->otp_index);

					write_cmd[0] = FLASH_CMD_OTP_WRITE;
					write_cmd[1] = (write_addr >> 16) & 0xff;
					write_cmd[2] = (write_addr >>  8) & 0xff;
					write_cmd[3] = (write_addr >>  0) & 0xff;

					os_memcpy(write_cmd + 4, otp_cfg->write_data + (FLASH_OTP_WRITE_BYTE_LEN_MAX * i), FLASH_OTP_WRITE_BYTE_LEN_MAX);

					null_prf("%d\n", __LINE__);
					delay_ms(40);
					flash_bypass_op_write(&otp_code, write_cmd, write_cmd_len);
					delay_ms(40);
					flash_bypass_op_write(&otp_code, write_cmd, write_cmd_len);
					delay_ms(20);
				}
				if (otp_cfg->write_data_len % FLASH_OTP_WRITE_BYTE_LEN_MAX)
				{
					write_cmd_len  = FLASH_OTP_WRITE_HEAD_LEN + otp_cfg->write_data_len % FLASH_OTP_WRITE_BYTE_LEN_MAX;
					os_memset(write_cmd, 0, write_cmd_len);
					uint32_t write_addr = otp_cfg->byte_offset + FLASH_OTP_WRITE_BYTE_LEN_MAX * (otp_cfg->write_data_len / FLASH_OTP_WRITE_BYTE_LEN_MAX);
					// puya flash
					if((0x854215 == flash_id) || (0x856015 == flash_id) || (0x852015 == flash_id))
						write_addr += FLASH_OTP_IDX_X_ADDR(otp_cfg->otp_index);

					write_cmd[0] = FLASH_CMD_OTP_WRITE;
					write_cmd[1] = (write_addr >> 16) & 0xff;
					write_cmd[2] = (write_addr >>  8) & 0xff;
					write_cmd[3] = (write_addr >>  0) & 0xff;

					os_memcpy(write_cmd + 4, otp_cfg->write_data + (FLASH_OTP_WRITE_BYTE_LEN_MAX * (otp_cfg->write_data_len / FLASH_OTP_WRITE_BYTE_LEN_MAX)), otp_cfg->write_data_len % FLASH_OTP_WRITE_BYTE_LEN_MAX);

					null_prf("%d\n", __LINE__);
					delay_ms(40);
					flash_bypass_op_write(&otp_code, write_cmd, write_cmd_len);
					delay_ms(40);
					flash_bypass_op_write(&otp_code, write_cmd, write_cmd_len);
					delay_ms(20);
				}
			}
			else
			{
				if (otp_cfg->write_data_len % FLASH_OTP_WRITE_BYTE_LEN_MAX)
				{
					write_cmd_len  = FLASH_OTP_WRITE_HEAD_LEN + otp_cfg->write_data_len % FLASH_OTP_WRITE_BYTE_LEN_MAX;
					os_memset(write_cmd, 0, write_cmd_len);
					uint32_t write_addr = otp_cfg->byte_offset;
					// puya flash
					if((0x854215 == flash_id) || (0x856015 == flash_id) || (0x852015 == flash_id))
						write_addr += FLASH_OTP_IDX_X_ADDR(otp_cfg->otp_index);
					write_cmd[0] = FLASH_CMD_OTP_WRITE;
					write_cmd[1] = (write_addr >> 16) & 0xff;
					write_cmd[2] = (write_addr >>  8) & 0xff;
					write_cmd[3] = (write_addr >>  0) & 0xff;

					os_memcpy(write_cmd + 4, otp_cfg->write_data, otp_cfg->write_data_len);

					null_prf("%d\n", __LINE__);
					delay_ms(40);
					flash_bypass_op_write(&otp_code, write_cmd, write_cmd_len);
					delay_ms(40);
					flash_bypass_op_write(&otp_code, write_cmd, write_cmd_len);
					delay_ms(20);
				}
			}
			os_free(write_cmd);
			otp_test_flag = 0;
			break;
		}

		case CMD_OTP_LOCK: {
			if(otp_test_flag) {
				break;
			}
			otp_test_flag = 1;
			uint8_t  write_cmd[3]  = {0};
			uint8_t  write_cmd_len;

			#if 0
			// puya flash
			if((0x854215 == flash_id) || (0x856015 == flash_id) || (0x852015 == flash_id)) {
				uint8_t  read_cmd = FLASH_CMD_OTP_STATE_READ_H;
				uint8_t  read_cmd_len = FLASH_OTP_READ_STATE_TASK_LEN;

				uint8_t  read_data = 0;
				uint8_t  read_data_len = FLASH_OTP_READ_STATE_TASK_LEN;

				null_prf("%d\n", __LINE__);
				delay_ms(40);
				flash_bypass_op_read(&read_cmd, read_cmd_len, &read_data, read_data_len);
				delay_ms(40);
				flash_bypass_op_read(&read_cmd, read_cmd_len, &read_data, read_data_len);
				delay_ms(20);

				os_printf("====== read_data[1]: 0x%x\n", read_data);
				write_cmd_len = 2;

				write_cmd[0] = FLASH_CMD_OTP_STATE_WRITE_H;
				write_cmd[1] = read_data | (1 << (FLASH_OTP_STATE_OFFSET + otp_cfg->otp_index));
			}
			else if ((0xEB6015 == flash_id) || (0xc84015 == flash_id)) 
			#endif
			{
				// zg_th25q16b && gd_25q16c
				uint8_t  read_cmd = FLASH_CMD_OTP_STATE_READ_H;
				uint8_t  read_cmd_len = FLASH_OTP_READ_STATE_TASK_LEN;

				uint8_t  read_data_high = 0;
				uint8_t  read_data_high_len = FLASH_OTP_READ_STATE_TASK_LEN;

				null_prf("%d\n", __LINE__);
				delay_ms(40);
				flash_bypass_op_read(&read_cmd, read_cmd_len, &read_data_high, read_data_high_len);
				delay_ms(40);
				flash_bypass_op_read(&read_cmd, read_cmd_len, &read_data_high, read_data_high_len);
				delay_ms(20);

				read_cmd = FLASH_CMD_OTP_STATE_READ_L;
				read_cmd_len = FLASH_OTP_READ_STATE_TASK_LEN;

				uint8_t  read_data_low = 0;
				uint8_t  read_data_low_len = FLASH_OTP_READ_STATE_TASK_LEN;

				null_prf("%d\n", __LINE__);
				delay_ms(40);
				flash_bypass_op_read(&read_cmd, read_cmd_len, &read_data_low, read_data_low_len);
				delay_ms(40);
				flash_bypass_op_read(&read_cmd, read_cmd_len, &read_data_low, read_data_low_len);
				delay_ms(20);

				write_cmd_len = 3;

				write_cmd[0] = FLASH_CMD_OTP_STATE_WRITE_L;
				write_cmd[1] = read_data_low;
				write_cmd[2] = read_data_high | (1 << FLASH_OTP_STATE_OFFSET);

			}

			null_prf("%d\n", __LINE__);
			delay_ms(40);
			flash_bypass_op_write(&otp_code, write_cmd, write_cmd_len);
			delay_ms(40);
			flash_bypass_op_write(&otp_code, write_cmd, write_cmd_len);
			delay_ms(20);

			otp_test_flag = 0;
			break;
		}

		case CMD_OTP_READ_XBYTE: {
			uint8_t  tx_val[FLASH_OTP_READ_TASK_LEN] = {0};
			uint8_t  tx_len  = FLASH_OTP_READ_TASK_LEN;
			uint32_t tx_addr = otp_cfg->byte_offset;
			// puya flash
			if((0x854215 == flash_id) || (0x856015 == flash_id) || (0x852015 == flash_id))
				tx_addr += FLASH_OTP_IDX_X_ADDR(otp_cfg->otp_index);

			tx_val[0] = FLASH_CMD_OTP_READ;
			tx_val[1] = (tx_addr >> 16) & 0xff;
			tx_val[2] = (tx_addr >>  8) & 0xff;
			tx_val[3] = (tx_addr >>  0) & 0xff;

			uint8_t *rx_val = (uint8_t *) os_malloc(otp_cfg->read_data_len);
			uint32_t rx_len = otp_cfg->read_data_len;
			if (rx_val == NULL)
			{
				bk_printf("ERROR: memory allocation failed!\n");
			}

			null_prf("%d\n", __LINE__);
			delay_ms(10);
			flash_bypass_op_read(tx_val, tx_len, rx_val, rx_len);

			os_memcpy(otp_cfg->read_data, rx_val, otp_cfg->read_data_len);
			os_free(rx_val);

			break;
		}


		default:
			ret = OTP_OPERATION_FAILURE;
			break;
	}

	if(ret == 0)// success
	{
		ret = 1;
	}
	// bk_printf("otp operation ret = 0x%d\n", ret);
	return ret;
}
#endif // CFG_USE_FLASH_OTP
#endif
#endif

#if ((CFG_SOC_NAME == SOC_BK7221U) || (CFG_SOC_NAME == SOC_BK7238))  || (CFG_SOC_NAME == SOC_BK7231N)
uint32_t flash_bypass_operate_sr_init(void)
{
	flash_register_bypass_cb(flash_bypass_wr_sr_cb);
	return 0;
}
#endif
// eof


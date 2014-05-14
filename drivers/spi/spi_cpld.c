/*#define DEBUG*/
#define pr_fmt(fmt) "[CPLD] " fmt
#define pr_error(fmt) pr_err("ERROR: " fmt)

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/spi/cpld.h>
#include <linux/slab.h>
#include <linux/delay.h>


//0117
//#include <mach/clk.h>
//#include <mach/socinfo.h>

//#include "proc_comm.h"
//#include "clock.h"
//#include "mach-msm/clock-pcom.h"

#define CPLD_BUFSIZ		128

#define INTF_SPI		0
#define INTF_UART		1

#define TMUX_EBI2_OFFSET		0x204
#define EBI2_XMEM_CS0_CFG1_OFFSET	0x10028

struct cpld_driver {
	struct kobject		kobj;
	struct device		*dev;

	void __iomem		*cfg_base;
	void __iomem		*clk_base;
	void __iomem		*reg_base;
	void __iomem		*gpio_base;
	void __iomem		*sdc4_base;

	phys_addr_t		cfg_start;

	struct spi_device	*spi;
	struct spi_message	msg;
	struct spi_transfer	xfer;
	uint8_t			buf[CPLD_BUFSIZ];

	uint8_t			sysfs_buf[CPLD_BUFSIZ];
	int			sysfs_cnt;
	int			cfg_offs;
	int			clk_offs;
	int			gpio_offs;
	uint8_t			reg_addr;

	struct cpld_platform_data *pdata;
};

typedef volatile struct _cpld_reg_ {

	unsigned short tx_buffer1_cs_low;	//0x0
	unsigned short tx_buffer2_cs_low;	//0x2
	unsigned short version1_1;		//0x4
	unsigned short rx_buffer1_cs_low;	//0x6
	unsigned short tx_buffer1_cs_high;	//0x8
	unsigned short tx_buffer2_cs_high;	//0xA
	unsigned short version1_2;		//0xC
	unsigned short rx_buffer1_cs_high;	//0xE

} cpld_reg, *p_cpld_reg;

typedef enum _buf_num_ {
	tx_buffer_1,
	tx_buffer_2,
}enum_buf_num;

typedef struct _cpld_manager_
{
	p_cpld_reg p_cpld_reg;
	enum_buf_num current_tx_buf_num;
}cpld_manager;

cpld_manager g_cpld_manager =  { 0 };
static struct cpld_driver *g_cpld;

#define GPIO_SPI 0
#define EBI2_SPI 1


static void clock_setting(struct cpld_driver *cpld, int enable)
{
	pr_info("%s\n", __func__);
	
	if (cpld) {
		pr_info("CPLD true: call clock_set(%d)\n", enable);
		cpld->pdata->clock_set(enable);

		if(enable) {
			writel(0x01, cpld->sdc4_base);			/* MCI_POWER */
			writel(0x18100, cpld->sdc4_base + 0x04);	/* MCI_CLK */
		} else {
			writel(0x00, cpld->sdc4_base);			/* MCI_POWER */
			writel(0x08000, cpld->sdc4_base + 0x04);	/* MCI_CLK */
		}
	}
	else
	{
		pr_err("%s. Poiner is NULL.\n", __func__);
	}
}

int spi_set_route(int path)
{
	struct cpld_driver *cpld = g_cpld;
//    int gpio = cpld->pdata->intf_select;

	clock_setting(cpld, 1);
	
	if (path == EBI2_SPI) {
		if(cpld->pdata->intf_select)
			gpio_direction_output(cpld->pdata->intf_select, 1);
	
		writel(0, cpld->gpio_base + TMUX_EBI2_OFFSET);
	} else {
		if(cpld->pdata->intf_select)
			gpio_direction_output(cpld->pdata->intf_select, 0);

		writel(0x10, cpld->gpio_base + TMUX_EBI2_OFFSET);
	}

	pr_info("TMUX_EBI2: 0x%08x\n", readl(cpld->cfg_base));
	
	return 0;
}

EXPORT_SYMBOL(spi_set_route);


int gpio_spi_write(int length, unsigned char * buffer)
{
	struct spi_device *spi = g_cpld->spi;
	struct spi_message msg;
	struct spi_transfer xfer;
	int err = -1;
	unsigned char *buf;
	int len = 0;

	if (!buffer) {
		pr_info("%s. Pointer is NULL.\n", __func__);
		return err;
	}
	
	buf = buffer;
	len = length;
	
	xfer.tx_buf = buf;
	pr_info("%s: len = %d, bits = %d\n", __func__,
				len, xfer.bits_per_word);

	memset(&xfer, 0, sizeof(xfer));
	xfer.tx_buf = buf;
	xfer.len = len;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);

	err = spi_sync(spi, &msg);
	pr_info("%s: err = %d\n", __func__, err);

	return err;
}
EXPORT_SYMBOL(gpio_spi_write);

#if 0
static cpld_gpiospi_single_write(struct cpld_driver *cpld,
				uint16_t index, uint8_t value)
{
#if 0
	struct spi_device *spi = cpld->spi;
	struct spi_message msg;
	struct spi_transfer xfer;
	uint8_t buf[4];
	int err;

	buf[0] = 0x61;
	buf[1] = (index >> 8) & 0xff;
	buf[2] = index & 0xff;
	buf[3] = value;

	xfer.bits_per_word = 0;
	xfer.tx_buf = buf;

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);

	err = spi_sync(spi, &msg);

	pr_info("%s: err = %d\n", __func__, err);
	return err;
#else
	struct spi_device *spi = cpld->spi;
	struct spi_message *msg = &cpld.msg;
	struct spi_transfer *xfer = &cpld.xfer;
	uint8_t buf[4];
	int err;

	buf[0] = 0x60;
	buf[1] = (index >> 8) & 0xff;
	buf[2] = index & 0xff;
	buf[3] = value;

	xfer->tx_buf = buf;
	xfer->rx_buf = NULL;

	/*spi_message_add_tail(&cpld.xfer, &cpld.msg);*/

	err = spi_sync(spi, msg);

	pr_info("%s: err = %d\n", __func__, err);
	return err;
#endif
}
#endif

static int cpld_gpiospi_single_read(struct cpld_driver *cpld,
				uint16_t index, uint8_t *value)
{
	struct spi_message msg;
	int err;
	uint8_t tx_buf[3] = { 0x60, (index >> 8) & 0xff, index & 0xff };
	uint8_t rx_cmd = 0x61;
	struct spi_transfer xfer[] = {
		{
			.tx_buf = tx_buf,
			.rx_buf = NULL,
			.len = 3,
			.cs_change = 1,
			.bits_per_word = 8,
		}, {
			.tx_buf = &rx_cmd,
			.rx_buf = NULL,
			.len = 1,
			.cs_change = 0,
			.bits_per_word = 8,
		}, {
			.tx_buf = NULL,
			.rx_buf = value,
			.len = 1,
			.cs_change = 1,
			.bits_per_word = 8,
		},
	};

	spi_message_init(&msg);

	spi_message_add_tail(&xfer[0], &msg);
	spi_message_add_tail(&xfer[1], &msg);
	spi_message_add_tail(&xfer[2], &msg);

	err = spi_sync(cpld->spi, &msg);

	pr_info("%s: err = %d, value = 0x%x\n", __func__, err, *value);
	return err;
}

int gpio_spi_read(int length, unsigned char *buffer)
{
#if 1
	struct spi_device *spi = g_cpld->spi;
	struct spi_message msg;
	
	int err = -1;
	uint8_t tx_buf[3];
	uint8_t rx_cmd;
	struct spi_transfer xfer[3];
	unsigned char *buf;
	int len = 0;

	if (!buffer) {
		pr_info("%s. Pointer is NULL.\n", __func__);
		return err;
	}
	
	len = length;
	
	if (!buffer && len > 4){

		pr_err("%s.Failed. len:%d.\n", __func__, len);
		
		return err;
	}

	buf = buffer;

	memcpy(tx_buf, buf, sizeof(tx_buf));

	rx_cmd = *(buf+3);

	buf += 4;
	len = len - 4;

	xfer[0].tx_buf = tx_buf;
	xfer[0].rx_buf = NULL;
	xfer[0].len = 3;
	xfer[0].cs_change = 1;
	xfer[0].bits_per_word = 8;

	xfer[1].tx_buf = &rx_cmd;
	xfer[1].rx_buf = NULL;
	xfer[1].len = 1;
	xfer[1].cs_change = 0;
	xfer[1].bits_per_word = 8;

	
	xfer[2].tx_buf = NULL;
	xfer[2].rx_buf = buf;
	xfer[2].len = len;
	xfer[2].cs_change = 1;
	xfer[2].bits_per_word = 8;

	spi_message_init(&msg);

	spi_message_add_tail(&xfer[0], &msg);
	spi_message_add_tail(&xfer[1], &msg);
	spi_message_add_tail(&xfer[2], &msg);

	err = spi_sync(spi, &msg);

	pr_info("%s: err = %d\n", __func__, err);

	if (err)
	{
		err = -1;
	}

	return err;

#else
	struct spi_transfer *x = &lcd->xfer;
	int err = 0;

	if (len > CPLD_BUFSIZ)
		return -EINVAL;

	g_cpld->buf[0] = (data >> 8) & 0xff;
	g_cpld->buf[1] = data & 0xff;
	x->len = 2;
	err = spi_sync(g_cpld->spi, &g_cpld->msg);

	return 0;
#endif
}
EXPORT_SYMBOL(gpio_spi_read);

#if 1
int cpld_spi_write(int length, unsigned char *buffer)
{
	unsigned short data = 0;
	unsigned char *pBuf;
	unsigned short byte_high = 0;
	unsigned short byte_low = 0;
	int i = 0;
	int len = 0;

	len = length;
	
	//pr_info("===start to write!===, len:%d\n", len);
	
	if (!buffer || len <= 0) {
		pr_info("The parameter is wrong!\n");
		return (-1);
	}

	pBuf = buffer;
	
	for (i = 0; i < len; i++) {
		#if 1
		//pr_info("cpld_spi_write(): no offset\n");
		byte_low =  (*pBuf) & 0x00ff;
		byte_high = ((*pBuf) & 0xff00);
		#else
		byte_low =  (*pBuf) & 0x003f;
		byte_high = ((*pBuf) & 0x00c0) << 4;
		#endif
		data = byte_high | byte_low;
#if 0
		pr_info("i:%d, len-1:%d\n", i, len-1);
		pr_info("*pBuf:0x%x, byte_high:0x%x, byte_low:0x%x\n", *pBuf, byte_high, byte_low);			
		pr_info("*pBuf:0x%x, data:0x%x, tx_buff_num:%d\n", *pBuf, data, g_cpld_manager.current_tx_buf_num);
#endif
		if (i == (len - 1)) {
			if (tx_buffer_1 == g_cpld_manager.current_tx_buf_num)
			{
				g_cpld_manager.current_tx_buf_num = tx_buffer_2;
				g_cpld_manager.p_cpld_reg->tx_buffer2_cs_high = data;
			}
			else if (tx_buffer_2 == g_cpld_manager.current_tx_buf_num)
			{
				g_cpld_manager.current_tx_buf_num = tx_buffer_1;
				g_cpld_manager.p_cpld_reg->tx_buffer1_cs_high = data;
			}
			else
			{
				g_cpld_manager.current_tx_buf_num = tx_buffer_1;
				g_cpld_manager.p_cpld_reg->tx_buffer1_cs_high = data;
			}
			udelay(5);

			break;
		}

		
		if (tx_buffer_1 == g_cpld_manager.current_tx_buf_num)
		{
			g_cpld_manager.current_tx_buf_num = tx_buffer_2;
			g_cpld_manager.p_cpld_reg->tx_buffer2_cs_low = data;
		}
		else if (tx_buffer_2 == g_cpld_manager.current_tx_buf_num)
		{
			g_cpld_manager.current_tx_buf_num = tx_buffer_1;
			g_cpld_manager.p_cpld_reg->tx_buffer1_cs_low = data;
		}
		else
		{
			g_cpld_manager.current_tx_buf_num = tx_buffer_1;
			g_cpld_manager.p_cpld_reg->tx_buffer1_cs_low = data;
		}

		udelay(1);
		pBuf++;
	}

	//pr_info("===end to write!===\n");
	
	return len;
}

#else
int cpld_ebi2spi_write(unsigned char *buf, int len)
{
	/*struct cpld_driver *cpld = g_cpld;*/
	int i;

	switch_interface(INTF_SPI);

	for (i = 0; i < len; i++) {
		;	/* TODO: Write ebi2 data here */
	}

	switch_interface(INTF_UART);

	return 0;
}
#endif

EXPORT_SYMBOL(cpld_spi_write);

#if 1
int cpld_spi_read(int length, unsigned char *buffer)
{
	int i = 0;
	unsigned short data = 0;
	unsigned short byte_high = 0;
	unsigned short byte_low = 0;
	unsigned char *pBuf; 
    int count = 0;
	int write_count = 0;
	int len = 0;

	len = length;

	write_count = 3;

	//pr_info("===read=== len:%d\n", len);
	 
	if (!buffer || len <= (write_count + 1)) {
		pr_error("The parameter is wrong!\n");
		return (-1);
	}

	pBuf = buffer;
	
	cpld_spi_write(write_count, pBuf);


	//pr_info("===start to write addrewss for reading!===\n");
	count = len - write_count;
	pBuf = pBuf + write_count;
#if 1
	//byte_low =  (*pBuf) & 0x003f;
	//byte_high = ((*pBuf) & 0x00c0);
	byte_low =  (*pBuf);
	byte_high = ((*pBuf));

	data = byte_high | byte_low;
#else
	byte_low =  (*pBuf);
	byte_high = ((*pBuf)) << 4;

	data = byte_high | byte_low;
#endif

#if 0
	pr_info("*pBuf:0x%x, byte_high:0x%x, byte_low:0x%x\n", *pBuf, byte_high, byte_low);					
	pr_info("*pBuf:0x%x, data:0x%x\n", *pBuf, data);
#endif

	if (tx_buffer_1 == g_cpld_manager.current_tx_buf_num)
	{
		g_cpld_manager.current_tx_buf_num = tx_buffer_2;
		g_cpld_manager.p_cpld_reg->tx_buffer2_cs_low = data;
	}
	else if (tx_buffer_2 == g_cpld_manager.current_tx_buf_num)
	{
		g_cpld_manager.current_tx_buf_num = tx_buffer_1;
		g_cpld_manager.p_cpld_reg->tx_buffer1_cs_low = data;
	}
	else
	{
		g_cpld_manager.current_tx_buf_num = tx_buffer_1;
		g_cpld_manager.p_cpld_reg->tx_buffer1_cs_low = data;
	}
	udelay(5);

	//pr_info("===end to write addrewss for reading!===\n");
	
	pBuf++;
	count--;
    
	if (!count) {
		pr_info("failed. count==0.\n");
		return (-1);
	}

	//pr_info("===start to read!===\n");

	//udelay(10);
	
	for (i = 0; i < count; i++) {
			
		
			if (i == (count - 1)) {
				
				data = g_cpld_manager.p_cpld_reg->rx_buffer1_cs_high;
				#if 1
				byte_high = (data & 0xff00);
				byte_low = data & 0x00ff;
				#else
				byte_high = (data & 0x0c00) >> 4;
				byte_low = data & 0x003f;
				#endif
				*pBuf = (unsigned char)(byte_low | byte_high);

#if 0
				pr_info("i:%d, count-1:%d\n", i, count-1);
				pr_info("*pBuf:0x%x, byte_high:0x%x, byte_low:0x%x\n", *pBuf, byte_high, byte_low);					
				printk("*pBuf:0x%x, data:0x%x\n", *pBuf, data);
#endif
				udelay(5);
				break;
			}

			
			data = g_cpld_manager.p_cpld_reg->rx_buffer1_cs_low;
			#if 1
			byte_high = (data & 0xff00);
			byte_low = data & 0x00ff;
			#else
			byte_high = (data & 0x0c00) >> 4;
			byte_low = data & 0x003f;
			#endif

			*pBuf = (unsigned char)(byte_low | byte_high);

#if 0
			pr_info("i:%d, count-1:%d\n", i, count-1);
			pr_info("*pBuf:0x%x, byte_high:0x%x, byte_low:0x%x\n", *pBuf, byte_high, byte_low);					
			printk("*pBuf:0x%x, data:0x%x\n", *pBuf, data);
#endif
			udelay(5);
			pBuf++;
		}

		//pr_info("===end to read!===\n");
		
		return (0);
}
#else
int cpld_ebi2spi_read(unsigned char *buf, int len)
{
	/*struct cpld_driver *cpld = g_cpld;*/
	int i;

	switch_interface(INTF_SPI);

	for (i = 0; i < len; i++) {
		;	/* TODO: Read ebi2 data here */
	}

	switch_interface(INTF_UART);

	return 0;
}
#endif

EXPORT_SYMBOL(cpld_spi_read);

#if 0
static void printb(char *buff, size_t size, const char *title)
{
	int i;

	if (title)
		printk(KERN_INFO "%s:\n", title);
	for (i = 0; i < size; i++) {
		if (i % 16 == 0) {
			if (i)
				printk(KERN_INFO "\n");
			printk(KERN_INFO "  %04x:", i);
		}
		if (i % 2 == 0)
			printk(KERN_INFO " ");
		printk(KERN_INFO "%02x", buff[i]);
	}
	printk(KERN_INFO "\n");
}
#endif

#if 0
static int sprintb(char *dest, char *src, size_t size, const char *title)
{
	int i, count = 0;

	if (title)
		count += sprintf(dest, "%s:\n", title);
	for (i = 0; i < size; i++) {
		if (i % 16 == 0) {
			if (i)
				count += sprintf(dest + count, "\n");
			count += sprintf(dest + count, "  %08x:", i);
		}
		if (i % 2 == 0)
			count += sprintf(dest + count, " ");
		count += sprintf(dest + count, "%02x", src[i]);
	}
	count += sprintf(dest + count, "\n");

	return  count;
}
#endif

static int parse_arg(const char *buf, int buf_len,
			void *argv, int max_size, int arg_size)
{
	char string[buf_len + 1];
	unsigned long result = 0;
	char *str = string;
	char *p;
	int n = 0;

	if (buf_len < 0)
		return -1;

	memcpy(string, buf, buf_len);
	string[buf_len] = '\0';

	while ((p = strsep(&str, " :"))) {
		if (!strlen(p))
			continue;
		if (!strict_strtoul(p, 16, &result)) {
			if (arg_size == 1)
				((uint8_t *)argv)[n] = (uint8_t)result;
			else if (arg_size == 2)
				((uint16_t *)argv)[n] = (uint16_t)result;
			else if (arg_size == 4)
				((uint32_t *)argv)[n] = (uint32_t)result;
		} else {
			break;
		}
		if (++n == max_size / arg_size)
			break;
	}

	return n;
}

static ssize_t gpio_spi_store(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	struct cpld_driver *cpld = container_of(kobj, struct cpld_driver, kobj);
	uint8_t *argv = (uint8_t *)cpld->sysfs_buf;
	int n;

	n = parse_arg(buf + 2, count - 2, argv, CPLD_BUFSIZ, 1);

	pr_info("%s: n = %d\n", __func__, n);

	if (n > 0) {
		cpld->reg_addr = argv[0];
		if (buf[0] == 'w') {
			 gpio_spi_write(n, argv);
		} else {
			cpld->sysfs_cnt = (n == 1) ? 1 : argv[1];
		}
	}

	return count;
}

static ssize_t gpio_spi_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	struct cpld_driver *cpld = container_of(kobj, struct cpld_driver, kobj);
	ssize_t count = 0;
	int err;
	uint8_t data;

	/*err = cpld_gpio_spi_read(cpld->sysfs_buf, cpld->sysfs_cnt);*/

	err = cpld_gpiospi_single_read(cpld, cpld->reg_addr, &data);
	pr_info("%s: err = %d, rd_data = 0x%x\n", __func__, err, data);

	/*if (err == 0) {*/
		/*count += sprintb(buf, cpld->sysfs_buf, cpld->sysfs_cnt, NULL);*/
	/*}*/

	return count;
}

static ssize_t ebi2_spi_store(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	struct cpld_driver *cpld = container_of(kobj, struct cpld_driver, kobj);
	uint8_t *argv = (uint8_t *)cpld->sysfs_buf;
	int n;

	n = parse_arg(buf + 2, count - 2, argv, CPLD_BUFSIZ, 1);
	
	pr_info("0201: %s: n = %d\n", __func__, n);

	if (n > 0) {
		cpld->reg_addr = argv[0];
		if (buf[0] == 'w') {
			spi_set_route(EBI2_SPI);
			cpld_spi_write(n, argv);
		} 
		else if (buf[0] == 'r') {
			spi_set_route(EBI2_SPI);
			cpld_spi_read(argv[n-1], argv);
		} 
		else {
			cpld->sysfs_cnt = (n == 1) ? 1 : argv[1];
		}
	}

	return count;
}

static ssize_t ebi2_spi_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
#if 0
	struct cpld_driver *cpld = container_of(kobj, struct cpld_driver, kobj);
	uint8_t *argv = (uint8_t *)cpld->sysfs_buf;
	int n;
	int count;

	count = strlen(buf);

	n = parse_arg(buf + 2, count - 2, argv, CPLD_BUFSIZ, 1);

	pr_info("%s: n = %d, count:%d\n", __func__, n, count);

	if (n > 0) {
		cpld->reg_addr = argv[0];
		if (buf[0] == 'r') {
			cpld_spi_read(argv[n-1], argv);
		} 
		else {
			cpld->sysfs_cnt = (n == 1) ? 1 : argv[1];
		}
	}
#endif

	return 0;
}
	
static ssize_t cpld_clock_store(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	struct cpld_driver *cpld = container_of(kobj, struct cpld_driver, kobj);


	clock_setting(cpld, 1);
	
	pr_info("%s\n", __func__);
	
	return count;
}

static ssize_t cpld_clock_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	/*struct cpld_driver *cpld = container_of(kobj, struct cpld_driver, kobj);*/

	pr_info("%s\n", __func__);
	return 0;
}

static ssize_t switch_store(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	/*struct cpld_driver *cpld = container_of(kobj, struct cpld_driver, kobj);*/

	pr_info("%s. switch to ebi2 or uart2dm:%s.\n", __func__, &buf[0]);
	
	if (buf[0] == 'e') {
		pr_info("EBI2_SPI\n");
		spi_set_route(EBI2_SPI);
	}
	else if(buf[0] == 'u') {
		pr_info("GPIO_SPI\n");
		spi_set_route(GPIO_SPI);
	}
		
	return count;
}

static ssize_t switch_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	/*struct cpld_driver *cpld = container_of(kobj, struct cpld_driver, kobj);*/

	pr_info("%s\n", __func__);
	
	return 0;
}


static ssize_t config_store(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	struct cpld_driver *cpld = container_of(kobj, struct cpld_driver, kobj);
	uint32_t *argv = (uint32_t *)cpld->sysfs_buf;
	int n, i;

	pr_info("%s\n", __func__);

	n = parse_arg(buf + 2, count - 2, argv, CPLD_BUFSIZ, 4);

	if (n > 0) {
		cpld->cfg_offs = argv[0];
		if (buf[0] == 'w') {
			cpld->sysfs_cnt = (n == 1) ? 0 : (n - 1);
			for (i = 0; i < cpld->sysfs_cnt; i++)
				writel(argv[1 + i], cpld->cfg_base +
						cpld->cfg_offs + i * 4);
		} else {
			cpld->sysfs_cnt = (n == 1) ? 1 : argv[1];
		}
	}

	return count;
}

static ssize_t config_show(struct kobject *kobj, struct kobj_attribute *attr,
						char *buf)
{
	struct cpld_driver *cpld = container_of(kobj, struct cpld_driver, kobj);
	/*void __iomem *base = cpld->cfg_base + cpld->cfg_offs;*/
	uint32_t value;
	ssize_t count = 0;
	int i;

	pr_info("%s\n", __func__);

	if (cpld->sysfs_cnt > 0) {
		for (i = 0; i < cpld->sysfs_cnt; i++) {
			value = readl(cpld->cfg_base + cpld->cfg_offs + i * 4);
#if 0
			if (i % 4 == 0) {
				if (i)
					count += sprintf(buf + count, "\n");
				count += sprintf(buf + count, "  %08x:",
						cpld->cfg_start +
						cpld->cfg_offs + i * 4);
			}
			count += sprintf(buf + count, " %08x", value);
#endif
			count += sprintf(buf + count, "  %08x: %08x\n",
					cpld->cfg_start +
					cpld->cfg_offs + i * 4, value);
		}
		/*count += sprintf(buf + count, "\n");*/

		cpld->sysfs_cnt = 0;
	}

	return count;
}

static ssize_t gpio_store(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	struct cpld_driver *cpld = container_of(kobj, struct cpld_driver, kobj);
	uint32_t *argv = (uint32_t *)cpld->sysfs_buf;
	int n, i;

	pr_info("%s\n", __func__);

	n = parse_arg(buf + 2, count - 2, argv, CPLD_BUFSIZ, 4);

	if (n > 0) {
		cpld->gpio_offs = argv[0];
		if (buf[0] == 'w') {
			cpld->sysfs_cnt = (n == 1) ? 0 : (n - 1);
			for (i = 0; i < cpld->sysfs_cnt; i++)
				writel(argv[1 + i], cpld->gpio_base +
						cpld->gpio_offs + i * 4);
		} else {
			cpld->sysfs_cnt = (n == 1) ? 1 : argv[1];
		}
	}

	return count;
}

static ssize_t clock_store(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	struct cpld_driver *cpld = container_of(kobj, struct cpld_driver, kobj);
	uint32_t *argv = (uint32_t *)cpld->sysfs_buf;
	int n, i;

	pr_info("%s\n", __func__);

	n = parse_arg(buf + 2, count - 2, argv, CPLD_BUFSIZ, 4);

	if (n > 0) {
		cpld->clk_offs = argv[0];
		if (buf[0] == 'w') {
			cpld->sysfs_cnt = (n == 1) ? 0 : (n - 1);
			for (i = 0; i < cpld->sysfs_cnt; i++)
				writel(argv[1 + i], cpld->clk_base +
						cpld->clk_offs + i * 4);
		} else {
			cpld->sysfs_cnt = (n == 1) ? 1 : argv[1];
		}
	}

	return count;
}

static ssize_t clock_show(struct kobject *kobj, struct kobj_attribute *attr,
						char *buf)
{
	struct cpld_driver *cpld = container_of(kobj, struct cpld_driver, kobj);
	void __iomem *base = cpld->clk_base + cpld->clk_offs;
	uint32_t value;
	ssize_t count = 0;
	int i;

	pr_info("%s\n", __func__);

	if (cpld->sysfs_cnt > 0) {
		for (i = 0; i < cpld->sysfs_cnt; i++) {
			value = readl(base + i * 4);

			if (i % 8 == 0) {
				if (i)
					count += sprintf(buf + count, "\n");
				count += sprintf(buf + count, "  %08x:",
						(unsigned int)base + i * 4);
			}
			count += sprintf(buf + count, " %08x", value);
		}
		count += sprintf(buf + count, "\n");

		cpld->sysfs_cnt = 0;
	}

	return count;
}

static ssize_t
gpio_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct cpld_driver *cpld = container_of(kobj, struct cpld_driver, kobj);
	void __iomem *base = cpld->gpio_base + cpld->gpio_offs;
	uint32_t value;
	ssize_t count = 0;
	int i;

	pr_info("%s\n", __func__);

	if (cpld->sysfs_cnt > 0) {
		for (i = 0; i < cpld->sysfs_cnt; i++) {
			value = readl(base + i * 4);

			if (i % 8 == 0) {
				if (i)
					count += sprintf(buf + count, "\n");
				count += sprintf(buf + count, "  %08x:",
						(unsigned int)base + i * 4);
			}
			count += sprintf(buf + count, " %08x", value);
		}
		count += sprintf(buf + count, "\n");

		cpld->sysfs_cnt = 0;
	}

	return count;
}

static ssize_t register_store(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t count)
{
#if 0
	struct cpld_driver *cpld = container_of(kobj, struct cpld_driver, kobj);
	uint8_t *argv = (uint8_t *)cpld->sysfs_buf;
	int n, i;


	pr_info("0113: %s.\n", __func__);

	n = parse_arg(buf + 2, count - 2, argv, CPLD_BUFSIZ, 1);

	if (n > 0) {
		cpld->reg_addr = argv[0];
		if (buf[0] == 'w') {
			cpld->sysfs_cnt = (n == 1) ? 0 : (n - 1);
			for (i = 0; i < cpld->sysfs_cnt; i++)
				writeb(argv[1 + i],
				       cpld->reg_base + cpld->reg_addr + i);
		} else {
			cpld->sysfs_cnt = (n == 1) ? 1 : argv[1];
		}
	}

	return count;
#else
	struct cpld_driver *cpld = container_of(kobj, struct cpld_driver, kobj);
	uint32_t *argv = (uint32_t *)cpld->sysfs_buf;
	int n, i;

	pr_info("%s\n", __func__);

	n = parse_arg(buf + 2, count - 2, argv, CPLD_BUFSIZ, 4);

	if (n > 0) {
		cpld->reg_addr = argv[0];
		
		if (buf[0] == 'w') {

			spi_set_route(EBI2_SPI);
			
			cpld->sysfs_cnt = (n == 1) ? 0 : (n - 1);
			for (i = 0; i < cpld->sysfs_cnt; i++){
				
				writel(argv[1 + i], cpld->reg_base +
						cpld->reg_addr + i * 4);
			}
		} else {
			cpld->sysfs_cnt = (n == 1) ? 1 : argv[1];
		}
	}

	return count;
#endif
}

static ssize_t
register_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
#if 0
	struct cpld_driver *cpld = container_of(kobj, struct cpld_driver, kobj);
	uint8_t *rd_buf = (uint8_t *)cpld->sysfs_buf;
	ssize_t count = 0;
	int i;
	
	pr_info("0113: %s.\n", __func__);

	if (cpld->sysfs_cnt > 0) {
		memset(rd_buf, 0, cpld->sysfs_cnt);
		for (i = 0; i < cpld->sysfs_cnt; i++)
			rd_buf[i] = readb(cpld->reg_base + cpld->reg_addr + i);
		count += sprintb(buf, rd_buf, cpld->sysfs_cnt, NULL);
	}
	cpld->sysfs_cnt = 0;

	return count;
#else
	struct cpld_driver *cpld = container_of(kobj, struct cpld_driver, kobj);
	uint32_t value;
	ssize_t count = 0;
	int i;

	pr_info("0113-1:%s\n", __func__);

	spi_set_route(EBI2_SPI);
	
	if (cpld->sysfs_cnt > 0) {
		for (i = 0; i < cpld->sysfs_cnt; i++) {
			
			value = readl(cpld->reg_base + cpld->reg_addr + i * 4);

#if 0
			if (i % 4 == 0) {
				if (i)
					count += sprintf(buf + count, "\n");
				count += sprintf(buf + count, "  %08x:",
						cpld->cfg_start +
						cpld->cfg_offs + i * 4);
			}
			count += sprintf(buf + count, " %08x", value);
#endif
			count += sprintf(buf + count, "%08x\n", value);
		}
		/*count += sprintf(buf + count, "\n");*/

		cpld->sysfs_cnt = 0;
	}

	return count;
#endif
}

static struct kobj_attribute cpld_attrs[] = {
	__ATTR(gpio_spi, (S_IWUSR | S_IRUGO), gpio_spi_show, gpio_spi_store),
	__ATTR(ebi2_spi, (S_IWUSR | S_IRUGO), ebi2_spi_show, ebi2_spi_store),
	__ATTR(config, (S_IWUSR | S_IRUGO), config_show, config_store),
	__ATTR(gpio, (S_IWUSR | S_IRUGO), gpio_show, gpio_store),
	__ATTR(register, (S_IWUSR | S_IRUGO), register_show, register_store),
	__ATTR(clock, (S_IWUSR | S_IRUGO), clock_show, clock_store),
	__ATTR(cpld_clock, (S_IWUSR | S_IRUGO), cpld_clock_show,cpld_clock_store),
	__ATTR(switch, (S_IWUSR | S_IRUGO), switch_show,switch_store),
	__ATTR_NULL
};

static struct kobj_type cpld_ktype = {
	.sysfs_ops = &kobj_sysfs_ops,
};

static int cpld_sysfs_create(struct cpld_driver *cpld)
{
	struct kobj_attribute *p;
	int err;

	err = kobject_init_and_add(&cpld->kobj, &cpld_ktype, NULL, "cpld");
	if (err == 0) {
		for (p = cpld_attrs; p->attr.name; p++)
			err = sysfs_create_file(&cpld->kobj, &p->attr);
	}

	return err;
}

static void cpld_sysfs_remove(struct cpld_driver *cpld)
{
	struct kobj_attribute *p;

	if (cpld->kobj.state_in_sysfs) {
		for (p = cpld_attrs; p->attr.name; p++)
			sysfs_remove_file(&cpld->kobj, &p->attr);
		kobject_del(&cpld->kobj);
	}
}

static int __devinit cpld_gpiospi_probe(struct spi_device *spi)
{
	struct cpld_driver *cpld = g_cpld;
	int err;

	pr_info("%s entry\n", __func__);

	spi->bits_per_word = 8;
	err = spi_setup(spi);
	if (err) {
		pr_error("spi_setup error\n");
		return err;
	}

	cpld->spi = spi;
	cpld->xfer.bits_per_word = 0;
	dev_set_drvdata(&spi->dev, cpld);

	/*spi_message_init(&cpld->msg);*/

	return 0;
}

static int __devexit cpld_gpiospi_remove(struct spi_device *spi)
{
	/*struct tdo24m *lcd = dev_get_drvdata(&spi->dev);*/

	pr_info("%s\n", __func__);

	return 0;
}

static struct spi_driver cpld_gpiospi_driver = {
	.probe	= cpld_gpiospi_probe,
	.remove	= __devexit_p(cpld_gpiospi_remove),
	.driver	= {
		.name	= "gpio-cpld",
		.owner	= THIS_MODULE,
	},
};

static int cpld_gpio_init(struct cpld_driver *cpld)
{
	unsigned int value;
	//int err;
#if 1
	pr_info("%s\n", __func__);
	//if (cpld->pdata->init_cpld_clk){
	//	cpld->pdata->init_cpld_clk(1);
	//}
#else
	if (cpld->pdata->init_gpio){
		cpld->pdata->init_gpio();
	}
#endif

	value = readl(cpld->clk_base + 0xb8);
	pr_info("0131-1: cpld_gpio_init. SDC4_MD: 0x%08x\n", value);

	value = readl(cpld->clk_base + 0xbc);
	pr_info("cpld_gpio_init. SDC4_NS: 0x%08x\n", value);

	writel(0, cpld->gpio_base + TMUX_EBI2_OFFSET);

	value = readl(cpld->cfg_base + EBI2_XMEM_CS0_CFG1_OFFSET);
	value |= (1 << 31);
	writel(value, cpld->cfg_base + EBI2_XMEM_CS0_CFG1_OFFSET);

	writel(0x02, cpld->cfg_base);

	writel(0x031F3200, cpld->cfg_base + 0x10008);
#if 0
	if(cpld->pdata->cpld_power_pwd){
		err = gpio_request(cpld->pdata->cpld_power_pwd, "cpld_power");
		if (err) {
			pr_error("failed to request gpio cpld_power\n");
			return -EINVAL;
		}
	}

	if(cpld->pdata->intf_select){
		err = gpio_request(cpld->pdata->intf_select, "interface_select");
		if (err) {
			pr_error("failed to request gpio interface_select\n");
			gpio_free(cpld->pdata->cpld_power_pwd);
			return -EINVAL;
		}
	}


	if(cpld->pdata->cpld_power_pwd)
		gpio_direction_output(cpld->pdata->cpld_power_pwd, 1);	/* Power */
	if(cpld->pdata->intf_select)
		gpio_direction_output(cpld->pdata->intf_select, 1);
#endif

	g_cpld_manager.p_cpld_reg = cpld->reg_base;
	g_cpld_manager.current_tx_buf_num = tx_buffer_1;
	
	return 0;
}

int cpld_open_init(void)
{
	pr_info("%s\n", __func__);
	if (g_cpld->pdata->init_cpld_clk){
		pr_info("%s: Enable SDMC4 CLK\n", __func__);
		g_cpld->pdata->init_cpld_clk(1);
	}

	clock_setting(g_cpld, 1);

	cpld_gpio_init(g_cpld);

	if (g_cpld->pdata->power_func){
		pr_info("%s: Enable CPLD power\n", __func__);
		g_cpld->pdata->power_func(1);
	}

	return 0;
}

int cpld_release(void)
{
	pr_info("%s\n", __func__);
	if (g_cpld->pdata->init_cpld_clk){
		pr_info("%s: Disable SDMC4 CLK\n", __func__);
		g_cpld->pdata->init_cpld_clk(0);
	}

	clock_setting(g_cpld, 0);
	
	if (g_cpld->pdata->power_func){
		pr_info("%s: Disable CPLD power\n", __func__);
		g_cpld->pdata->power_func(0);
	}

	return 0;
}


static int cpld_probe(struct platform_device *pdev)
{
	struct cpld_platform_data *pdata = pdev->dev.platform_data;
	struct cpld_driver *cpld;
	struct resource *cfg_mem, *reg_mem, *gpio_mem, *clk_mem, *sdc4_mem;
	int err;
	int cfg_value;

	pr_info("%s\n", __func__);

	cfg_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!cfg_mem) {
		pr_error("no config mem resource!\n");
		return -ENODEV;
	}

	reg_mem = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!reg_mem) {
		pr_error("no reg mem resource!\n");
		return -ENODEV;
	}

	gpio_mem = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (!gpio_mem) {
		pr_error("no gpio mem resource!\n");
		return -ENODEV;
	}

	clk_mem = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	if (!clk_mem) {
		pr_error("no clock mem resource!\n");
		return -ENODEV;
	}

	sdc4_mem = platform_get_resource(pdev, IORESOURCE_MEM, 4);
	if (!clk_mem) {
		pr_error("no sdc4 mem resource!\n");
		return -ENODEV;
	}

	cpld = kzalloc(sizeof(struct cpld_driver), GFP_KERNEL);
	if (!cpld){
		pr_error("create cpld memory fail\n");
		return -ENOMEM;
	}

	g_cpld = cpld;
	cpld->pdata = pdata;

	cpld->cfg_base = ioremap(cfg_mem->start, resource_size(cfg_mem));
	if (!cpld->cfg_base) {
		pr_error("no config mem resource\n");
		err = -ENOMEM;
		goto err_config_ioremap;
	}

	cpld->reg_base = ioremap(reg_mem->start, resource_size(reg_mem));
	if (!cpld->reg_base) {
		pr_error("no reg mem resource\n");
		err = -ENOMEM;
		goto err_register_ioremap;
	}

	cpld->gpio_base = ioremap(gpio_mem->start, resource_size(gpio_mem));
	if (!cpld->gpio_base) {
		pr_error("no gpio mem resource\n");
		err = -ENOMEM;
		goto err_gpio_ioremap;
	}

	cpld->clk_base = ioremap(clk_mem->start, resource_size(clk_mem));
	if (!cpld->clk_base) {
		pr_error("no clock mem resource\n");
		err = -ENOMEM;
		goto err_clk_ioremap;
	}

	cpld->sdc4_base = ioremap(sdc4_mem->start, resource_size(sdc4_mem));
	if (!cpld->sdc4_base) {
		pr_error("no sdc4 mem resource\n");
		err = -ENOMEM;
		goto err_sdc4_ioremap;
	}

	cfg_value = readl(cpld->cfg_base);
	pr_info("cfg_value = %08x\n", cfg_value);
	cfg_value = readl(cpld->cfg_base + 0x0008);
	pr_info("cfg_value = %08x\n", cfg_value);

	err = spi_register_driver(&cpld_gpiospi_driver);
	if (err)
		goto err_1;

#if 0
	err = platform_driver_probe(ebi2_driver, ebi2_probe);
	if (err)
		goto err_2;
#endif
	/* Enable SDMC4 CLK */
	if (cpld->pdata->init_cpld_clk){
		pr_info("%s: Enable SDMC4 CLK\n", __func__);
		cpld->pdata->init_cpld_clk(1);
	}
	/* Enable CPLD power */
	if (pdata->power_func){
		pr_info("%s: Enable CPLD power\n", __func__);
		pdata->power_func(1);
	}

	clock_setting(cpld, 1);
	
	cpld_gpio_init(cpld);

	cpld_sysfs_create(cpld);

	pr_info("CPLD ready\n");

	cpld_release();
	return 0;

#if 0
err_2:
	spi_unregister_driver(&cpld_spi_driver);
#endif
err_1:
err_sdc4_ioremap:
	iounmap(cpld->clk_base);
err_clk_ioremap:
	iounmap(cpld->gpio_base);
err_gpio_ioremap:
	iounmap(cpld->reg_base);
err_register_ioremap:
	iounmap(cpld->cfg_base);
err_config_ioremap:
	kfree(cpld);

	return err;
}

static int __exit cpld_remove(struct platform_device *pdev)
{
	struct cpld_driver *cpld = g_cpld;
	struct cpld_platform_data *pdata = pdev->dev.platform_data;
	pr_info("%s\n", __func__);

	spi_unregister_driver(&cpld_gpiospi_driver);
	cpld_sysfs_remove(cpld);
	iounmap(cpld->clk_base);
	iounmap(cpld->gpio_base);
	iounmap(cpld->reg_base);
	iounmap(cpld->cfg_base);
	iounmap(cpld->sdc4_base);
	kfree(cpld);

	if (pdata->init_cpld_clk){
		pr_info("%s: Disable SDMC4 CLK\n", __func__);
		pdata->init_cpld_clk(0);
	}

	clock_setting(cpld, 0);

	if (pdata->power_func){
		pr_info("%s: Disable CPLD power\n", __func__);
		pdata->power_func(0);
	}

	return 0;
}

static struct platform_driver cpld_platform_driver = {
	.driver.name	= "cpld",
	.driver.owner	= THIS_MODULE,
	.probe		= cpld_probe,
	.remove		= __exit_p(cpld_remove),
};

static int __init cpld_init(void)
{
	pr_info("%s\n",__func__);
	return platform_driver_register(&cpld_platform_driver);
}
module_init(cpld_init);

static void __exit cpld_exit(void)
{
	pr_info("%s\n",__func__);

	platform_driver_unregister(&cpld_platform_driver);
}
module_exit(cpld_exit);

MODULE_DESCRIPTION("Driver for CPLD");
MODULE_LICENSE("GPL");

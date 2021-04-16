/*
 *  Driver for CPS-COM-xxx serial ports
 *
 *  Based on drivers/tty/serial/8250.c, by Russell King.
 *
 *  Copyright (C) 2015 syunsuke okamoto <okamoto@contec.jp>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * A note about mapbase / membase
 *
 *  mapbase is the physical address of the IO port.
 *  membase is an 'ioremapped' cookie.
 */

#if defined(CONFIG_SERIAL_8250_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
#define SUPPORT_SYSRQ
#endif

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/sysrq.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/tty.h>
#include <linux/ratelimit.h>
#include <linux/tty_flip.h>
#include <linux/serial_reg.h>
#include <linux/serial_core.h>
#include <linux/serial.h>
#include <linux/serial_8250.h>
#include <linux/nmi.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include <asm/io.h>
#include <asm/irq.h>

#include <linux/uaccess.h> 

#include "8250.h"

#include "serial_cpscom.h"

#ifdef CONFIG_SPARC
#include "suncore.h"
#endif

#define DRV_VERSION	"1.0.9"

/*
 * Configuration:
 *   share_irqs - whether we pass IRQF_SHARED to request_irq().  This option
 *                is unsafe when used on edge-triggered interrupts.
 */
static unsigned int share_irqs = CPSCOM_SHARE_IRQS;

static unsigned int nr_uarts = CPSCOM_RUNTIME_UARTS;

static struct uart_driver cpscom_reg;

static int serial_index(struct uart_port *port)
{
	return (cpscom_reg.minor) + port->line;
}

static unsigned int skip_txen_test; /* force skip of txen test at init time */

 static int contec_mcs341_create_8250_device_sysfs(struct device *);
 static void contec_mcs341_remove_8250_device_sysfs(struct device *);
 
 static int subunit_interrupt[CPS_DEVICE_MAX_NUM] = {0};
 static int subunit_power_state[CPS_DEVICE_MAX_NUM] = {0};
 static int subunit_powerreset[CPS_DEVICE_MAX_NUM] = {0};
 //2018.05.11	
 //  static int lora_deviceID = 0;
 static u16 subunit_leds_status[CPS_DEVICE_MAX_NUM] = {0};

/*
 * Debugging.
 */
#if 0
#define DEBUG_AUTOCONF(fmt...)	printk(fmt)
#else
#define DEBUG_AUTOCONF(fmt...)	do { } while (0)
#endif

#if 0
#define DEBUG_INTR(fmt...)	printk(fmt)
#else
#define DEBUG_INTR(fmt...)	do { } while (0)
#endif

//#define DEBUG_SERIAL_REGISTER
//#define DEBUG_SERIAL_CEMARKING_TEST		// CE Marking Test Version

#ifdef DEBUG_SERIAL_REGISTER
#define DEBUG_IOPORT(fmt...)	printk(fmt)
#else
#define DEBUG_IOPORT(fmt...)	do { } while (0)
#endif

#if 0
#define DEBUG_LOCALADDR_EF(fmt...)	printk(fmt)
#else
#define DEBUG_LOCALADDR_EF(fmt...)	do { } while (0)
#endif

#if 0
#define DEBUG_INITCOMPORT(fmt...)	printk(fmt)
#else
#define DEBUG_INITCOMPORT(fmt...)	do { } while (0)
#endif 

#define PASS_LIMIT	512

#define BOTH_EMPTY 	(UART_LSR_TEMT | UART_LSR_THRE)


/*
 * We default to IRQ0 for the "no irq" hack.   Some
 * machine types want others as well - they're free
 * to redefine this in their header file.
 */
#define is_real_interrupt(irq)	((irq) != 0)

#ifdef CONFIG_SERIAL_8250_DETECT_IRQ
#define CONFIG_SERIAL_DETECT_IRQ 1
#endif
#ifdef CONFIG_SERIAL_8250_MANY_PORTS
#define CONFIG_SERIAL_MANY_PORTS 1
#endif

/*
 * HUB6 is always on.  This will be removed once the header
 * files have been cleaned.
 */
#define CONFIG_HUB6 1

#include <asm/serial.h>
/*
 * SERIAL_PORT_DFNS tells us about built-in ports that have no
 * standard enumeration mechanism.   Platforms that can find all
 * serial ports via mechanisms like ACPI or PCI need not supply it.
 */
#ifndef SERIAL_PORT_DFNS
#define SERIAL_PORT_DFNS
#endif

static const struct old_serial_port old_serial_port[] = {
	SERIAL_PORT_DFNS /* defined in asm/serial.h */
};

#define UART_NR	CPSCOM_NR_UARTS

#ifdef CONFIG_SERIAL_8250_RSA

#define PORT_RSA_MAX 4
static unsigned long probe_rsa[PORT_RSA_MAX];
static unsigned int probe_rsa_count;
#endif /* CONFIG_SERIAL_8250_RSA  */

#define UART_FCTR_RS485 ( 0x08 )

struct uart_8250_port {
	struct uart_port	port;
	struct timer_list	timer;		/* "no irq" timer */
	struct list_head	list;		/* ports on this IRQ */
	unsigned short		capabilities;	/* port capabilities */
	unsigned short		bugs;		/* port bugs */
	unsigned int		tx_loadsz;	/* transmit fifo load size */
	unsigned char		acr;
	unsigned char		ier;
	unsigned char		lcr;
	unsigned char		mcr;
	unsigned char		mcr_mask;	/* mask of user bits */
	unsigned char		mcr_force;	/* mask of forced bits */
	unsigned char		cur_iotype;	/* Running I/O type */

	/*
	 * Some bits in registers are cleared on a read, so they must
	 * be saved whenever the register is read but the bits will not
	 * be immediately processed.
	 */
#define LSR_SAVE_FLAGS UART_LSR_BRK_ERROR_BITS
	unsigned char		lsr_saved_flags;
#define MSR_SAVE_FLAGS UART_MSR_ANY_DELTA
	unsigned char		msr_saved_flags;

	struct serial_rs485     rs485_config;//
	int						rs485_flag;
};

struct irq_info {
	struct			hlist_node node;
	int			irq;
	spinlock_t		lock;	/* Protects list not the hash */
	struct list_head	*head;
};

#define NR_IRQ_HASH		32	/* Can be adjusted later */
static struct hlist_head irq_lists[NR_IRQ_HASH];
static DEFINE_MUTEX(hash_mutex);	/* Used to walk the hash */

/*
 * Here we define the default xmit fifo size used for each type of UART.
 */
static const struct serial8250_config uart_config[] = {
	[PORT_UNKNOWN] = {
		.name		= "unknown",
		.fifo_size	= 1,
		.tx_loadsz	= 1,
	},
	[PORT_8250] = {
		.name		= "8250",
		.fifo_size	= 1,
		.tx_loadsz	= 1,
	},
	[PORT_16450] = {
		.name		= "16450",
		.fifo_size	= 1,
		.tx_loadsz	= 1,
	},
	[PORT_16550] = {
		.name		= "16550",
		.fifo_size	= 1,
		.tx_loadsz	= 1,
	},
	[PORT_16550A] = {
		.name		= "16550A",
		.fifo_size	= 16,
		.tx_loadsz	= 16,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_10,
	},
	[PORT_CPS16550] = { // old fpga
		.name		="CPS16550",
		.fifo_size	= 128,
		.tx_loadsz	= 128,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_00,
		.flags		= UART_CAP_FIFO,
	},

	[PORT_CPS16550A] = { // new fpga
		.name		="CPS16550A",
		.fifo_size	= 128,
		.tx_loadsz	= 128,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_00,
		.flags		= UART_CAP_FIFO,
	},

	[PORT_CIRRUS] = {
		.name		= "Cirrus",
		.fifo_size	= 1,
		.tx_loadsz	= 1,
	},
	[PORT_16650] = {
		.name		= "ST16650",
		.fifo_size	= 1,
		.tx_loadsz	= 1,
		.flags		= UART_CAP_FIFO | UART_CAP_EFR | UART_CAP_SLEEP,
	},
	[PORT_16650V2] = {
		.name		= "ST16650V2",
		.fifo_size	= 32,
		.tx_loadsz	= 16,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_01 |
				  UART_FCR_T_TRIG_00,
		.flags		= UART_CAP_FIFO | UART_CAP_EFR | UART_CAP_SLEEP,
	},
	[PORT_16750] = {
		.name		= "TI16750",
		.fifo_size	= 64,
		.tx_loadsz	= 64,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_10 |
				  UART_FCR7_64BYTE,
		.flags		= UART_CAP_FIFO | UART_CAP_SLEEP | UART_CAP_AFE,
	},
	[PORT_STARTECH] = {
		.name		= "Startech",
		.fifo_size	= 1,
		.tx_loadsz	= 1,
	},
	[PORT_16C950] = {
		.name		= "16C950/954",
		.fifo_size	= 128,
		.tx_loadsz	= 128,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_10,
		/* UART_CAP_EFR breaks billionon CF bluetooth card. */
		.flags		= UART_CAP_FIFO | UART_CAP_SLEEP,
	},
	[PORT_16654] = {
		.name		= "ST16654",
		.fifo_size	= 64,
		.tx_loadsz	= 32,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_01 |
				  UART_FCR_T_TRIG_10,
		.flags		= UART_CAP_FIFO | UART_CAP_EFR | UART_CAP_SLEEP,
	},
	[PORT_16850] = {
		.name		= "XR16850",
		.fifo_size	= 128,
		.tx_loadsz	= 128,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_10,
		.flags		= UART_CAP_FIFO | UART_CAP_EFR | UART_CAP_SLEEP,
	},
	[PORT_RSA] = {
		.name		= "RSA",
		.fifo_size	= 2048,
		.tx_loadsz	= 2048,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_11,
		.flags		= UART_CAP_FIFO,
	},
	[PORT_NS16550A] = {
		.name		= "NS16550A",
		.fifo_size	= 16,
		.tx_loadsz	= 16,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_10,
		.flags		= UART_CAP_FIFO | UART_NATSEMI,
	},
	[PORT_XSCALE] = {
		.name		= "XScale",
		.fifo_size	= 32,
		.tx_loadsz	= 32,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_10,
		.flags		= UART_CAP_FIFO | UART_CAP_UUE | UART_CAP_RTOIE,
	},
	[PORT_RM9000] = {
		.name		= "RM9000",
		.fifo_size	= 16,
		.tx_loadsz	= 16,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_10,
		.flags		= UART_CAP_FIFO,
	},
	[PORT_OCTEON] = {
		.name		= "OCTEON",
		.fifo_size	= 64,
		.tx_loadsz	= 64,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_10,
		.flags		= UART_CAP_FIFO,
	},
	[PORT_AR7] = {
		.name		= "AR7",
		.fifo_size	= 16,
		.tx_loadsz	= 16,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_00,
		.flags		= UART_CAP_FIFO | UART_CAP_AFE,
	},
	[PORT_U6_16550A] = {
		.name		= "U6_16550A",
		.fifo_size	= 64,
		.tx_loadsz	= 64,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_10,
		.flags		= UART_CAP_FIFO | UART_CAP_AFE,
	},
	[PORT_TEGRA] = {
		.name		= "Tegra",
		.fifo_size	= 32,
		.tx_loadsz	= 8,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_01 |
				  UART_FCR_T_TRIG_01,
		.flags		= UART_CAP_FIFO | UART_CAP_RTOIE,
	},
	[PORT_XR17D15X] = {
		.name		= "XR17D15X",
		.fifo_size	= 64,
		.tx_loadsz	= 64,
		.fcr		= UART_FCR_ENABLE_FIFO | UART_FCR_R_TRIG_10,
		.flags		= UART_CAP_FIFO | UART_CAP_AFE | UART_CAP_EFR,
	},
};

#if defined(CONFIG_MIPS_ALCHEMY)

/* Au1x00 UART hardware has a weird register layout */
static const u8 au_io_in_map[] = {
	[UART_RX]  = 0,
	[UART_IER] = 2,
	[UART_IIR] = 3,
	[UART_LCR] = 5,
	[UART_MCR] = 6,
	[UART_LSR] = 7,
	[UART_MSR] = 8,
};

static const u8 au_io_out_map[] = {
	[UART_TX]  = 1,
	[UART_IER] = 2,
	[UART_FCR] = 4,
	[UART_LCR] = 5,
	[UART_MCR] = 6,
};

/* sane hardware needs no mapping */
static inline int map_8250_in_reg(struct uart_port *p, int offset)
{
	if (p->iotype != UPIO_AU)
		return offset;
	return au_io_in_map[offset];
}

static inline int map_8250_out_reg(struct uart_port *p, int offset)
{
	if (p->iotype != UPIO_AU)
		return offset;
	return au_io_out_map[offset];
}

#elif defined(CONFIG_SERIAL_8250_RM9K)

static const u8
	regmap_in[8] = {
		[UART_RX]	= 0x00,
		[UART_IER]	= 0x0c,
		[UART_IIR]	= 0x14,
		[UART_LCR]	= 0x1c,
		[UART_MCR]	= 0x20,
		[UART_LSR]	= 0x24,
		[UART_MSR]	= 0x28,
		[UART_SCR]	= 0x2c
	},
	regmap_out[8] = {
		[UART_TX] 	= 0x04,
		[UART_IER]	= 0x0c,
		[UART_FCR]	= 0x18,
		[UART_LCR]	= 0x1c,
		[UART_MCR]	= 0x20,
		[UART_LSR]	= 0x24,
		[UART_MSR]	= 0x28,
		[UART_SCR]	= 0x2c
	};

static inline int map_8250_in_reg(struct uart_port *p, int offset)
{
	if (p->iotype != UPIO_RM9000)
		return offset;
	return regmap_in[offset];
}

static inline int map_8250_out_reg(struct uart_port *p, int offset)
{
	if (p->iotype != UPIO_RM9000)
		return offset;
	return regmap_out[offset];
}

#else

/* sane hardware needs no mapping */
#define map_8250_in_reg(up, offset) (offset)
#define map_8250_out_reg(up, offset) (offset)

#endif

static unsigned int hub6_serial_in(struct uart_port *p, int offset)
{
	offset = map_8250_in_reg(p, offset) << p->regshift;
	outb(p->hub6 - 1 + offset, p->iobase);
	return inb(p->iobase + 1);
}

static void hub6_serial_out(struct uart_port *p, int offset, int value)
{
	offset = map_8250_out_reg(p, offset) << p->regshift;
	outb(p->hub6 - 1 + offset, p->iobase);
	outb(value, p->iobase + 1);
}

static unsigned int mem_serial_in(struct uart_port *p, int offset)
{
	offset = map_8250_in_reg(p, offset) << p->regshift;
	return readb(p->membase + offset);
}

static void mem_serial_out(struct uart_port *p, int offset, int value)
{
	offset = map_8250_out_reg(p, offset) << p->regshift;
	writeb(value, p->membase + offset);
}

static void mem32_serial_out(struct uart_port *p, int offset, int value)
{
	offset = map_8250_out_reg(p, offset) << p->regshift;
	writel(value, p->membase + offset);
}

static unsigned int mem32_serial_in(struct uart_port *p, int offset)
{
	offset = map_8250_in_reg(p, offset) << p->regshift;
	return readl(p->membase + offset);
}

static unsigned int au_serial_in(struct uart_port *p, int offset)
{
	offset = map_8250_in_reg(p, offset) << p->regshift;
	return __raw_readl(p->membase + offset);
}

static void au_serial_out(struct uart_port *p, int offset, int value)
{
	offset = map_8250_out_reg(p, offset) << p->regshift;
	__raw_writel(value, p->membase + offset);
}

static unsigned int io_serial_in(struct uart_port *p, int offset)
{
	offset = map_8250_in_reg(p, offset) << p->regshift;
	return inb(p->iobase + offset);
}

static void io_serial_out(struct uart_port *p, int offset, int value)
{
	offset = map_8250_out_reg(p, offset) << p->regshift;
	outb(value, p->iobase + offset);
}

static unsigned int cpscom_serial_in(struct uart_port *p, int offset)
{
	unsigned int val;
	unsigned char valb = 0;
	int dev;
	int ch;
#ifdef DEBUG_SERIAL_CEMARKING_TEST
	unsigned int valN[3];
	int addr;
	int cnt,cntA,cntB;
//	int dev;
#endif


#ifdef DEBUG_SERIAL_REGISTER
	unsigned char reg[][4] = {
		{"RHR"},{"IER"},{"ISR"},
		{"LCR"},{"MCR"},{"LSR"},
		{"MSR"},{"SPR"}
	};
#endif
	offset = map_8250_in_reg(p, offset) << p->regshift;

//	if( (unsigned long)( p->membase + offset ) & 0x01 ) {
//		cps_fpga_access(CPS_FPGA_ACCESS_BYTE_HIGH); // cps_common_io.h
//	}else{
//		cps_fpga_access(CPS_FPGA_ACCESS_BYTE_LOW); // cps_common_io.h
//	}
#ifdef DEBUG_SERIAL_CEMARKING_TEST
	contec_mcs341_inpb( (unsigned long)(p->membase + offset), &valb );
	valN[0] = valb;
	//valN[0] = readb( p->membase + offset );
#else
	contec_mcs341_inpb( (unsigned long)(p->membase + offset), &valb );
	val = valb;
	//val = readb(p->membase + offset);
#endif

#ifdef DEBUG_SERIAL_CEMARKING_TEST

	dev	= contec_mcs341_device_deviceNum_get( (unsigned long) p->mapbase );
	addr	= contec_mcs341_device_serial_channel_get( (unsigned long) p->mapbase );
	for( cnt = 1; cnt  < 3; cnt ++ ){

		valN[cnt]	= contec_mcs341_device_mirror_get(dev, addr);
	}

	if( valN[0] == valN[1]  ) val = valN[0];
	else val = valN[1];
#endif

#ifdef DEBUG_SERIAL_REGISTER
	dev = contec_mcs341_device_deviceNum_get( (unsigned long) p->mapbase );
	ch = contec_mcs341_device_serial_channel_get( (unsigned long) p->mapbase );

	DEBUG_IOPORT(KERN_INFO "[Read]<dev:%d Ch:%d> %s : %x(%c)\n", dev, ch, reg[offset], val, val);
#endif

//	cps_fpga_access(CPS_FPGA_ACCESS_WORD); // cps_common_io.h
	
	return val;

}

static void cpscom_serial_out(struct uart_port *p, int offset, int value)
{

	int dev, ch;

#ifdef DEBUG_SERIAL_REGISTER
	unsigned char reg[][4]={
		{"THR"},{"IER"},{"FCR"},
		{"LCR"},{"MCR"},{"---"},
		{"---"},{"SPR"}
	};
#endif
	offset = map_8250_out_reg(p, offset) << p->regshift;

//	if( (unsigned long)( p->membase + offset ) & 0x01 ) {
//		cps_fpga_access(CPS_FPGA_ACCESS_BYTE_HIGH); // cps_common_io.h
//	}else{
//		cps_fpga_access(CPS_FPGA_ACCESS_BYTE_LOW); // cps_common_io.h
//	}

//	writeb(value, p->membase + offset);
	contec_mcs341_outb( (unsigned long)(p->membase + offset), (unsigned char)value );
	dev = contec_mcs341_device_deviceNum_get( (unsigned long) p->mapbase );
	ch = contec_mcs341_device_serial_channel_get( (unsigned long) p->mapbase );

#ifdef DEBUG_SERIAL_REGISTER
	DEBUG_IOPORT(KERN_INFO "[Write]<Dev:%d Ch:%d> %s : %x\n", dev, ch, reg[offset], value );
#endif
//	cps_fpga_access(CPS_FPGA_ACCESS_WORD); // cps_common_io.h

}

static int cpscom_default_handle_irq(struct uart_port *port);

static void set_io_from_upio(struct uart_port *p)
{
	struct uart_8250_port *up =
		container_of(p, struct uart_8250_port, port);
	switch (p->iotype) {
	case UPIO_HUB6:
		p->serial_in = hub6_serial_in;
		p->serial_out = hub6_serial_out;
		break;

	case UPIO_MEM:
		p->serial_in = mem_serial_in;
		p->serial_out = mem_serial_out;
		break;
	case UPIO_CPS:
		p->serial_in = cpscom_serial_in;
		p->serial_out = cpscom_serial_out;
		break;
	case UPIO_RM9000:
	case UPIO_MEM32:
		p->serial_in = mem32_serial_in;
		p->serial_out = mem32_serial_out;
		break;

	case UPIO_AU:
		p->serial_in = au_serial_in;
		p->serial_out = au_serial_out;
		break;

	default:
		p->serial_in = io_serial_in;
		p->serial_out = io_serial_out;
		break;
	}
	/* Remember loaded iotype */
	up->cur_iotype = p->iotype;
	p->handle_irq = cpscom_default_handle_irq;
}

static void
serial_out_sync(struct uart_8250_port *up, int offset, int value)
{
	struct uart_port *p = &up->port;
	switch (p->iotype) {
	case UPIO_MEM:
	case UPIO_MEM32:
	case UPIO_AU:
	case UPIO_CPS:
		p->serial_out(p, offset, value);
		p->serial_in(p, UART_LCR);	/* safe, no side-effects */
		break;
	default:
		p->serial_out(p, offset, value);
	}
}

#define serial_in(up, offset)		\
	(up->port.serial_in(&(up)->port, (offset)))
#define serial_out(up, offset, value)	\
	(up->port.serial_out(&(up)->port, (offset), (value)))
/*
 * We used to support using pause I/O for certain machines.  We
 * haven't supported this for a while, but just in case it's badly
 * needed for certain old 386 machines, I've left these #define's
 * in....
 */
#define serial_inp(up, offset)		serial_in(up, offset)
#define serial_outp(up, offset, value)	serial_out(up, offset, value)

/* Uart divisor latch read */
static inline int _serial_dl_read(struct uart_8250_port *up)
{
	return serial_inp(up, UART_DLL) | serial_inp(up, UART_DLM) << 8;
}

/* Uart divisor latch write */
static inline void _serial_dl_write(struct uart_8250_port *up, int value)
{
	serial_outp(up, UART_DLL, value & 0xff);
	serial_outp(up, UART_DLM, value >> 8 & 0xff);
}

#if defined(CONFIG_MIPS_ALCHEMY)
/* Au1x00 haven't got a standard divisor latch */
static int serial_dl_read(struct uart_8250_port *up)
{
	if (up->port.iotype == UPIO_AU)
		return __raw_readl(up->port.membase + 0x28);
	else
		return _serial_dl_read(up);
}

static void serial_dl_write(struct uart_8250_port *up, int value)
{
	if (up->port.iotype == UPIO_AU)
		__raw_writel(value, up->port.membase + 0x28);
	else
		_serial_dl_write(up, value);
}
#elif defined(CONFIG_SERIAL_8250_RM9K)
static int serial_dl_read(struct uart_8250_port *up)
{
	return	(up->port.iotype == UPIO_RM9000) ?
		(((__raw_readl(up->port.membase + 0x10) << 8) |
		(__raw_readl(up->port.membase + 0x08) & 0xff)) & 0xffff) :
		_serial_dl_read(up);
}

static void serial_dl_write(struct uart_8250_port *up, int value)
{
	if (up->port.iotype == UPIO_RM9000) {
		__raw_writel(value, up->port.membase + 0x08);
		__raw_writel(value >> 8, up->port.membase + 0x10);
	} else {
		_serial_dl_write(up, value);
	}
}
#else
#define serial_dl_read(up) _serial_dl_read(up)
#define serial_dl_write(up, value) _serial_dl_write(up, value)
#endif

/*
 * For the 16C950
 */
static void serial_icr_write(struct uart_8250_port *up, int offset, int value)
{
	serial_out(up, UART_SCR, offset);
	serial_out(up, UART_ICR, value);
}

static unsigned int serial_icr_read(struct uart_8250_port *up, int offset)
{
	unsigned int value;

	serial_icr_write(up, UART_ACR, up->acr | UART_ACR_ICRRD);
	serial_out(up, UART_SCR, offset);
	value = serial_in(up, UART_ICR);
	serial_icr_write(up, UART_ACR, up->acr);

	return value;
}

/*
 * FIFO support.
 */
static void cpscom_clear_fifos(struct uart_8250_port *p)
{
	if (p->capabilities & UART_CAP_FIFO) {
		serial_outp(p, UART_FCR, UART_FCR_ENABLE_FIFO);
		serial_outp(p, UART_FCR, UART_FCR_ENABLE_FIFO |
			       UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT);
		serial_outp(p, UART_FCR, 0);
	}
}

/*
 * IER sleep support.  UARTs which have EFRs need the "extended
 * capability" bit enabled.  Note that on XR16C850s, we need to
 * reset LCR to write to IER.
 */
static void cpscom_set_sleep(struct uart_8250_port *p, int sleep)
{
	if (p->capabilities & UART_CAP_SLEEP) {
		if (p->capabilities & UART_CAP_EFR) {
			serial_outp(p, UART_LCR, UART_LCR_CONF_MODE_B);
			serial_outp(p, UART_EFR, UART_EFR_ECB);
			serial_outp(p, UART_LCR, 0);
		}
		serial_outp(p, UART_IER, sleep ? UART_IERX_SLEEP : 0);
		if (p->capabilities & UART_CAP_EFR) {
			serial_outp(p, UART_LCR, UART_LCR_CONF_MODE_B);
			serial_outp(p, UART_EFR, 0);
			serial_outp(p, UART_LCR, 0);
		}
	}
}

#ifdef CONFIG_SERIAL_8250_RSA
/*
 * Attempts to turn on the RSA FIFO.  Returns zero on failure.
 * We set the port uart clock rate if we succeed.
 */
static int __enable_rsa(struct uart_8250_port *up)
{
	unsigned char mode;
	int result;

	mode = serial_inp(up, UART_RSA_MSR);
	result = mode & UART_RSA_MSR_FIFO;

	if (!result) {
		serial_outp(up, UART_RSA_MSR, mode | UART_RSA_MSR_FIFO);
		mode = serial_inp(up, UART_RSA_MSR);
		result = mode & UART_RSA_MSR_FIFO;
	}

	if (result)
		up->port.uartclk = SERIAL_RSA_BAUD_BASE * 16;

	return result;
}

static void enable_rsa(struct uart_8250_port *up)
{
	if (up->port.type == PORT_RSA) {
		if (up->port.uartclk != SERIAL_RSA_BAUD_BASE * 16) {
			spin_lock_irq(&up->port.lock);
			__enable_rsa(up);
			spin_unlock_irq(&up->port.lock);
		}
		if (up->port.uartclk == SERIAL_RSA_BAUD_BASE * 16)
			serial_outp(up, UART_RSA_FRR, 0);
	}
}

/*
 * Attempts to turn off the RSA FIFO.  Returns zero on failure.
 * It is unknown why interrupts were disabled in here.  However,
 * the caller is expected to preserve this behaviour by grabbing
 * the spinlock before calling this function.
 */
static void disable_rsa(struct uart_8250_port *up)
{
	unsigned char mode;
	int result;

	if (up->port.type == PORT_RSA &&
	    up->port.uartclk == SERIAL_RSA_BAUD_BASE * 16) {
		spin_lock_irq(&up->port.lock);

		mode = serial_inp(up, UART_RSA_MSR);
		result = !(mode & UART_RSA_MSR_FIFO);

		if (!result) {
			serial_outp(up, UART_RSA_MSR, mode & ~UART_RSA_MSR_FIFO);
			mode = serial_inp(up, UART_RSA_MSR);
			result = !(mode & UART_RSA_MSR_FIFO);
		}

		if (result)
			up->port.uartclk = SERIAL_RSA_BAUD_BASE_LO * 16;
		spin_unlock_irq(&up->port.lock);
	}
}
#endif /* CONFIG_SERIAL_8250_RSA */

/*
 * This is a quickie test to see how big the FIFO is.
 * It doesn't work at all the time, more's the pity.
 */
static int size_fifo(struct uart_8250_port *up)
{
	unsigned char old_fcr, old_mcr, old_lcr;
	unsigned short old_dl;
	int count;

	old_lcr = serial_inp(up, UART_LCR);
	serial_outp(up, UART_LCR, 0);
	old_fcr = serial_inp(up, UART_FCR);
	old_mcr = serial_inp(up, UART_MCR);
	serial_outp(up, UART_FCR, UART_FCR_ENABLE_FIFO |
		    UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT);
	serial_outp(up, UART_MCR, UART_MCR_LOOP);
	serial_outp(up, UART_LCR, UART_LCR_CONF_MODE_A);
	old_dl = serial_dl_read(up);
	serial_dl_write(up, 0x0001);
	serial_outp(up, UART_LCR, 0x03);
	for (count = 0; count < 256; count++)
		serial_outp(up, UART_TX, count);
	mdelay(20);/* FIXME - schedule_timeout */
	for (count = 0; (serial_inp(up, UART_LSR) & UART_LSR_DR) &&
	     (count < 256); count++)
		serial_inp(up, UART_RX);
	serial_outp(up, UART_FCR, old_fcr);
	serial_outp(up, UART_MCR, old_mcr);
	serial_outp(up, UART_LCR, UART_LCR_CONF_MODE_A);
	serial_dl_write(up, old_dl);
	serial_outp(up, UART_LCR, old_lcr);

	return count;
}

/*
 * Read UART ID using the divisor method - set DLL and DLM to zero
 * and the revision will be in DLL and device type in DLM.  We
 * preserve the device state across this.
 */
static unsigned int autoconfig_read_divisor_id(struct uart_8250_port *p)
{
	unsigned char old_dll, old_dlm, old_lcr;
	unsigned int id;

	old_lcr = serial_inp(p, UART_LCR);
	serial_outp(p, UART_LCR, UART_LCR_CONF_MODE_A);

	old_dll = serial_inp(p, UART_DLL);
	old_dlm = serial_inp(p, UART_DLM);

	serial_outp(p, UART_DLL, 0);
	serial_outp(p, UART_DLM, 0);

	id = serial_inp(p, UART_DLL) | serial_inp(p, UART_DLM) << 8;

	serial_outp(p, UART_DLL, old_dll);
	serial_outp(p, UART_DLM, old_dlm);
	serial_outp(p, UART_LCR, old_lcr);

	return id;
}

/*
 * This is a helper routine to autodetect StarTech/Exar/Oxsemi UART's.
 * When this function is called we know it is at least a StarTech
 * 16650 V2, but it might be one of several StarTech UARTs, or one of
 * its clones.  (We treat the broken original StarTech 16650 V1 as a
 * 16550, and why not?  Startech doesn't seem to even acknowledge its
 * existence.)
 *
 * What evil have men's minds wrought...
 */
static void autoconfig_has_efr(struct uart_8250_port *up)
{
	unsigned int id1, id2, id3, rev;

	/*
	 * Everything with an EFR has SLEEP
	 */
	up->capabilities |= UART_CAP_EFR | UART_CAP_SLEEP;

	/*
	 * First we check to see if it's an Oxford Semiconductor UART.
	 *
	 * If we have to do this here because some non-National
	 * Semiconductor clone chips lock up if you try writing to the
	 * LSR register (which serial_icr_read does)
	 */

	/*
	 * Check for Oxford Semiconductor 16C950.
	 *
	 * EFR [4] must be set else this test fails.
	 *
	 * This shouldn't be necessary, but Mike Hudson (Exoray@isys.ca)
	 * claims that it's needed for 952 dual UART's (which are not
	 * recommended for new designs).
	 */
	up->acr = 0;
	serial_out(up, UART_LCR, UART_LCR_CONF_MODE_B);
	serial_out(up, UART_EFR, UART_EFR_ECB);
	serial_out(up, UART_LCR, 0x00);
	id1 = serial_icr_read(up, UART_ID1);
	id2 = serial_icr_read(up, UART_ID2);
	id3 = serial_icr_read(up, UART_ID3);
	rev = serial_icr_read(up, UART_REV);

	DEBUG_AUTOCONF("950id=%02x:%02x:%02x:%02x ", id1, id2, id3, rev);

	if (id1 == 0x16 && id2 == 0xC9 &&
	    (id3 == 0x50 || id3 == 0x52 || id3 == 0x54)) {
		up->port.type = PORT_16C950;

		/*
		 * Enable work around for the Oxford Semiconductor 952 rev B
		 * chip which causes it to seriously miscalculate baud rates
		 * when DLL is 0.
		 */
		if (id3 == 0x52 && rev == 0x01)
			up->bugs |= UART_BUG_QUOT;
		return;
	}

	/*
	 * We check for a XR16C850 by setting DLL and DLM to 0, and then
	 * reading back DLL and DLM.  The chip type depends on the DLM
	 * value read back:
	 *  0x10 - XR16C850 and the DLL contains the chip revision.
	 *  0x12 - XR16C2850.
	 *  0x14 - XR16C854.
	 */
	id1 = autoconfig_read_divisor_id(up);
	DEBUG_AUTOCONF("850id=%04x ", id1);

	id2 = id1 >> 8;
	if (id2 == 0x10 || id2 == 0x12 || id2 == 0x14) {
		up->port.type = PORT_16850;
		return;
	}

	/*
	 * It wasn't an XR16C850.
	 *
	 * We distinguish between the '654 and the '650 by counting
	 * how many bytes are in the FIFO.  I'm using this for now,
	 * since that's the technique that was sent to me in the
	 * serial driver update, but I'm not convinced this works.
	 * I've had problems doing this in the past.  -TYT
	 */
	if (size_fifo(up) == 64)
		up->port.type = PORT_16654;
	else
		up->port.type = PORT_16650V2;
}

/*
 * We detected a chip without a FIFO.  Only two fall into
 * this category - the original 8250 and the 16450.  The
 * 16450 has a scratch register (accessible with LCR=0)
 */
static void autoconfig_8250(struct uart_8250_port *up)
{
	unsigned char scratch, status1, status2;

	up->port.type = PORT_8250;

	scratch = serial_in(up, UART_SCR);
	serial_outp(up, UART_SCR, 0xa5);
	status1 = serial_in(up, UART_SCR);
	serial_outp(up, UART_SCR, 0x5a);
	status2 = serial_in(up, UART_SCR);
	serial_outp(up, UART_SCR, scratch);

	if (status1 == 0xa5 && status2 == 0x5a)
		up->port.type = PORT_16450;
}

static int broken_efr(struct uart_8250_port *up)
{
	/*
	 * Exar ST16C2550 "A2" devices incorrectly detect as
	 * having an EFR, and report an ID of 0x0201.  See
	 * http://linux.derkeiler.com/Mailing-Lists/Kernel/2004-11/4812.html 
	 */
	if (autoconfig_read_divisor_id(up) == 0x0201 && size_fifo(up) == 16)
		return 1;

	return 0;
}

static inline int ns16550a_goto_highspeed(struct uart_8250_port *up)
{
	unsigned char status;

	status = serial_in(up, 0x04); /* EXCR2 */
#define PRESL(x) ((x) & 0x30)
	if (PRESL(status) == 0x10) {
		/* already in high speed mode */
		return 0;
	} else {
		status &= ~0xB0; /* Disable LOCK, mask out PRESL[01] */
		status |= 0x10;  /* 1.625 divisor for baud_base --> 921600 */
		serial_outp(up, 0x04, status);
	}
	return 1;
}

/*
 * We know that the chip has FIFOs.  Does it have an EFR?  The
 * EFR is located in the same register position as the IIR and
 * we know the top two bits of the IIR are currently set.  The
 * EFR should contain zero.  Try to read the EFR.
 */
static void autoconfig_16550a(struct uart_8250_port *up)
{
	unsigned char status1, status2;
	unsigned int iersave;

	up->port.type = PORT_16550A;
	up->capabilities |= UART_CAP_FIFO;

	/*
	 * Check for presence of the EFR when DLAB is set.
	 * Only ST16C650V1 UARTs pass this test.
	 */
	serial_outp(up, UART_LCR, UART_LCR_CONF_MODE_A);
	if (serial_in(up, UART_EFR) == 0) {
		serial_outp(up, UART_EFR, 0xA8);
		if (serial_in(up, UART_EFR) != 0) {
			DEBUG_AUTOCONF("EFRv1 ");
			up->port.type = PORT_16650;
			up->capabilities |= UART_CAP_EFR | UART_CAP_SLEEP;
		} else {
			DEBUG_AUTOCONF("Motorola 8xxx DUART ");
		}
		serial_outp(up, UART_EFR, 0);
		return;
	}

	/*
	 * Maybe it requires 0xbf to be written to the LCR.
	 * (other ST16C650V2 UARTs, TI16C752A, etc)
	 */
	serial_outp(up, UART_LCR, UART_LCR_CONF_MODE_B);
	if (serial_in(up, UART_EFR) == 0 && !broken_efr(up)) {
		DEBUG_AUTOCONF("EFRv2 ");
		if( up->port.iotype == UPIO_CPS && up->port.line != -1 ){
			unsigned char id = 0;
			unsigned char fpgaVer = 0;
			//  CONPROSYS Devices gets product id and fpga version.
			id = contec_mcs341_device_productid_get( up->port.line / 2 );
			fpgaVer = contec_mcs341_device_fpga_version_get( up->port.line / 2 );

			if( ( id == CPS_DEVICE_COM2PC ||
					id == CPS_DEVICE_COM2PD ||
					id == CPS_DEVICE_COM1PC ||
					id == CPS_DEVICE_COM1PC ) &&
					fpgaVer < 2 ){
				up->port.type = PORT_CPS16550;
				serial_outp(up, UART_LCR, UART_LCR_CONF_MODE_A);
				serial_outp(up, UART_EFR, 0);
				serial_outp(up, UART_LCR, 0);
				return;
			}
			// new fpga mode
			up->port.type = PORT_CPS16550A;
			//serial_outp(up, UART_LCR, UART_LCR_CONF_MODE_A);
			//serial_outp(up, UART_EFR, 4);
			serial_outp(up, UART_LCR, 0);
			return;
		}
		autoconfig_has_efr(up);
		return;
	}

	/*
	 * Check for a National Semiconductor SuperIO chip.
	 * Attempt to switch to bank 2, read the value of the LOOP bit
	 * from EXCR1. Switch back to bank 0, change it in MCR. Then
	 * switch back to bank 2, read it from EXCR1 again and check
	 * it's changed. If so, set baud_base in EXCR2 to 921600. -- dwmw2
	 */
	serial_outp(up, UART_LCR, 0);
	status1 = serial_in(up, UART_MCR);
	serial_outp(up, UART_LCR, 0xE0);
	status2 = serial_in(up, 0x02); /* EXCR1 */

	if (!((status2 ^ status1) & UART_MCR_LOOP)) {
		serial_outp(up, UART_LCR, 0);
		serial_outp(up, UART_MCR, status1 ^ UART_MCR_LOOP);
		serial_outp(up, UART_LCR, 0xE0);
		status2 = serial_in(up, 0x02); /* EXCR1 */
		serial_outp(up, UART_LCR, 0);
		serial_outp(up, UART_MCR, status1);

		if ((status2 ^ status1) & UART_MCR_LOOP) {
			unsigned short quot;

			serial_outp(up, UART_LCR, 0xE0);

			quot = serial_dl_read(up);
			quot <<= 3;

			if (ns16550a_goto_highspeed(up))
				serial_dl_write(up, quot);

			serial_outp(up, UART_LCR, 0);

			up->port.uartclk = 921600*16;
			up->port.type = PORT_NS16550A;
			up->capabilities |= UART_NATSEMI;
			return;
		}
	}

	/*
	 * No EFR.  Try to detect a TI16750, which only sets bit 5 of
	 * the IIR when 64 byte FIFO mode is enabled when DLAB is set.
	 * Try setting it with and without DLAB set.  Cheap clones
	 * set bit 5 without DLAB set.
	 */
	serial_outp(up, UART_LCR, 0);
	serial_outp(up, UART_FCR, UART_FCR_ENABLE_FIFO | UART_FCR7_64BYTE);
	status1 = serial_in(up, UART_IIR) >> 5;
	serial_outp(up, UART_FCR, UART_FCR_ENABLE_FIFO);
	serial_outp(up, UART_LCR, UART_LCR_CONF_MODE_A);
	serial_outp(up, UART_FCR, UART_FCR_ENABLE_FIFO | UART_FCR7_64BYTE);
	status2 = serial_in(up, UART_IIR) >> 5;
	serial_outp(up, UART_FCR, UART_FCR_ENABLE_FIFO);
	serial_outp(up, UART_LCR, 0);

	DEBUG_AUTOCONF("iir1=%d iir2=%d ", status1, status2);

	if (status1 == 6 && status2 == 7) {
		up->port.type = PORT_16750;
		up->capabilities |= UART_CAP_AFE | UART_CAP_SLEEP;
		return;
	}

	/*
	 * Try writing and reading the UART_IER_UUE bit (b6).
	 * If it works, this is probably one of the Xscale platform's
	 * internal UARTs.
	 * We're going to explicitly set the UUE bit to 0 before
	 * trying to write and read a 1 just to make sure it's not
	 * already a 1 and maybe locked there before we even start start.
	 */
	iersave = serial_in(up, UART_IER);
	serial_outp(up, UART_IER, iersave & ~UART_IER_UUE);
	if (!(serial_in(up, UART_IER) & UART_IER_UUE)) {
		/*
		 * OK it's in a known zero state, try writing and reading
		 * without disturbing the current state of the other bits.
		 */
		serial_outp(up, UART_IER, iersave | UART_IER_UUE);
		if (serial_in(up, UART_IER) & UART_IER_UUE) {
			/*
			 * It's an Xscale.
			 * We'll leave the UART_IER_UUE bit set to 1 (enabled).
			 */
			DEBUG_AUTOCONF("Xscale ");
			up->port.type = PORT_XSCALE;
			up->capabilities |= UART_CAP_UUE | UART_CAP_RTOIE;
			return;
		}
	} else {
		/*
		 * If we got here we couldn't force the IER_UUE bit to 0.
		 * Log it and continue.
		 */
		DEBUG_AUTOCONF("Couldn't force IER_UUE to 0 ");
	}
	serial_outp(up, UART_IER, iersave);

	/*
	 * Exar uarts have EFR in a weird location
	 */
	if (up->port.flags & UPF_EXAR_EFR) {
		up->port.type = PORT_XR17D15X;
		up->capabilities |= UART_CAP_AFE | UART_CAP_EFR;
	}

	/*
	 * We distinguish between 16550A and U6 16550A by counting
	 * how many bytes are in the FIFO.
	 */
	if (up->port.type == PORT_16550A && size_fifo(up) == 64) {
		up->port.type = PORT_U6_16550A;
		up->capabilities |= UART_CAP_AFE;
	}
}

/*
 * This routine is called by rs_init() to initialize a specific serial
 * port.  It determines what type of UART chip this serial port is
 * using: 8250, 16450, 16550, 16550A.  The important question is
 * whether or not this UART is a 16550A or not, since this will
 * determine whether or not we can use its FIFO features or not.
 */
static void autoconfig(struct uart_8250_port *up, unsigned int probeflags)
{
	unsigned char status1, scratch, scratch2, scratch3;
	unsigned char save_lcr, save_mcr;
	unsigned long flags;

	if (!up->port.iobase && !up->port.mapbase && !up->port.membase)
		return;

	DEBUG_AUTOCONF("ttyCPS%d: autoconf (0x%04lx, 0x%p): ",
		       serial_index(&up->port), up->port.iobase, up->port.membase);

	/*
	 * We really do need global IRQs disabled here - we're going to
	 * be frobbing the chips IRQ enable register to see if it exists.
	 */
	spin_lock_irqsave(&up->port.lock, flags);

	up->capabilities = 0;
	up->bugs = 0;

	if (!(up->port.flags & UPF_BUGGY_UART)) {
		/*
		 * Do a simple existence test first; if we fail this,
		 * there's no point trying anything else.
		 *
		 * 0x80 is used as a nonsense port to prevent against
		 * false positives due to ISA bus float.  The
		 * assumption is that 0x80 is a non-existent port;
		 * which should be safe since include/asm/io.h also
		 * makes this assumption.
		 *
		 * Note: this is safe as long as MCR bit 4 is clear
		 * and the device is in "PC" mode.
		 */
		scratch = serial_inp(up, UART_IER);
		serial_outp(up, UART_IER, 0);
#ifdef __i386__
		outb(0xff, 0x080);
#endif
		/*
		 * Mask out IER[7:4] bits for test as some UARTs (e.g. TL
		 * 16C754B) allow only to modify them if an EFR bit is set.
		 */
		scratch2 = serial_inp(up, UART_IER) & 0x0f;
		serial_outp(up, UART_IER, 0x0F);
#ifdef __i386__
		outb(0, 0x080);
#endif
		scratch3 = serial_inp(up, UART_IER) & 0x0f;
		serial_outp(up, UART_IER, scratch);
		if (scratch2 != 0 || scratch3 != 0x0F) {
			/*
			 * We failed; there's nothing here
			 */
			DEBUG_AUTOCONF("IER test failed (%02x, %02x) ",
				       scratch2, scratch3);
			goto out;
		}
	}

	save_mcr = serial_in(up, UART_MCR);
	save_lcr = serial_in(up, UART_LCR);

	/*
	 * Check to see if a UART is really there.  Certain broken
	 * internal modems based on the Rockwell chipset fail this
	 * test, because they apparently don't implement the loopback
	 * test mode.  So this test is skipped on the COM 1 through
	 * COM 4 ports.  This *should* be safe, since no board
	 * manufacturer would be stupid enough to design a board
	 * that conflicts with COM 1-4 --- we hope!
	 */
	if (!(up->port.flags & UPF_SKIP_TEST)) {
		serial_outp(up, UART_MCR, UART_MCR_LOOP | 0x0A);
		status1 = serial_inp(up, UART_MSR) & 0xF0;
		serial_outp(up, UART_MCR, save_mcr);
		if (status1 != 0x90) {
			DEBUG_AUTOCONF("LOOP test failed (%02x) ",
				       status1);
			goto out;
		}
	}

	/*
	 * We're pretty sure there's a port here.  Lets find out what
	 * type of port it is.  The IIR top two bits allows us to find
	 * out if it's 8250 or 16450, 16550, 16550A or later.  This
	 * determines what we test for next.
	 *
	 * We also initialise the EFR (if any) to zero for later.  The
	 * EFR occupies the same register location as the FCR and IIR.
	 */
	serial_outp(up, UART_LCR, UART_LCR_CONF_MODE_B);
	serial_outp(up, UART_EFR, 0);
	serial_outp(up, UART_LCR, 0);

	serial_outp(up, UART_FCR, UART_FCR_ENABLE_FIFO);
	scratch = serial_in(up, UART_IIR) >> 6;

	DEBUG_AUTOCONF("iir=%d ", scratch);

	switch (scratch) {
	case 0:
		autoconfig_8250(up);
		break;
	case 1:
		up->port.type = PORT_UNKNOWN;
		break;
	case 2:
		up->port.type = PORT_16550;
		break;
	case 3:
		autoconfig_16550a(up);
		break;
	}

#ifdef CONFIG_SERIAL_8250_RSA
	/*
	 * Only probe for RSA ports if we got the region.
	 */
	if (up->port.type == PORT_16550A && probeflags & PROBE_RSA) {
		int i;

		for (i = 0 ; i < probe_rsa_count; ++i) {
			if (probe_rsa[i] == up->port.iobase &&
			    __enable_rsa(up)) {
				up->port.type = PORT_RSA;
				break;
			}
		}
	}
#endif

	serial_outp(up, UART_LCR, save_lcr);

	if (up->capabilities != uart_config[up->port.type].flags) {
		printk(KERN_WARNING
		       "ttyCPS%d: detected caps %08x should be %08x\n",
		       serial_index(&up->port), up->capabilities,
		       uart_config[up->port.type].flags);
	}

	up->port.fifosize = uart_config[up->port.type].fifo_size;
	up->capabilities = uart_config[up->port.type].flags;
	up->tx_loadsz = uart_config[up->port.type].tx_loadsz;

	if (up->port.type == PORT_UNKNOWN)
		goto out;

	/*
	 * Reset the UART.
	 */
#ifdef CONFIG_SERIAL_8250_RSA
	if (up->port.type == PORT_RSA)
		serial_outp(up, UART_RSA_FRR, 0);
#endif
	serial_outp(up, UART_MCR, save_mcr);
	cpscom_clear_fifos(up);
	serial_in(up, UART_RX);
	if (up->capabilities & UART_CAP_UUE)
		serial_outp(up, UART_IER, UART_IER_UUE);
	else
		serial_outp(up, UART_IER, 0);

 out:
	spin_unlock_irqrestore(&up->port.lock, flags);
	DEBUG_AUTOCONF("type=%s\n", uart_config[up->port.type].name);
}

static void autoconfig_irq(struct uart_8250_port *up)
{
	unsigned char save_mcr, save_ier;
	unsigned char save_ICP = 0;
	unsigned int ICP = 0;
	unsigned long irqs;
	int irq;

	if (up->port.flags & UPF_FOURPORT) {
		ICP = (up->port.iobase & 0xfe0) | 0x1f;
		save_ICP = inb_p(ICP);
		outb_p(0x80, ICP);
		(void) inb_p(ICP);
	}

	/* forget possible initially masked and pending IRQ */
	probe_irq_off(probe_irq_on());
	save_mcr = serial_inp(up, UART_MCR);
	save_ier = serial_inp(up, UART_IER);
	serial_outp(up, UART_MCR, UART_MCR_OUT1 | UART_MCR_OUT2);

	irqs = probe_irq_on();
	serial_outp(up, UART_MCR, 0);
	udelay(10);
	if (up->port.flags & UPF_FOURPORT) {
		serial_outp(up, UART_MCR,
			    UART_MCR_DTR | UART_MCR_RTS);
	} else {
		serial_outp(up, UART_MCR,
			    UART_MCR_DTR | UART_MCR_RTS | UART_MCR_OUT2);
	}
	serial_outp(up, UART_IER, 0x0f);	/* enable all intrs */
	(void)serial_inp(up, UART_LSR);
	(void)serial_inp(up, UART_RX);
	(void)serial_inp(up, UART_IIR);
	(void)serial_inp(up, UART_MSR);
	serial_outp(up, UART_TX, 0xFF);
	udelay(20);
	irq = probe_irq_off(irqs);
	serial_outp(up, UART_MCR, save_mcr);
	serial_outp(up, UART_IER, save_ier);

	if (up->port.flags & UPF_FOURPORT)
		outb_p(save_ICP, ICP);

	up->port.irq = (irq > 0) ? irq : 0;
}

static inline void __stop_tx(struct uart_8250_port *p)
{
	if (p->ier & UART_IER_THRI) {
		p->ier &= ~UART_IER_THRI;
		serial_out(p, UART_IER, p->ier);
	}
}

static void cpscom_stop_tx(struct uart_port *port)
{
	struct uart_8250_port *up =
		container_of(port, struct uart_8250_port, port);

	__stop_tx(up);

	/*
	 * We really want to stop the transmitter from sending.
	 */
	if (up->port.type == PORT_16C950) {
		up->acr |= UART_ACR_TXDIS;
		serial_icr_write(up, UART_ACR, up->acr);
	}
}

static void transmit_chars(struct uart_8250_port *up);

static void cpscom_start_tx(struct uart_port *port)
{
	struct uart_8250_port *up =
		container_of(port, struct uart_8250_port, port);

	if (!(up->ier & UART_IER_THRI)) {
		up->ier |= UART_IER_THRI;
		serial_out(up, UART_IER, up->ier);

		if (up->bugs & UART_BUG_TXEN) {
			unsigned char lsr;
			lsr = serial_in(up, UART_LSR);
			up->lsr_saved_flags |= lsr & LSR_SAVE_FLAGS;
			if ((up->port.type == PORT_RM9000) ?
				(lsr & UART_LSR_THRE) :
				(lsr & UART_LSR_TEMT))
				transmit_chars(up);
		}
	}

	/*
	 * Re-enable the transmitter if we disabled it.
	 */
	if (up->port.type == PORT_16C950 && up->acr & UART_ACR_TXDIS) {
		up->acr &= ~UART_ACR_TXDIS;
		serial_icr_write(up, UART_ACR, up->acr);
	}
}

static void cpscom_stop_rx(struct uart_port *port)
{
	struct uart_8250_port *up =
		container_of(port, struct uart_8250_port, port);

	up->ier &= ~UART_IER_RLSI;
	up->port.read_status_mask &= ~UART_LSR_DR;
	serial_out(up, UART_IER, up->ier);
}

static void cpscom_enable_ms(struct uart_port *port)
{
	struct uart_8250_port *up =
		container_of(port, struct uart_8250_port, port);

	/* no MSR capabilities */
	if (up->bugs & UART_BUG_NOMSR)
		return;

	up->ier |= UART_IER_MSI;
	serial_out(up, UART_IER, up->ier);
}

/*
 * Clear the Tegra rx fifo after a break
 *
 * FIXME: This needs to become a port specific callback once we have a
 * framework for this
 */
static void clear_rx_fifo(struct uart_8250_port *up)
{
	unsigned int status, tmout = 10000;
	do {
		status = serial_in(up, UART_LSR);
		if (status & (UART_LSR_FIFOE | UART_LSR_BRK_ERROR_BITS))
			status = serial_in(up, UART_RX);
		else
			break;
		if (--tmout == 0)
			break;
		udelay(1);
	} while (1);
}

static void
receive_chars(struct uart_8250_port *up, unsigned int *status)
{
	struct tty_struct *tty = up->port.state->port.tty;
	unsigned char ch, lsr = *status;
	int max_count = 256;
	char flag;

	do {
		if (likely(lsr & UART_LSR_DR))
			ch = serial_inp(up, UART_RX);
		else
			/*
			 * Intel 82571 has a Serial Over Lan device that will
			 * set UART_LSR_BI without setting UART_LSR_DR when
			 * it receives a break. To avoid reading from the
			 * receive buffer without UART_LSR_DR bit set, we
			 * just force the read character to be 0
			 */
			ch = 0;

		flag = TTY_NORMAL;
		up->port.icount.rx++;

		lsr |= up->lsr_saved_flags;
		up->lsr_saved_flags = 0;

		if (unlikely(lsr & UART_LSR_BRK_ERROR_BITS)) {
			/*
			 * For statistics only
			 */
			if (lsr & UART_LSR_BI) {
				lsr &= ~(UART_LSR_FE | UART_LSR_PE);
				up->port.icount.brk++;
				/*
				 * If tegra port then clear the rx fifo to
				 * accept another break/character.
				 */
				if (up->port.type == PORT_TEGRA)
					clear_rx_fifo(up);

				/*
				 * We do the SysRQ and SAK checking
				 * here because otherwise the break
				 * may get masked by ignore_status_mask
				 * or read_status_mask.
				 */
				if (uart_handle_break(&up->port))
					goto ignore_char;
			} else if (lsr & UART_LSR_PE)
				up->port.icount.parity++;
			else if (lsr & UART_LSR_FE)
				up->port.icount.frame++;
			if (lsr & UART_LSR_OE)
				up->port.icount.overrun++;

			/*
			 * Mask off conditions which should be ignored.
			 */
			lsr &= up->port.read_status_mask;

			if (lsr & UART_LSR_BI) {
				DEBUG_INTR("handling break....");
				flag = TTY_BREAK;
			} else if (lsr & UART_LSR_PE)
				flag = TTY_PARITY;
			else if (lsr & UART_LSR_FE)
				flag = TTY_FRAME;
		}
		if (uart_handle_sysrq_char(&up->port, ch))
			goto ignore_char;

		uart_insert_char(&up->port, lsr, UART_LSR_OE, ch, flag);

ignore_char:
		lsr = serial_inp(up, UART_LSR);
	} while ((lsr & (UART_LSR_DR | UART_LSR_BI)) && (max_count-- > 0));
	spin_unlock(&up->port.lock);
	tty_flip_buffer_push(tty);
	spin_lock(&up->port.lock);
	*status = lsr;
}

static void transmit_chars(struct uart_8250_port *up)
{
	struct circ_buf *xmit = &up->port.state->xmit;
	int count;

	if (up->port.x_char) {
		serial_outp(up, UART_TX, up->port.x_char);
		up->port.icount.tx++;
		up->port.x_char = 0;
		return;
	}
	if (uart_tx_stopped(&up->port)) {
		cpscom_stop_tx(&up->port);
		return;
	}
	if (uart_circ_empty(xmit)) {
		__stop_tx(up);
		return;
	}


	count = up->tx_loadsz;
	do {
		serial_out(up, UART_TX, xmit->buf[xmit->tail]);
		xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
		up->port.icount.tx++;
		if (uart_circ_empty(xmit))
			break;
	} while (--count > 0);

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(&up->port);

	DEBUG_INTR("THRE...");

	if (uart_circ_empty(xmit))
		__stop_tx(up);
}

static unsigned int check_modem_status(struct uart_8250_port *up)
{
	unsigned int status = serial_in(up, UART_MSR);

	status |= up->msr_saved_flags;
	up->msr_saved_flags = 0;

	if (up->rs485_flag != 0) {
		status |= UART_MSR_CTS;
	}

	if (status & UART_MSR_ANY_DELTA && up->ier & UART_IER_MSI &&
	    up->port.state != NULL) {
		if (status & UART_MSR_TERI)
			up->port.icount.rng++;
		if (status & UART_MSR_DDSR)
			up->port.icount.dsr++;
		if (status & UART_MSR_DDCD)
			uart_handle_dcd_change(&up->port, status & UART_MSR_DCD);
		if (status & UART_MSR_DCTS)
			uart_handle_cts_change(&up->port, status & UART_MSR_CTS);

		wake_up_interruptible(&up->port.state->port.delta_msr_wait);
	}

	return status;
}

/*
 * This handles the interrupt from one port.
 */
static void cpscom_handle_port(struct uart_8250_port *up)
{
	unsigned int status;
	unsigned long flags;

	spin_lock_irqsave(&up->port.lock, flags);

	status = serial_inp(up, UART_LSR);

	DEBUG_INTR("status = %x...", status);

	if (status & (UART_LSR_DR | UART_LSR_BI))
		receive_chars(up, &status);
	check_modem_status(up);
	if (status & UART_LSR_THRE)
		transmit_chars(up);

	spin_unlock_irqrestore(&up->port.lock, flags);
}

int cpscom_handle_irq(struct uart_port *port, unsigned int iir)
{
	struct uart_8250_port *up =
		container_of(port, struct uart_8250_port, port);

	if (!(iir & UART_IIR_NO_INT)) {
		cpscom_handle_port(up);
		return 1;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(cpscom_handle_irq);

static int cpscom_default_handle_irq(struct uart_port *port)
{
	struct uart_8250_port *up =
		container_of(port, struct uart_8250_port, port);
	unsigned int iir = serial_in(up, UART_IIR);

	return cpscom_handle_irq(port, iir);
}

/*
 * This is the serial driver's interrupt routine.
 *
 * Arjan thinks the old way was overly complex, so it got simplified.
 * Alan disagrees, saying that need the complexity to handle the weird
 * nature of ISA shared interrupts.  (This is a special exception.)
 *
 * In order to handle ISA shared interrupts properly, we need to check
 * that all ports have been serviced, and therefore the ISA interrupt
 * line has been de-asserted.
 *
 * This means we need to loop through all ports. checking that they
 * don't have an interrupt pending.
 */
static irqreturn_t cpscom_interrupt(int irq, void *dev_id)
{
	struct irq_info *i = dev_id;
	struct list_head *l, *end = NULL;
	int pass_counter = 0, handled = 0;

	DEBUG_INTR("cpscom_interrupt(%d)...", irq);

	spin_lock(&i->lock);

	l = i->head;
	do {
		struct uart_8250_port *up;
		struct uart_port *port;

		up = list_entry(l, struct uart_8250_port, list);
		port = &up->port;

		if (port->handle_irq(port)) {
			handled = 1;
			end = NULL;
		} else if (end == NULL)
			end = l;

		l = l->next;

		if (l == i->head && pass_counter++ > PASS_LIMIT) {
			/* If we hit this, we're dead. */
			printk_ratelimited(KERN_ERR
				"cps-serial: too much work for irq%d\n", irq);
			break;
		}
	} while (l != end);

	spin_unlock(&i->lock);

	DEBUG_INTR("end.\n");

	return IRQ_RETVAL(handled);
}

/*
 * To support ISA shared interrupts, we need to have one interrupt
 * handler that ensures that the IRQ line has been deasserted
 * before returning.  Failing to do this will result in the IRQ
 * line being stuck active, and, since ISA irqs are edge triggered,
 * no more IRQs will be seen.
 */
static void serial_do_unlink(struct irq_info *i, struct uart_8250_port *up)
{
	spin_lock_irq(&i->lock);

	if (!list_empty(i->head)) {
		if (i->head == &up->list)
			i->head = i->head->next;
		list_del(&up->list);
	} else {
		BUG_ON(i->head != &up->list);
		i->head = NULL;
	}
	spin_unlock_irq(&i->lock);
	/* List empty so throw away the hash node */
	if (i->head == NULL) {
		hlist_del(&i->node);
		kfree(i);
	}
}

static int serial_link_irq_chain(struct uart_8250_port *up)
{
	struct hlist_head *h;
	struct hlist_node *n;
	struct irq_info *i;
	int devNum ;
	int ret, irq_flags = up->port.flags & UPF_SHARE_IRQ ? IRQF_SHARED : 0;

	mutex_lock(&hash_mutex);
	devNum = contec_mcs341_device_deviceNum_get( (unsigned long) up->port.mapbase );
//	h = &irq_lists[up->port.irq % NR_IRQ_HASH];
	h = &irq_lists[devNum % NR_IRQ_HASH];

	hlist_for_each(n, h) {
		i = hlist_entry(n, struct irq_info, node);
		if (i->irq == up->port.irq)
			break;
	}

	if (n == NULL) {
		i = kzalloc(sizeof(struct irq_info), GFP_KERNEL);
		if (i == NULL) {
			mutex_unlock(&hash_mutex);
			return -ENOMEM;
		}
		spin_lock_init(&i->lock);
		i->irq = up->port.irq;
		hlist_add_head(&i->node, h);
	}
	mutex_unlock(&hash_mutex);

	spin_lock_irq(&i->lock);

	if (i->head) {
		list_add(&up->list, i->head);
		spin_unlock_irq(&i->lock);

		ret = 0;
	} else {
		INIT_LIST_HEAD(&up->list);
		i->head = &up->list;
		spin_unlock_irq(&i->lock);
		irq_flags |= up->port.irqflags;
		ret = request_irq(up->port.irq, cpscom_interrupt,
				  irq_flags, "cpscom_serial", i);
		if (ret < 0)
			serial_do_unlink(i, up);
	}

	return ret;
}

static void serial_unlink_irq_chain(struct uart_8250_port *up)
{
	struct irq_info *i;
	struct hlist_node *n;
	struct hlist_head *h;
	int devNum;

	mutex_lock(&hash_mutex);

//	h = &irq_lists[up->port.irq % NR_IRQ_HASH];
	devNum = contec_mcs341_device_deviceNum_get( (unsigned long) up->port.mapbase );
	h = &irq_lists[ devNum % NR_IRQ_HASH ];

	hlist_for_each(n, h) {
		i = hlist_entry(n, struct irq_info, node);
		if (i->irq == up->port.irq)
			break;
	}

	BUG_ON(n == NULL);
	BUG_ON(i->head == NULL);

	if (list_empty(i->head))
		free_irq(up->port.irq, i);

	serial_do_unlink(i, up);
	mutex_unlock(&hash_mutex);
}

/*
 * This function is used to handle ports that do not have an
 * interrupt.  This doesn't work very well for 16450's, but gives
 * barely passable results for a 16550A.  (Although at the expense
 * of much CPU overhead).
 */
static void cpscom_timeout(unsigned long data)
{
	struct uart_8250_port *up = (struct uart_8250_port *)data;
	unsigned int iir;

	iir = serial_in(up, UART_IIR);
	if (!(iir & UART_IIR_NO_INT))
		cpscom_handle_port(up);
	mod_timer(&up->timer, jiffies + uart_poll_timeout(&up->port));
}

static void cpscom_backup_timeout(unsigned long data)
{
	struct uart_8250_port *up = (struct uart_8250_port *)data;
	unsigned int iir, ier = 0, lsr;
	unsigned long flags;

	spin_lock_irqsave(&up->port.lock, flags);

	/*
	 * Must disable interrupts or else we risk racing with the interrupt
	 * based handler.
	 */
	if (is_real_interrupt(up->port.irq)) {
		ier = serial_in(up, UART_IER);
		serial_out(up, UART_IER, 0);
	}

	iir = serial_in(up, UART_IIR);

	/*
	 * This should be a safe test for anyone who doesn't trust the
	 * IIR bits on their UART, but it's specifically designed for
	 * the "Diva" UART used on the management processor on many HP
	 * ia64 and parisc boxes.
	 */
	lsr = serial_in(up, UART_LSR);
	up->lsr_saved_flags |= lsr & LSR_SAVE_FLAGS;
	if ((iir & UART_IIR_NO_INT) && (up->ier & UART_IER_THRI) &&
	    (!uart_circ_empty(&up->port.state->xmit) || up->port.x_char) &&
	    (lsr & UART_LSR_THRE)) {
		iir &= ~(UART_IIR_ID | UART_IIR_NO_INT);
		iir |= UART_IIR_THRI;
	}

	if (!(iir & UART_IIR_NO_INT))
		transmit_chars(up);

	if (is_real_interrupt(up->port.irq))
		serial_out(up, UART_IER, ier);

	spin_unlock_irqrestore(&up->port.lock, flags);

	/* Standard timer interval plus 0.2s to keep the port running */
	mod_timer(&up->timer,
		jiffies + uart_poll_timeout(&up->port) + HZ / 5);
}

static unsigned int cpscom_tx_empty(struct uart_port *port)
{
	struct uart_8250_port *up =
		container_of(port, struct uart_8250_port, port);
	unsigned long flags;
	unsigned int lsr;

	spin_lock_irqsave(&up->port.lock, flags);
	lsr = serial_in(up, UART_LSR);
	up->lsr_saved_flags |= lsr & LSR_SAVE_FLAGS;
	spin_unlock_irqrestore(&up->port.lock, flags);

	return (lsr & BOTH_EMPTY) == BOTH_EMPTY ? TIOCSER_TEMT : 0;
}

static unsigned int cpscom_get_mctrl(struct uart_port *port)
{
	struct uart_8250_port *up =
		container_of(port, struct uart_8250_port, port);
	unsigned int status;
	unsigned int ret;

	status = check_modem_status(up);

	ret = 0;
	if (status & UART_MSR_DCD)
		ret |= TIOCM_CAR;
	if (status & UART_MSR_RI)
		ret |= TIOCM_RNG;
	if (status & UART_MSR_DSR)
		ret |= TIOCM_DSR;
	if (status & UART_MSR_CTS)
		ret |= TIOCM_CTS;

	return ret;
}

static void cpscom_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	struct uart_8250_port *up =
		container_of(port, struct uart_8250_port, port);
	unsigned char mcr = 0;

	if (mctrl & TIOCM_RTS)
		mcr |= UART_MCR_RTS;
	if (mctrl & TIOCM_DTR)
		mcr |= UART_MCR_DTR;
	if (mctrl & TIOCM_OUT1)
		mcr |= UART_MCR_OUT1;
	if (mctrl & TIOCM_OUT2)
		mcr |= UART_MCR_OUT2;
	if (mctrl & TIOCM_LOOP)
		mcr |= UART_MCR_LOOP;

	mcr = (mcr & up->mcr_mask) | up->mcr_force | up->mcr;

	serial_out(up, UART_MCR, mcr);
}

static void cpscom_break_ctl(struct uart_port *port, int break_state)
{
	struct uart_8250_port *up =
		container_of(port, struct uart_8250_port, port);
	unsigned long flags;

	spin_lock_irqsave(&up->port.lock, flags);
	if (break_state == -1)
		up->lcr |= UART_LCR_SBC;
	else
		up->lcr &= ~UART_LCR_SBC;
	serial_out(up, UART_LCR, up->lcr);
	spin_unlock_irqrestore(&up->port.lock, flags);
}

/*
 *	Wait for transmitter & holding register to empty
 */
static void wait_for_xmitr(struct uart_8250_port *up, int bits)
{
	unsigned int status, tmout = 10000;

	/* Wait up to 10ms for the character(s) to be sent. */
	for (;;) {
		status = serial_in(up, UART_LSR);

		up->lsr_saved_flags |= status & LSR_SAVE_FLAGS;

		if ((status & bits) == bits)
			break;
		if (--tmout == 0)
			break;
		udelay(1);
	}

	/* Wait up to 1s for flow control if necessary */
	if (up->port.flags & UPF_CONS_FLOW) {
		unsigned int tmout;
		for (tmout = 1000000; tmout; tmout--) {
			unsigned int msr = serial_in(up, UART_MSR);
			up->msr_saved_flags |= msr & MSR_SAVE_FLAGS;
			if (msr & UART_MSR_CTS)
				break;
			udelay(1);
			touch_nmi_watchdog();
		}
	}
}

#ifdef CONFIG_CONSOLE_POLL
/*
 * Console polling routines for writing and reading from the uart while
 * in an interrupt or debug context.
 */

static int cpscom_get_poll_char(struct uart_port *port)
{
	struct uart_8250_port *up =
		container_of(port, struct uart_8250_port, port);
	unsigned char lsr = serial_inp(up, UART_LSR);

	if (!(lsr & UART_LSR_DR))
		return NO_POLL_CHAR;

	return serial_inp(up, UART_RX);
}


static void cpscom_put_poll_char(struct uart_port *port,
			 unsigned char c)
{
	unsigned int ier;
	struct uart_8250_port *up =
		container_of(port, struct uart_8250_port, port);

	/*
	 *	First save the IER then disable the interrupts
	 */
	ier = serial_in(up, UART_IER);
	if (up->capabilities & UART_CAP_UUE)
		serial_out(up, UART_IER, UART_IER_UUE);
	else
		serial_out(up, UART_IER, 0);

	wait_for_xmitr(up, BOTH_EMPTY);
	/*
	 *	Send the character out.
	 *	If a LF, also do CR...
	 */
	serial_out(up, UART_TX, c);
	if (c == 10) {
		wait_for_xmitr(up, BOTH_EMPTY);
		serial_out(up, UART_TX, 13);
	}

	/*
	 *	Finally, wait for transmitter to become empty
	 *	and restore the IER
	 */
	wait_for_xmitr(up, BOTH_EMPTY);
	serial_out(up, UART_IER, ier);
}

#endif /* CONFIG_CONSOLE_POLL */

static int cpscom_startup(struct uart_port *port)
{
	struct uart_8250_port *up =
		container_of(port, struct uart_8250_port, port);
	unsigned long flags;
	unsigned char lsr, iir;
	int retval;

	up->port.fifosize = uart_config[up->port.type].fifo_size;
	up->tx_loadsz = uart_config[up->port.type].tx_loadsz;
	up->capabilities = uart_config[up->port.type].flags;
	up->mcr = 0;

	if (up->port.iotype != up->cur_iotype)
		set_io_from_upio(port);

	if (up->port.type == PORT_16C950) {
		/* Wake up and initialize UART */
		up->acr = 0;
		serial_outp(up, UART_LCR, UART_LCR_CONF_MODE_B);
		serial_outp(up, UART_EFR, UART_EFR_ECB);
		serial_outp(up, UART_IER, 0);
		serial_outp(up, UART_LCR, 0);
		serial_icr_write(up, UART_CSR, 0); /* Reset the UART */
		serial_outp(up, UART_LCR, 0xBF);
		serial_outp(up, UART_EFR, UART_EFR_ECB);
		serial_outp(up, UART_LCR, 0);
	}

#ifdef CONFIG_SERIAL_8250_RSA
	/*
	 * If this is an RSA port, see if we can kick it up to the
	 * higher speed clock.
	 */
	enable_rsa(up);
#endif

	/*
	 * Clear the FIFO buffers and disable them.
	 * (they will be reenabled in set_termios())
	 */
	cpscom_clear_fifos(up);

	/*
	 * Clear the interrupt registers.
	 */
	(void) serial_inp(up, UART_LSR);
	(void) serial_inp(up, UART_RX);
	(void) serial_inp(up, UART_IIR);
	(void) serial_inp(up, UART_MSR);

	/*
	 * At this point, there's no way the LSR could still be 0xff;
	 * if it is, then bail out, because there's likely no UART
	 * here.
	 */
	if (!(up->port.flags & UPF_BUGGY_UART) &&
	    (serial_inp(up, UART_LSR) == 0xff)) {
		printk_ratelimited(KERN_INFO "ttyCPS%d: LSR safety check engaged!\n",
				   serial_index(&up->port));
		return -ENODEV;
	}

	/*
	 * For a XR16C850, we need to set the trigger levels
	 */
	if (up->port.type == PORT_16850) {
		unsigned char fctr;

		serial_outp(up, UART_LCR, UART_LCR_CONF_MODE_B);

		fctr = serial_inp(up, UART_FCTR) & ~(UART_FCTR_RX|UART_FCTR_TX);
		serial_outp(up, UART_FCTR, fctr | UART_FCTR_TRGD | UART_FCTR_RX);
		serial_outp(up, UART_TRG, UART_TRG_96);
		serial_outp(up, UART_FCTR, fctr | UART_FCTR_TRGD | UART_FCTR_TX);
		serial_outp(up, UART_TRG, UART_TRG_96);

		serial_outp(up, UART_LCR, 0);
	}

	/* CPS16550A set fifos */
	if( up->port.type == PORT_CPS16550A ){
		unsigned char fctr;

		serial_outp(up, UART_LCR, UART_LCR_CONF_MODE_B);

		serial_outp(up, UART_TRG, 0x40);
		fctr = serial_inp(up, UART_FCTR) & ~(UART_FCTR_TX);
		serial_outp(up, UART_FCTR, fctr | UART_FCTR_RX);
		serial_outp(up, UART_TRG, 0x40);
		serial_outp(up, UART_FCTR, fctr | UART_FCTR_TX);

		serial_outp(up, UART_LCR, 0);
	}

	if (is_real_interrupt(up->port.irq)) {
		unsigned char iir1;
		/*
		 * Test for UARTs that do not reassert THRE when the
		 * transmitter is idle and the interrupt has already
		 * been cleared.  Real 16550s should always reassert
		 * this interrupt whenever the transmitter is idle and
		 * the interrupt is enabled.  Delays are necessary to
		 * allow register changes to become visible.
		 */
		spin_lock_irqsave(&up->port.lock, flags);
		if (up->port.irqflags & IRQF_SHARED)
			disable_irq_nosync(up->port.irq);

		wait_for_xmitr(up, UART_LSR_THRE);
		serial_out_sync(up, UART_IER, UART_IER_THRI);
		udelay(1); /* allow THRE to set */
		iir1 = serial_in(up, UART_IIR);
		serial_out(up, UART_IER, 0);
		serial_out_sync(up, UART_IER, UART_IER_THRI);
		udelay(1); /* allow a working UART time to re-assert THRE */
		iir = serial_in(up, UART_IIR);
		serial_out(up, UART_IER, 0);

		if (up->port.irqflags & IRQF_SHARED)
			enable_irq(up->port.irq);
		spin_unlock_irqrestore(&up->port.lock, flags);

		/*
		 * If the interrupt is not reasserted, setup a timer to
		 * kick the UART on a regular basis.
		 */
		if (!(iir1 & UART_IIR_NO_INT) && (iir & UART_IIR_NO_INT)) {
			up->bugs |= UART_BUG_THRE;
			pr_debug("ttyS%d - using backup timer\n",
				 serial_index(port));
		}
	}

	/*
	 * The above check will only give an accurate result the first time
	 * the port is opened so this value needs to be preserved.
	 */
	if (up->bugs & UART_BUG_THRE) {
		up->timer.function = cpscom_backup_timeout;
		up->timer.data = (unsigned long)up;
		mod_timer(&up->timer, jiffies +
			uart_poll_timeout(port) + HZ / 5);
	}

	/*
	 * If the "interrupt" for this port doesn't correspond with any
	 * hardware interrupt, we use a timer-based system.  The original
	 * driver used to do this with IRQ0.
	 */
	if (!is_real_interrupt(up->port.irq)) {
		up->timer.data = (unsigned long)up;
		mod_timer(&up->timer, jiffies + uart_poll_timeout(port));
	} else {
		retval = serial_link_irq_chain(up);
		if (retval)
			return retval;
	}

	/*
	 * Now, initialize the UART
	 */
	serial_outp(up, UART_LCR, UART_LCR_WLEN8);

	spin_lock_irqsave(&up->port.lock, flags);
	if (up->port.flags & UPF_FOURPORT) {
		if (!is_real_interrupt(up->port.irq))
			up->port.mctrl |= TIOCM_OUT1;
	} else
		/*
		 * Most PC uarts need OUT2 raised to enable interrupts.
		 */
		if (is_real_interrupt(up->port.irq)){
			up->port.mctrl |= TIOCM_OUT2;
			if(up->port.iotype == UPIO_CPS){
				up->port.mctrl &= ~TIOCM_OUT1;
			}
		}
	cpscom_set_mctrl(&up->port, up->port.mctrl);

	/* Serial over Lan (SoL) hack:
	   Intel 8257x Gigabit ethernet chips have a
	   16550 emulation, to be used for Serial Over Lan.
	   Those chips take a longer time than a normal
	   serial device to signalize that a transmission
	   data was queued. Due to that, the above test generally
	   fails. One solution would be to delay the reading of
	   iir. However, this is not reliable, since the timeout
	   is variable. So, let's just don't test if we receive
	   TX irq. This way, we'll never enable UART_BUG_TXEN.
	 */
	if (skip_txen_test || up->port.flags & UPF_NO_TXEN_TEST)
		goto dont_test_tx_en;

	/*
	 * Do a quick test to see if we receive an
	 * interrupt when we enable the TX irq.
	 */
	serial_outp(up, UART_IER, UART_IER_THRI);
	lsr = serial_in(up, UART_LSR);
	iir = serial_in(up, UART_IIR);
	serial_outp(up, UART_IER, 0);

	if (lsr & UART_LSR_TEMT && iir & UART_IIR_NO_INT) {
		if (!(up->bugs & UART_BUG_TXEN)) {
			up->bugs |= UART_BUG_TXEN;
			pr_debug("ttyS%d - enabling bad tx status workarounds\n",
				 serial_index(port));
		}
	} else {
		up->bugs &= ~UART_BUG_TXEN;
	}

dont_test_tx_en:

	// 2018.01.19
	if( up->port.type != PORT_CPS16550 ){
		serial_outp(up, UART_LCR, UART_LCR_CONF_MODE_A );
		serial_outp(up, UART_FCR, 0x80 );
		serial_outp(up, UART_LCR, 0);
	}

	spin_unlock_irqrestore(&up->port.lock, flags);

	/*
	 * Clear the interrupt registers again for luck, and clear the
	 * saved flags to avoid getting false values from polling
	 * routines or the previous session.
	 */
	serial_inp(up, UART_LSR);
	serial_inp(up, UART_RX);
	serial_inp(up, UART_IIR);
	serial_inp(up, UART_MSR);
	up->lsr_saved_flags = 0;
	up->msr_saved_flags = 0;

	/*
	 * Finally, enable interrupts.  Note: Modem status interrupts
	 * are set via set_termios(), which will be occurring imminently
	 * anyway, so we don't enable them here.
	 */
	up->ier = UART_IER_RLSI | UART_IER_RDI;
	serial_outp(up, UART_IER, up->ier);

	if (up->port.flags & UPF_FOURPORT) {
		unsigned int icp;
		/*
		 * Enable interrupts on the AST Fourport board
		 */
		icp = (up->port.iobase & 0xfe0) | 0x01f;
		outb_p(0x80, icp);
		(void) inb_p(icp);
	}

	return 0;
}

static void cpscom_shutdown(struct uart_port *port)
{
	struct uart_8250_port *up =
		container_of(port, struct uart_8250_port, port);
	unsigned long flags;

	/*
	 * Disable interrupts from this port
	 */
	up->ier = 0;
	serial_outp(up, UART_IER, 0);

	spin_lock_irqsave(&up->port.lock, flags);
	if (up->port.flags & UPF_FOURPORT) {
		/* reset interrupts on the AST Fourport board */
		inb((up->port.iobase & 0xfe0) | 0x1f);
		up->port.mctrl |= TIOCM_OUT1;
	} else{
		if( up->port.iotype == UPIO_CPS ){
			up->port.mctrl |= TIOCM_OUT1;
		}
		up->port.mctrl &= ~TIOCM_OUT2;
	}

	cpscom_set_mctrl(&up->port, up->port.mctrl);
	spin_unlock_irqrestore(&up->port.lock, flags);

	/*
	 * Disable break condition and FIFOs
	 */
	serial_out(up, UART_LCR, serial_inp(up, UART_LCR) & ~UART_LCR_SBC);
	cpscom_clear_fifos(up);

#ifdef CONFIG_SERIAL_8250_RSA
	/*
	 * Reset the RSA board back to 115kbps compat mode.
	 */
	disable_rsa(up);
#endif

	/*
	 * Read data port to reset things, and then unlink from
	 * the IRQ chain.
	 */
	(void) serial_in(up, UART_RX);

	del_timer_sync(&up->timer);
	up->timer.function = cpscom_timeout;
	if (is_real_interrupt(up->port.irq))
		serial_unlink_irq_chain(up);
}

static unsigned int cpscom_get_divisor(struct uart_port *port, unsigned int baud)
{
	unsigned int quot;

	/*
	 * Handle magic divisors for baud rates above baud_base on
	 * SMSC SuperIO chips.
	 */
	if ((port->flags & UPF_MAGIC_MULTIPLIER) &&
	    baud == (port->uartclk/4))
		quot = 0x8001;
	else if ((port->flags & UPF_MAGIC_MULTIPLIER) &&
		 baud == (port->uartclk/8))
		quot = 0x8002;
	else
		quot = uart_get_divisor(port, baud);

	return quot;
}

void
cpscom_do_set_termios(struct uart_port *port, struct ktermios *termios,
		          struct ktermios *old)
{
	struct uart_8250_port *up =
		container_of(port, struct uart_8250_port, port);
	unsigned char cval, fcr = 0;
	unsigned long flags;
	unsigned int baud, quot;

	switch (termios->c_cflag & CSIZE) {
	case CS5:
		cval = UART_LCR_WLEN5;
		break;
	case CS6:
		cval = UART_LCR_WLEN6;
		break;
	case CS7:
		cval = UART_LCR_WLEN7;
		break;
	default:
	case CS8:
		cval = UART_LCR_WLEN8;
		break;
	}

	if (termios->c_cflag & CSTOPB)
		cval |= UART_LCR_STOP;
	if (termios->c_cflag & PARENB)
		cval |= UART_LCR_PARITY;
	if (!(termios->c_cflag & PARODD))
		cval |= UART_LCR_EPAR;
#ifdef CMSPAR
	if (termios->c_cflag & CMSPAR)
		cval |= UART_LCR_SPAR;
#endif

	/*
	 * Ask the core to calculate the divisor for us.
	 */
	baud = uart_get_baud_rate(port, termios, old,
				  port->uartclk / 16 / 0xffff,
				  port->uartclk / 16);
	quot = cpscom_get_divisor(port, baud);

	/*
	 * Oxford Semi 952 rev B workaround
	 */
	if (up->bugs & UART_BUG_QUOT && (quot & 0xff) == 0)
		quot++;

	if (up->capabilities & UART_CAP_FIFO && up->port.fifosize > 1) {
		if (baud < 2400)
			fcr = UART_FCR_ENABLE_FIFO | UART_FCR_TRIGGER_1;
		else
			fcr = uart_config[up->port.type].fcr;
	}

	/*
	 * MCR-based auto flow control.  When AFE is enabled, RTS will be
	 * deasserted when the receive FIFO contains more characters than
	 * the trigger, or the MCR RTS bit is cleared.  In the case where
	 * the remote UART is not using CTS auto flow control, we must
	 * have sufficient FIFO entries for the latency of the remote
	 * UART to respond.  IOW, at least 32 bytes of FIFO.
	 */
	if (up->capabilities & UART_CAP_AFE && up->port.fifosize >= 32) {
		up->mcr &= ~UART_MCR_AFE;
		if (termios->c_cflag & CRTSCTS)
			up->mcr |= UART_MCR_AFE;
	}

	/*
	 * Ok, we're now changing the port state.  Do it with
	 * interrupts disabled.
	 */
	spin_lock_irqsave(&up->port.lock, flags);

	/*
	 * Update the per-port timeout.
	 */
	uart_update_timeout(port, termios->c_cflag, baud);

	up->port.read_status_mask = UART_LSR_OE | UART_LSR_THRE | UART_LSR_DR;
	if (termios->c_iflag & INPCK)
		up->port.read_status_mask |= UART_LSR_FE | UART_LSR_PE;
	if (termios->c_iflag & (BRKINT | PARMRK))
		up->port.read_status_mask |= UART_LSR_BI;

	/*
	 * Characteres to ignore
	 */
	up->port.ignore_status_mask = 0;
	if (termios->c_iflag & IGNPAR)
		up->port.ignore_status_mask |= UART_LSR_PE | UART_LSR_FE;
	if (termios->c_iflag & IGNBRK) {
		up->port.ignore_status_mask |= UART_LSR_BI;
		/*
		 * If we're ignoring parity and break indicators,
		 * ignore overruns too (for real raw support).
		 */
		if (termios->c_iflag & IGNPAR)
			up->port.ignore_status_mask |= UART_LSR_OE;
	}

	/*
	 * ignore all characters if CREAD is not set
	 */
	if ((termios->c_cflag & CREAD) == 0)
		up->port.ignore_status_mask |= UART_LSR_DR;

	/*
	 * CTS flow control flag and modem status interrupts
	 */
	up->ier &= ~UART_IER_MSI;
	if (!(up->bugs & UART_BUG_NOMSR) &&
			UART_ENABLE_MS(&up->port, termios->c_cflag))
		up->ier |= UART_IER_MSI;
	if (up->capabilities & UART_CAP_UUE)
		up->ier |= UART_IER_UUE;
	if (up->capabilities & UART_CAP_RTOIE)
		up->ier |= UART_IER_RTOIE;

	serial_out(up, UART_IER, up->ier);

	if (up->capabilities & UART_CAP_EFR) {
		unsigned char efr = 0;
		/*
		 * TI16C752/Startech hardware flow control.  FIXME:
		 * - TI16C752 requires control thresholds to be set.
		 * - UART_MCR_RTS is ineffective if auto-RTS mode is enabled.
		 */
		if (termios->c_cflag & CRTSCTS)
			efr |= UART_EFR_CTS;

		serial_outp(up, UART_LCR, UART_LCR_CONF_MODE_B);
		if (up->port.flags & UPF_EXAR_EFR)
			serial_outp(up, UART_XR_EFR, efr);
		else
			serial_outp(up, UART_EFR, efr);
	}

	if (up->capabilities & UART_NATSEMI) {
		/* Switch to bank 2 not bank 1, to avoid resetting EXCR2 */
		serial_outp(up, UART_LCR, 0xe0);
	} else {
		serial_outp(up, UART_LCR, cval | UART_LCR_DLAB);/* set DLAB */
	}

	serial_dl_write(up, quot);

	/*
	 * LCR DLAB must be set to enable 64-byte FIFO mode. If the FCR
	 * is written without DLAB set, this mode will be disabled.
	 */
	if (up->port.type == PORT_16750)
		serial_outp(up, UART_FCR, fcr);

	serial_outp(up, UART_LCR, cval);		/* reset DLAB */
	up->lcr = cval;					/* Save LCR */
	if (up->port.type != PORT_16750) {
		if (fcr & UART_FCR_ENABLE_FIFO) {
			/* emulated UARTs (Lucent Venus 167x) need two steps */
			serial_outp(up, UART_FCR, UART_FCR_ENABLE_FIFO);
		}
		serial_outp(up, UART_FCR, fcr);		/* set fcr */
	}

	if (up->rs485_flag != 0) {
		unsigned char	fctr, old_lcr;

		old_lcr = serial_inp(up, UART_LCR);
		serial_outp(up, UART_LCR, UART_LCR_CONF_MODE_B);

		fctr = serial_inp(up, UART_FCTR);
		serial_outp(up, UART_FCTR, fctr | UART_FCTR_RS485);

		serial_outp(up, UART_LCR, old_lcr);
	}

	cpscom_set_mctrl(&up->port, up->port.mctrl);
	spin_unlock_irqrestore(&up->port.lock, flags);
	/* Don't rewrite B0 */
	if (tty_termios_baud_rate(termios))
		tty_termios_encode_baud_rate(termios, baud, baud);
}
EXPORT_SYMBOL(cpscom_do_set_termios);

static void
cpscom_set_termios(struct uart_port *port, struct ktermios *termios,
		       struct ktermios *old)
{
	if (port->set_termios)
		port->set_termios(port, termios, old);
	else
		cpscom_do_set_termios(port, termios, old);
}

static void
cpscom_set_ldisc(struct uart_port *port, int new)
{
	if (new == N_PPS) {
		port->flags |= UPF_HARDPPS_CD;
		cpscom_enable_ms(port);
	} else
		port->flags &= ~UPF_HARDPPS_CD;
}


void cpscom_do_pm(struct uart_port *port, unsigned int state,
		      unsigned int oldstate)
{
	struct uart_8250_port *p =
		container_of(port, struct uart_8250_port, port);

	cpscom_set_sleep(p, state != 0);
}
EXPORT_SYMBOL(cpscom_do_pm);

static void
cpscom_pm(struct uart_port *port, unsigned int state,
	      unsigned int oldstate)
{
	if (port->pm)
		port->pm(port, state, oldstate);
	else
		cpscom_do_pm(port, state, oldstate);
}

static unsigned int cpscom_port_size(struct uart_8250_port *pt)
{
	if (pt->port.iotype == UPIO_AU)
		return 0x1000;
	return 8 << pt->port.regshift;
}

/*
 * Resource handling.
 */
static int cpscom_request_std_resource(struct uart_8250_port *up)
{
	unsigned int size = cpscom_port_size(up);
	int ret = 0;

	switch (up->port.iotype) {
	case UPIO_AU:
	case UPIO_TSI:
	case UPIO_MEM32:
	case UPIO_MEM:
	case UPIO_CPS:
		if (!up->port.mapbase)
			break;
		printk(KERN_INFO "mapbase %x size %d\n", up->port.mapbase, size);
		if (!request_mem_region(up->port.mapbase, size, "cpscom_serial")) {
			ret = -EBUSY;
			break;
		}
		printk(KERN_INFO "mapbase %x size %d\n", up->port.mapbase, size);

		if (up->port.flags & UPF_IOREMAP) {
			up->port.membase = ioremap_nocache(up->port.mapbase,
									size);
			if (!up->port.membase) {
				printk(KERN_INFO "Error!\n");
				release_mem_region(up->port.mapbase, size);
				ret = -ENOMEM;
			}
		}
		break;

	case UPIO_HUB6:
	case UPIO_PORT:
		if (!request_region(up->port.iobase, size, "cpscom_serial"))
			ret = -EBUSY;
		break;
	}
	return ret;
}

static void cpscom_release_std_resource(struct uart_8250_port *up)
{
	unsigned int size = cpscom_port_size(up);

	switch (up->port.iotype) {
	case UPIO_AU:
	case UPIO_TSI:
	case UPIO_MEM32:
	case UPIO_MEM:
	case UPIO_CPS:
		if (!up->port.mapbase)
			break;

		if (up->port.flags & UPF_IOREMAP) {
			iounmap(up->port.membase);
			up->port.membase = NULL;
		}

		release_mem_region(up->port.mapbase, size);
		break;

	case UPIO_HUB6:
	case UPIO_PORT:
		release_region(up->port.iobase, size);
		break;
	}
}

static int cpscom_host_init(void)
{

	int ret = 0;

	contec_mcs341_controller_cpsDevicesInit();

	// CEMARKING
	contec_mcs341_controller_setInterrupt(0, 1);
	contec_mcs341_controller_setInterrupt(0, 2);
	contec_mcs341_controller_setInterrupt(0, 3);

	return ret;
}


static int cpscom_ivr_settings(struct uart_8250_port *up)
{
	unsigned char __iomem *IVR;
	resource_size_t	map_ivr;
	int ret = 0;

	switch (up->port.iotype) {
	case UPIO_CPS:
		if (!up->port.mapbase)
			break;

		map_ivr = (resource_size_t)(((unsigned long)up->port.mapbase & ~CONTEC_SERIAL_IVR_MASK) | CONTEC_SERIAL_IVR );
		printk(KERN_INFO "%lx", (unsigned long)map_ivr );
		if (!request_mem_region(map_ivr, 1, "cpscom_serial_ivr")) {
			ret = -EBUSY;
			break;
		}

		IVR = ioremap_nocache(map_ivr, 1 );
		printk(KERN_INFO "1: %lx", (unsigned long) IVR );

		if (!IVR) {
			release_mem_region(map_ivr, 1 );
			ret = -ENOMEM;
			break;
		}

//		cps_fpga_access(CPS_FPGA_ACCESS_BYTE_HIGH);
//		writeb( 0x83, IVR );
//		cps_fpga_access(CPS_FPGA_ACCESS_WORD);

		contec_mcs341_outb((unsigned long)IVR, 0x83);

		iounmap( IVR );
		release_mem_region( map_ivr, 1 );
		break;

	}
	return ret;
}

static int cpscom_request_rsa_resource(struct uart_8250_port *up)
{
	unsigned long start = UART_RSA_BASE << up->port.regshift;
	unsigned int size = 8 << up->port.regshift;
	int ret = -EINVAL;

	switch (up->port.iotype) {
	case UPIO_HUB6:
	case UPIO_PORT:
		start += up->port.iobase;
		if (request_region(start, size, "serial-rsa"))
			ret = 0;
		else
			ret = -EBUSY;
		break;
	}

	return ret;
}

static void cpscom_release_rsa_resource(struct uart_8250_port *up)
{
	unsigned long offset = UART_RSA_BASE << up->port.regshift;
	unsigned int size = 8 << up->port.regshift;

	switch (up->port.iotype) {
	case UPIO_HUB6:
	case UPIO_PORT:
		release_region(up->port.iobase + offset, size);
		break;
	}
}

static void cpscom_release_port(struct uart_port *port)
{
	struct uart_8250_port *up =
		container_of(port, struct uart_8250_port, port);

	cpscom_release_std_resource(up);
	if (up->port.type == PORT_RSA)
		cpscom_release_rsa_resource(up);
}

static int cpscom_request_port(struct uart_port *port)
{
	struct uart_8250_port *up =
		container_of(port, struct uart_8250_port, port);
	int ret = 0;

	ret = cpscom_request_std_resource(up);
	if (ret == 0 && up->port.type == PORT_RSA) {
		ret = cpscom_request_rsa_resource(up);
		if (ret < 0)
			cpscom_release_std_resource(up);
	}

	return ret;
}

static void cpscom_config_port(struct uart_port *port, int flags)
{
	struct uart_8250_port *up =
		container_of(port, struct uart_8250_port, port);
	int probeflags = PROBE_ANY;
	int ret;
	
	/*
	 * Find the region that we can probe for.  This in turn
	 * tells us whether we can probe for the type of port.
	 */
	ret = cpscom_request_std_resource(up);
	if (ret < 0)
		return;

	ret = cpscom_request_rsa_resource(up);
	if (ret < 0)
		probeflags &= ~PROBE_RSA;

	ret = cpscom_ivr_settings(up);
	if( ret < 0){
		printk(KERN_INFO "IVR not setting mode.\n");
		//return;
	}

	if (up->port.iotype != up->cur_iotype)
		set_io_from_upio(port);

	if (flags & UART_CONFIG_TYPE)
		autoconfig(up, probeflags);

	/* if access method is AU, it is a 16550 with a quirk */
	if (up->port.type == PORT_16550A && up->port.iotype == UPIO_AU)
		up->bugs |= UART_BUG_NOMSR;
	if (up->port.type != PORT_UNKNOWN && flags & UART_CONFIG_IRQ)
		autoconfig_irq(up);

	if (up->port.type != PORT_RSA && probeflags & PROBE_RSA)
		cpscom_release_rsa_resource(up);
	if (up->port.type == PORT_UNKNOWN)
		cpscom_release_std_resource(up);
}

static int
cpscom_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	if (ser->irq >= nr_irqs || ser->irq < 0 ||
	    ser->baud_base < 9600 || ser->type < PORT_UNKNOWN ||
	    ser->type >= ARRAY_SIZE(uart_config) || ser->type == PORT_CIRRUS ||
	    ser->type == PORT_STARTECH)
		return -EINVAL;
	return 0;
}

/***
	Ver.1.0.1 Ioctl called AutoRS485 Enable/Disable Function.  (from CPS16550/CPS16550A only )
***/

#define SER_RS485_AUTORTS_ENABLE	(1 << 8)
#define SER_RS485_SET_RS485			(1 << 9)

static int cpscom_ioctl(struct uart_port *port, unsigned int cmd, unsigned long arg)
{
	struct uart_8250_port *up;
	unsigned long flags;
	unsigned int mode;
	unsigned char fctr, old_lcr;


	up = container_of(port, struct uart_8250_port, port);

	switch( cmd ){
	case TIOCSRS485:
		if ( up->port.iotype != UPIO_CPS ) return -ENOTTY;

		if(!access_ok(VERITY_READ, (struct serial_rs485 *)arg, sizeof(struct serial_rs485) ) ){
			return -EFAULT;
		}

		if (copy_from_user(&up->rs485_config, (struct serial_rs485 *) arg, sizeof(struct serial_rs485)))
				return -EFAULT;

		spin_lock_irqsave(&up->port.lock, flags);

		/* Disable interrupts from this port */
		mode = up->ier;
		up->ier = 0;
		serial_out(up, UART_IER, 0);

		old_lcr = serial_inp(up, UART_LCR);

		serial_outp(up, UART_LCR, UART_LCR_CONF_MODE_B);

		fctr = serial_inp(up, UART_FCTR);

		if( up->rs485_config.flags & SER_RS485_AUTORTS_ENABLE )
			serial_outp(up, UART_FCTR, fctr | UART_FCTR_RS485 );
		else
			serial_outp(up, UART_FCTR, fctr & ~UART_FCTR_RS485 );

		if (up->rs485_config.flags & SER_RS485_ENABLED) {
			serial_outp(up, UART_FCTR, fctr | UART_FCTR_RS485);
		}
		if (up->rs485_config.flags & SER_RS485_SET_RS485) {
			if (up->rs485_config.flags & SER_RS485_ENABLED) {
				up->rs485_flag = 1;
			}
			else {
				up->rs485_flag = 0;
			}
		}

		serial_outp(up, UART_LCR, old_lcr);

		/* Enable interrupts */
		up->ier = mode;
		serial_out(up, UART_IER, up->ier);

		spin_unlock_irqrestore(&up->port.lock, flags);

		break;
	case TIOCGRS485:
		if ( up->port.iotype != UPIO_CPS ) return -ENOTTY; 

		if(!access_ok(VERITY_WRITE, (struct serial_rs485 *)arg, sizeof(struct serial_rs485) ) ){
			return -EFAULT;
		}

		spin_lock_irqsave(&up->port.lock, flags);

		/* Disable interrupts from this port */
		mode = up->ier;
		up->ier = 0;
		serial_out(up, UART_IER, 0);

		old_lcr = serial_inp(up, UART_LCR);
		serial_outp(up, UART_LCR, UART_LCR_CONF_MODE_B);

		fctr = serial_inp(up, UART_FCTR);

		if( fctr & UART_FCTR_RS485 )
			up->rs485_config.flags |= SER_RS485_AUTORTS_ENABLE;
		else
			up->rs485_config.flags &= ~SER_RS485_AUTORTS_ENABLE;

			serial_outp(up, UART_LCR, old_lcr);
		/* Enable interrupts */
		up->ier = mode;
		serial_out(up, UART_IER, up->ier);

		spin_unlock_irqrestore(&up->port.lock, flags);

		if (copy_to_user( (struct serial_rs485 *) arg, &up->rs485_config,
			sizeof(struct serial_rs485)))
				return -EFAULT;

		break;
	default:
		return -ENOIOCTLCMD;

	}
	return 0;
}

static const char *
cpscom_type(struct uart_port *port)
{
	int type = port->type;

	if (type >= ARRAY_SIZE(uart_config))
		type = 0;
	return uart_config[type].name;
}

static struct uart_ops cpscom_pops = {
	.tx_empty	= cpscom_tx_empty,
	.set_mctrl	= cpscom_set_mctrl,
	.get_mctrl	= cpscom_get_mctrl,
	.stop_tx	= cpscom_stop_tx,
	.start_tx	= cpscom_start_tx,
	.stop_rx	= cpscom_stop_rx,
	.enable_ms	= cpscom_enable_ms,
	.break_ctl	= cpscom_break_ctl,
	.startup	= cpscom_startup,
	.shutdown	= cpscom_shutdown,
	.set_termios	= cpscom_set_termios,
	.set_ldisc	= cpscom_set_ldisc,
	.pm		= cpscom_pm,
	.type		= cpscom_type,
	.release_port	= cpscom_release_port,
	.request_port	= cpscom_request_port,
	.config_port	= cpscom_config_port,
	.verify_port	= cpscom_verify_port,
	.ioctl = cpscom_ioctl,		// Ver.1.0.1 added
#ifdef CONFIG_CONSOLE_POLL
	.poll_get_char = cpscom_get_poll_char,
	.poll_put_char = cpscom_put_poll_char,
#endif
};

static struct uart_8250_port cpscom_ports[UART_NR];

static void (*cpscom_isa_config)(int port, struct uart_port *up,
	unsigned short *capabilities);

void cpscom_set_isa_configurator(
	void (*v)(int port, struct uart_port *up, unsigned short *capabilities))
{
	cpscom_isa_config = v;
}
EXPORT_SYMBOL(cpscom_set_isa_configurator);

static int cpscom_getchannel_of_device( int devNum ){

	int devId = contec_mcs341_device_productid_get( devNum );

	switch( devId ){
		case CPS_DEVICE_COM2PD:
		case CPS_DEVICE_COM2PC:
			return 2;

		case CPS_DEVICE_COM1PD:
		case CPS_DEVICE_COM1PC:
		case CPS_DEVICE_COM1QL:
		//2018.03.06
		case CPS_DEVICE_COM1LC:
			return 1;	
		default:
			return 0;	
	}
}


static void __init cpscom_init_ports(void)
//static void __init cpscom_init_ports( unsigned int base_num )
{
	struct uart_8250_port *up;
	static int first = 1;
	int i, j, irqflag = 0;

	if (!first)
		return;
	first = 0;

	for (i = 0; i < nr_uarts; i++) {
		struct uart_8250_port *up = &cpscom_ports[i];

		if( !contec_mcs341_device_IsCategory( ( i / 2 ), CPS_CATEGORY_COM ) ){
			up->port.line = -1;
			continue;
		}
		if( (i % 2) >= cpscom_getchannel_of_device( i / 2 ) ){
			up->port.line = -1;
			continue;
		}
		up->port.line = i;
		spin_lock_init(&up->port.lock);

		init_timer(&up->timer);
		up->timer.function = cpscom_timeout;

		/*
		 * ALPHA_KLUDGE_MCR needs to be killed.
		 */
		up->mcr_mask = ~ALPHA_KLUDGE_MCR;
		up->mcr_force = ALPHA_KLUDGE_MCR;

		up->port.ops = &cpscom_pops;

		up->rs485_flag = 0;
	}

	if (share_irqs)
		irqflag = IRQF_SHARED;
	for (i = 0, up = cpscom_ports;
	     i < nr_uarts;
	     i++, up++) {
		if( !contec_mcs341_device_IsCategory( ( i / 2 ), CPS_CATEGORY_COM ) ) continue;
		j = i % 2;
		if( j >= cpscom_getchannel_of_device( i / 2 ) ) continue;


		up->port.iobase   = cpscom_serial_ports[j].port + ( ( i / 2 ) * 0x100 ) ;
		up->port.irq      = irq_canonicalize(cpscom_serial_ports[j].irq);
		up->port.irqflags = cpscom_serial_ports[j].irqflags;
		up->port.uartclk  = cpscom_serial_ports[j].baud_base * 16;
		up->port.flags    = cpscom_serial_ports[j].flags;
		up->port.hub6     = cpscom_serial_ports[j].hub6;
		up->port.membase  = cpscom_serial_ports[j].iomem_base + ( ( i / 2 ) * 0x100) ;
		up->port.iotype   = cpscom_serial_ports[j].io_type;
		up->port.regshift = cpscom_serial_ports[j].iomem_reg_shift;
		if( cpscom_serial_ports[j].mapbase )
			up->port.mapbase		= cpscom_serial_ports[j].mapbase + ( ( i/ 2 ) * 0x100 );

		set_io_from_upio(&up->port);
		up->port.irqflags |= irqflag;
		if (cpscom_isa_config != NULL)
			cpscom_isa_config(i, &up->port, &up->capabilities);
#ifdef DEBUG_SERIAL_CEMARKING_TEST
		cpscom_config_port(&up->port,(UART_CONFIG_TYPE | UART_CONFIG_IRQ) );
#else
		cpscom_config_port(&up->port, UART_CONFIG_TYPE);
#endif
	}
}

static void
cpscom_init_fixed_type_port(struct uart_8250_port *up, unsigned int type)
{
	up->port.type = type;
	up->port.fifosize = uart_config[type].fifo_size;
	up->capabilities = uart_config[type].flags;
	up->tx_loadsz = uart_config[type].tx_loadsz;
}

static void __init
cpscom_register_ports(struct uart_driver *drv, struct device *dev)
{
	int i;

	for (i = 0; i < nr_uarts; i++) {
		struct uart_8250_port *up = &cpscom_ports[i];
		up->cur_iotype = 0xFF;
	}

	cpscom_init_ports();

	for (i = 0; i < nr_uarts; i++) {
		struct uart_8250_port *up = &cpscom_ports[i];

		up->port.dev = dev;
		if( up->port.line != -1 ){
			if (up->port.flags & UPF_FIXED_TYPE)
			cpscom_init_fixed_type_port(up, up->port.type);
			DEBUG_INITCOMPORT(KERN_INFO " uart_line:%d \n",up->port.line);
			uart_add_one_port(drv, &up->port);

			contec_mcs341_create_8250_device_sysfs(up->port.private_data);
		}
	}
}

int cpscom_match_port( struct uart_port *port1, struct uart_port *port2 )
{
	if( port1->iotype != UPIO_CPS)
		return 0;
	else
		return (port1->mapbase == port2->mapbase);
}


#ifdef CONFIG_SERIAL_8250_CONSOLE

static void cpscom_console_putchar(struct uart_port *port, int ch)
{
	struct uart_8250_port *up =
		container_of(port, struct uart_8250_port, port);

	wait_for_xmitr(up, UART_LSR_THRE);
	serial_out(up, UART_TX, ch);
}

/*
 *	Print a string to the serial port trying not to disturb
 *	any possible real use of the port...
 *
 *	The console_lock must be held when we get here.
 */
static void
cpscom_console_write(struct console *co, const char *s, unsigned int count)
{
	struct uart_8250_port *up = &cpscom_ports[co->index];
	unsigned long flags;
	unsigned int ier;
	int locked = 1;

	touch_nmi_watchdog();

	local_irq_save(flags);
	if (up->port.sysrq) {
		/* cpscom_handle_port() already took the lock */
		locked = 0;
	} else if (oops_in_progress) {
		locked = spin_trylock(&up->port.lock);
	} else
		spin_lock(&up->port.lock);

	/*
	 *	First save the IER then disable the interrupts
	 */
	ier = serial_in(up, UART_IER);

	if (up->capabilities & UART_CAP_UUE)
		serial_out(up, UART_IER, UART_IER_UUE);
	else
		serial_out(up, UART_IER, 0);

	uart_console_write(&up->port, s, count, cpscom_console_putchar);

	/*
	 *	Finally, wait for transmitter to become empty
	 *	and restore the IER
	 */
	wait_for_xmitr(up, BOTH_EMPTY);
	serial_out(up, UART_IER, ier);

	/*
	 *	The receive handling will happen properly because the
	 *	receive ready bit will still be set; it is not cleared
	 *	on read.  However, modem control will not, we must
	 *	call it if we have saved something in the saved flags
	 *	while processing with interrupts off.
	 */
	if (up->msr_saved_flags)
		check_modem_status(up);

	if (locked)
		spin_unlock(&up->port.lock);
	local_irq_restore(flags);
}

static int __init cpscom_console_setup(struct console *co, char *options)
{
	struct uart_port *port;
	int baud = 115200;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	/*
	 * Check whether an invalid uart number has been specified, and
	 * if so, search for the first available port that does have
	 * console support.
	 */
	if (co->index >= nr_uarts)
		co->index = 0;
	port = &cpscom_ports[co->index].port;
	if (!port->iobase && !port->membase)
		return -ENODEV;

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);

	return uart_set_options(port, co, baud, parity, bits, flow);
}

static int cpscom_console_early_setup(void)
{
	return cpscom_find_port_for_earlycon();
}

static struct console cpscom_console = {
	.name		= "ttyCPS",
	.write		= cpscom_console_write,
	.device		= uart_console_device,
	.setup		= cpscom_console_setup,
	.early_setup	= cpscom_console_early_setup,
	.flags		= CON_PRINTBUFFER | CON_ANYTIME,
	.index		= -1,
	.data		= &cpscom_reg,
};

static int __init cpscom_console_init(void)
{
	if (nr_uarts > UART_NR)
		nr_uarts = UART_NR;

	cpscom_init_ports();
	register_console(&cpscom_console);
	return 0;
}
console_initcall(cpscom_console_init);

int cpscom_find_port(struct uart_port *p)
{
	int line;
	struct uart_port *port;

	for (line = 0; line < nr_uarts; line++) {
		port = &cpscom_ports[line].port;
		if( p->iotype != UPIO_CPS){
			if (uart_match_port(p, port))
				return line;
		}else{
			if( cpscom_match_port(p, port))
				return line;
		}
	}
	return -ENODEV;
}

#define CPSCOM_CONSOLE	&cpscom_console
#else
#define CPSCOM_CONSOLE	NULL
#endif

static struct uart_driver cpscom_reg = {
	.owner			= THIS_MODULE,
	.driver_name		= "cpscom-serial",
	.dev_name		= "ttyCPS",
	.major			= CPSCOM_MAJOR,
	.minor			= 0,
	.cons			= CPSCOM_CONSOLE,
};

static struct uart_driver cpscom_reg_lora = {
	.owner			= THIS_MODULE,
	.driver_name		= "cpscom-serial",
	.dev_name		= "ttyCPSLORA",
	.major			= CPSCOM_MAJOR,
	.minor			= 0,
	.cons			= CPSCOM_CONSOLE,
};

/*
 * early_cpscom_setup - early registration for 8250 ports
 *
 * Setup an 8250 port structure prior to console initialisation.  Use
 * after console initialisation will cause undefined behaviour.
 */
int __init early_cpscom_setup(struct uart_port *port)
{
	struct uart_port *p;

	if (port->line >= ARRAY_SIZE(cpscom_ports))
		return -ENODEV;

	cpscom_init_ports();
	p = &cpscom_ports[port->line].port;
	p->iobase       = port->iobase;
	p->membase      = port->membase;
	p->irq          = port->irq;
	p->irqflags     = port->irqflags;
	p->uartclk      = port->uartclk;
	p->fifosize     = port->fifosize;
	p->regshift     = port->regshift;
	p->iotype       = port->iotype;
	p->flags        = port->flags;
	p->mapbase      = port->mapbase;
	p->private_data = port->private_data;
	p->type		= port->type;
	p->line		= port->line;

	set_io_from_upio(p);
	if (port->serial_in)
		p->serial_in = port->serial_in;
	if (port->serial_out)
		p->serial_out = port->serial_out;
	if (port->handle_irq)
		p->handle_irq = port->handle_irq;
	else
		p->handle_irq = cpscom_default_handle_irq;

	return 0;
}

/**
 *	cpscom_suspend_port - suspend one serial port
 *	@line:  serial line number
 *
 *	Suspend one serial port.
 */
void cpscom_suspend_port(int line)
{
	uart_suspend_port(&cpscom_reg, &cpscom_ports[line].port);
}

/**
 *	cpscom_resume_port - resume one serial port
 *	@line:  serial line number
 *
 *	Resume one serial port.
 */
void cpscom_resume_port(int line)
{
	struct uart_8250_port *up = &cpscom_ports[line];

	if (up->capabilities & UART_NATSEMI) {
		/* Ensure it's still in high speed mode */
		serial_outp(up, UART_LCR, 0xE0);

		ns16550a_goto_highspeed(up);

		serial_outp(up, UART_LCR, 0);
		up->port.uartclk = 921600*16;
	}
	uart_resume_port(&cpscom_reg, &up->port);
}

/*
 * Register a set of serial devices attached to a platform device.  The
 * list is terminated with a zero flags entry, which means we expect
 * all entries to have at least UPF_BOOT_AUTOCONF set.
 */
static int __devinit cpscom_probe(struct platform_device *dev)
{
	struct plat_serial8250_port *p = dev->dev.platform_data;
	struct uart_port port;
	int ret, i, irqflag = 0;

	memset(&port, 0, sizeof(struct uart_port));

	if (share_irqs)
		irqflag = IRQF_SHARED;

	for (i = 0; p && p->flags != 0; p++, i++) {
		port.iobase		= p->iobase;
		port.membase		= p->membase;
		port.irq		= p->irq;
		port.irqflags		= p->irqflags;
		port.uartclk		= p->uartclk;
		port.regshift		= p->regshift;
		port.iotype		= p->iotype;
		port.flags		= p->flags;
		port.mapbase		= p->mapbase;
		port.hub6		= p->hub6;
		port.private_data	= p->private_data;
		port.type		= p->type;
		port.serial_in		= p->serial_in;
		port.serial_out		= p->serial_out;
		port.handle_irq		= p->handle_irq;
		port.set_termios	= p->set_termios;
		port.pm			= p->pm;
		port.dev		= &dev->dev;
		port.irqflags		|= irqflag;
		ret = cpscom_register_port(&port);
		if (ret < 0) {
			dev_err(&dev->dev, "unable to register port at index %d "
				"(IO%lx MEM%llx IRQ%d): %d\n", i,
				p->iobase, (unsigned long long)p->mapbase,
				p->irq, ret);
		}
	}
	return 0;
}

/*
 * Remove serial ports registered against a platform device.
 */
static int __devexit cpscom_remove(struct platform_device *dev)
{
	int i;

	for (i = 0; i < nr_uarts; i++) {
		struct uart_8250_port *up = &cpscom_ports[i];

//		if (up->port.dev == &dev->dev)
		if (up->port.line != -1 ){
			contec_mcs341_remove_8250_device_sysfs(up->port.private_data);
			cpscom_unregister_port(i);
			cpscom_release_port(&up->port);
		}
	}
	return 0;
}

static int cpscom_suspend(struct platform_device *dev, pm_message_t state)
{
	int i;

	for (i = 0; i < UART_NR; i++) {
		struct uart_8250_port *up = &cpscom_ports[i];

		if (up->port.type != PORT_UNKNOWN && up->port.dev == &dev->dev)
			uart_suspend_port(&cpscom_reg, &up->port);
	}

	return 0;
}

static int cpscom_resume(struct platform_device *dev)
{
	int i;

	for (i = 0; i < UART_NR; i++) {
		struct uart_8250_port *up = &cpscom_ports[i];

		if (up->port.type != PORT_UNKNOWN && up->port.dev == &dev->dev)
			cpscom_resume_port(i);
	}

	return 0;
}

static struct platform_driver cpscom_isa_driver = {
	.probe		= cpscom_probe,
	.remove		= __devexit_p(cpscom_remove),
	.suspend	= cpscom_suspend,
	.resume		= cpscom_resume,
	.driver		= {
		.name	= "cpscom-serial",
		.owner	= THIS_MODULE,
	},
};

/*
 * cpscom_register_port and cpscom_unregister_port allows for
 * 16x50 serial ports to be configured at run-time, to support PCMCIA
 * modems and PCI multiport cards.
 */
static DEFINE_MUTEX(serial_mutex);

static struct uart_8250_port *cpscom_find_match_or_unused(struct uart_port *port)
{
	int i;

	/*
	 * First, find a port entry which matches.
	 */
	for (i = 0; i < nr_uarts; i++){
		if( cpscom_ports[i].port.iotype != UPIO_CPS ){
			if (uart_match_port(&cpscom_ports[i].port, port))
				return &cpscom_ports[i];
		}else{
			if (cpscom_match_port(&cpscom_ports[i].port, port))
				return &cpscom_ports[i];
		}
	}
	/*
	 * We didn't find a matching entry, so look for the first
	 * free entry.  We look for one which hasn't been previously
	 * used (indicated by zero iobase).
	 */
	for (i = 0; i < nr_uarts; i++)
		if (cpscom_ports[i].port.type == PORT_UNKNOWN &&
		    cpscom_ports[i].port.iobase == 0)
			return &cpscom_ports[i];

	/*
	 * That also failed.  Last resort is to find any entry which
	 * doesn't have a real port associated with it.
	 */
	for (i = 0; i < nr_uarts; i++)
		if (cpscom_ports[i].port.type == PORT_UNKNOWN)
			return &cpscom_ports[i];

	return NULL;
}

/**
 *	cpscom_register_port - register a serial port
 *	@port: serial port template
 *
 *	Configure the serial port specified by the request. If the
 *	port exists and is in use, it is hung up and unregistered
 *	first.
 *
 *	The port is then probed and if necessary the IRQ is autodetected
 *	If this fails an error is returned.
 *
 *	On success the port is ready to use and the line number is returned.
 */
int cpscom_register_port(struct uart_port *port)
{
	struct uart_8250_port *uart;
	int ret = -ENOSPC;

	if (port->uartclk == 0)
		return -EINVAL;

	mutex_lock(&serial_mutex);

	uart = cpscom_find_match_or_unused(port);
	if (uart) {
		uart_remove_one_port(&cpscom_reg, &uart->port);

		uart->port.iobase       = port->iobase;
		uart->port.membase      = port->membase;
		uart->port.irq          = port->irq;
		uart->port.irqflags     = port->irqflags;
		uart->port.uartclk      = port->uartclk;
		uart->port.fifosize     = port->fifosize;
		uart->port.regshift     = port->regshift;
		uart->port.iotype       = port->iotype;
		uart->port.flags        = port->flags | UPF_BOOT_AUTOCONF;
		uart->port.mapbase      = port->mapbase;
		uart->port.private_data = port->private_data;
		if (port->dev)
			uart->port.dev = port->dev;

		if (port->flags & UPF_FIXED_TYPE)
			cpscom_init_fixed_type_port(uart, port->type);

		set_io_from_upio(&uart->port);
		/* Possibly override default I/O functions.  */
		if (port->serial_in)
			uart->port.serial_in = port->serial_in;
		if (port->serial_out)
			uart->port.serial_out = port->serial_out;
		if (port->handle_irq)
			uart->port.handle_irq = port->handle_irq;
		/*  Possibly override set_termios call */
		if (port->set_termios)
			uart->port.set_termios = port->set_termios;
		if (port->pm)
			uart->port.pm = port->pm;

		if (cpscom_isa_config != NULL)
			cpscom_isa_config(0, &uart->port,
					&uart->capabilities);
		ret = uart_add_one_port(&cpscom_reg, &uart->port);
		if (ret == 0)
			ret = uart->port.line;
	}
	mutex_unlock(&serial_mutex);

	return ret;
}
EXPORT_SYMBOL(cpscom_register_port);

/**
 *	cpscom_unregister_port - remove a 16x50 serial port at runtime
 *	@line: serial line number
 *
 *	Remove one serial port.  This may not be called from interrupt
 *	context.  We hand the port back to the our control.
 */
void cpscom_unregister_port(int line)
{
	struct uart_8250_port *uart;

	if( line == -1 ) return;

	uart = &cpscom_ports[line];

	mutex_lock(&serial_mutex);
	uart_remove_one_port(&cpscom_reg, &uart->port);

	uart->port.flags &= ~UPF_BOOT_AUTOCONF;
	uart->port.type = PORT_UNKNOWN;
	uart->capabilities = uart_config[uart->port.type].flags;
	//2017.10.20
	// uart_add_one_port(&cpscom_reg, &uart->port);

	mutex_unlock(&serial_mutex);
}
EXPORT_SYMBOL(cpscom_unregister_port);

 /**
	 @~Japanese
	 @brief MCS341 contec_mcs341_power_show間数
	 @param *dev : device 構造体
	 @param *attr : device_attribute 構造体
	 @param buf : buffer
	 @return subunit_power_state : 0 または　1
	 @detail loraモジュールの状態を表示する
 **/
static int contec_mcs341_power_show(struct device *dev, struct device_attribute *attr,char *buf )
{
	struct uart_port * uport = dev_get_drvdata(dev);

	unsigned int devnum = 
		contec_mcs341_device_deviceNum_get( (unsigned long) uport->mapbase) - 1;

	return sprintf(buf,"%d", subunit_power_state[devnum]);
}

/**
	@~Japanese
	 @brief MCS341 contec_mcs341_power_store間数
	 @param *dev : device 構造体
	 @param *attr : device_attribute 構造体
	 @param buf : buffer
	 @param count : count
	 @return buf : Failed :-1  Success : more than 0
	 @detail loraモジュールをON/OFFする
**/
static int contec_mcs341_power_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count )
{
	unsigned short valb1=0x5003;
	unsigned short valb2=0;
	unsigned short valb3=0;

	int wait_ontime = 0; // wait on time for from power to reset (msec)
	int wait_offtime = 0; // wait off time for from reset to power (msec)

	struct uart_port * uport = dev_get_drvdata(dev);

	//2018.05.11
	int device_ID;
	
	unsigned int devnum = 
		contec_mcs341_device_deviceNum_get( (unsigned long) uport->mapbase) - 1;

	unsigned int addr1 = 0x30;
	unsigned int addr2 = 0x34;


	device_ID = contec_mcs341_device_productid_get( devnum );

	//2018.05.11
	switch ( device_ID ){
		case CPS_DEVICE_COM1QL: valb2=0x0010; valb3=0x0001; wait_ontime= 100; wait_offtime = 100; break;
		case CPS_DEVICE_COM1LC: valb2=0x0010; break;
					break;
		default : return -1;
	}

	switch( buf[0] ){
		case '0':
			subunit_power_state[devnum]=0;
			contec_mcs341_device_common_outw(devnum, addr1, valb1);

			if( valb3 > 0 ){
				contec_mcs341_device_common_outw(devnum, addr2, valb2);
				contec_cps_micro_delay_sleep(wait_offtime * USEC_PER_MSEC, 0);
			}

			if( valb2 > 0 ){
				contec_mcs341_device_common_outw(devnum, addr2, 0 );
			}
		
			subunit_powerreset[devnum] = 0;
			break;
		case '1':
			subunit_power_state[devnum]=1;
			contec_mcs341_device_common_outw(devnum, addr1, valb1);
			if( valb2 > 0 ){
				contec_mcs341_device_common_outw(devnum, addr2, valb2);
				contec_cps_micro_delay_sleep(wait_ontime * USEC_PER_MSEC, 0);			
			}
			if( valb3 > 0 )
				contec_mcs341_device_common_outw(devnum, addr2, (valb2 | valb3) );
			
			subunit_powerreset[devnum] = (valb2 | valb3);		
		break;
	}
	return strlen(buf);

}
static DEVICE_ATTR(dev_power , S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
	contec_mcs341_power_show, contec_mcs341_power_store );

 /**
	 @~Japanese
	 @brief MCS341 contec_mcs341_subunit_manual_power_show間数
	 @param *dev : device 構造体
	 @param *attr : device_attribute 構造体
	 @param buf : buffer
	 @return subunit_power_state : 0 または　1
	 @detail loraモジュールの状態を表示する
 **/
static int contec_mcs341_subunit_manual_power_show(struct device *dev, struct device_attribute *attr,char *buf )
{
	struct uart_port * uport = dev_get_drvdata(dev);

	unsigned int devnum = 
		contec_mcs341_device_deviceNum_get( (unsigned long) uport->mapbase) - 1;

	return sprintf(buf,"%d", (subunit_powerreset[devnum]  & 0x01) );
}

/**
	@~Japanese
	 @brief MCS341 contec_mcs341_subunit_manual_power_store間数
	 @param *dev : device 構造体
	 @param *attr : device_attribute 構造体
	 @param buf : buffer
	 @param count : count
	 @return buf : Failed :-1  Success : more than 0
	 @detail loraモジュールをON/OFFする
**/
static int contec_mcs341_subunit_manual_power_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count )
{
	unsigned short valb1=0x5003;

	struct uart_port * uport = dev_get_drvdata(dev);

	unsigned int devnum = 
		contec_mcs341_device_deviceNum_get( (unsigned long) uport->mapbase) - 1;

	unsigned int addr1 = 0x30;
	unsigned int addr2 = 0x34;		

	switch( buf[0] ){
		case '0':
		subunit_powerreset[devnum] = (subunit_powerreset[devnum] & ~0x01);
		contec_mcs341_device_common_outw(devnum, addr1, valb1);
		contec_mcs341_device_common_outw(devnum, addr2, subunit_powerreset[devnum]);
		break;
		case '1':
		subunit_powerreset[devnum] = ( subunit_powerreset[devnum] | 0x01 );
		contec_mcs341_device_common_outw(devnum, addr1, valb1);
		contec_mcs341_device_common_outw(devnum, addr2, subunit_powerreset[devnum]);		
		break;
	}
	return strlen(buf);

}
static DEVICE_ATTR(dev_subunit_manual_power , S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP,
	contec_mcs341_subunit_manual_power_show, contec_mcs341_subunit_manual_power_store );

 /**
	 @~Japanese
	 @brief MCS341 contec_mcs341_subunit_manual_reset_show間数
	 @param *dev : device 構造体
	 @param *attr : device_attribute 構造体
	 @param buf : buffer
	 @return subunit_power_state : 0 または　1
	 @detail loraモジュールの状態を表示する
 **/
static int contec_mcs341_subunit_manual_reset_show(struct device *dev, struct device_attribute *attr,char *buf )
{
	struct uart_port * uport = dev_get_drvdata(dev);

	unsigned int devnum = 
		contec_mcs341_device_deviceNum_get( (unsigned long) uport->mapbase) - 1;

	return sprintf(buf,"%d", ((subunit_powerreset[devnum]  & 0x10) >> 4) );
}

/**
	@~Japanese
	 @brief MCS341 contec_mcs341_subunit_manual_reset_store間数
	 @param *dev : device 構造体
	 @param *attr : device_attribute 構造体
	 @param buf : buffer
	 @param count : count
	 @return buf : Failed :-1  Success : more than 0
	 @detail loraモジュールをON/OFFする
**/
static int contec_mcs341_subunit_manual_reset_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count )
{
	unsigned short valb1=0x5003;

	struct uart_port * uport = dev_get_drvdata(dev);

	unsigned int devnum = 
		contec_mcs341_device_deviceNum_get( (unsigned long) uport->mapbase) - 1;

	unsigned int addr1 = 0x30;
	unsigned int addr2 = 0x34;		

	switch( buf[0] ){
		case '0':
		subunit_powerreset[devnum] = (subunit_powerreset[devnum] & ~0x10);
		contec_mcs341_device_common_outw(devnum, addr1, valb1);
		contec_mcs341_device_common_outw(devnum, addr2, subunit_powerreset[devnum]);
		break;
		case '1':
		subunit_powerreset[devnum] = ( subunit_powerreset[devnum] | 0x10 );
		contec_mcs341_device_common_outw(devnum, addr1, valb1);
		contec_mcs341_device_common_outw(devnum, addr2, subunit_powerreset[devnum]);		
		break;
	}
	return strlen(buf);

}
static DEVICE_ATTR(dev_subunit_manual_reset , S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP ,
	contec_mcs341_subunit_manual_reset_show, contec_mcs341_subunit_manual_reset_store );

 /**
	 @~Japanese
	 @brief MCS341 contec_mcs341_interrupt_show間数
	 @param *dev : device 構造体
	 @param *attr : device_attribute 構造体
	 @param buf : buffer
	 @return subunit_power_state : 0 または　1
 	 @detail loraモジュールの割り込み状態を表示する(作成中)
 **/
 static int contec_mcs341_interrupt_show(struct device *dev, struct device_attribute *attr,char *buf )
 {
	struct uart_port * uport = dev_get_drvdata(dev);

	unsigned int devnum = 
		contec_mcs341_device_deviceNum_get( (unsigned long) uport->mapbase) - 1;

	return sprintf(buf,"%d", subunit_interrupt[devnum]);
 }
 
 /**
	 @~Japanese
	 @brief MCS341 contec_mcs341_interrupt_store間数
	 @param *dev : device 構造体
	 @param *attr : device_attribute 構造体
	 @param buf : buffer
	 @param count : count
	 @return buf : 0 または　1
 	 @detail loraモジュールへ割り込みを行う(作成中)
 **/
 static int contec_mcs341_interrupt_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count )
 {

	struct uart_port * uport = dev_get_drvdata(dev);

	unsigned int devnum = 
		contec_mcs341_device_deviceNum_get( (unsigned long) uport->mapbase) - 1;

	switch( buf[0] ){
		case '0':
		subunit_interrupt[devnum]=0;
		break;
		case '1':
		subunit_interrupt[devnum]=1;
		break;
	}
	return strlen(buf);
 }
 static DEVICE_ATTR(interrupt , S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
	 contec_mcs341_interrupt_show, contec_mcs341_interrupt_store );
 
 /**
	 @~Japanese
	 @brief MCS341 contec_mcs341_deviceID_show間数
	 @param *dev : device 構造体
	 @param *attr : device_attribute 構造体
	 @param buf : buffer
	 @return buf : 0x19 または 0x03　
 	 @detail COMモジュールのdeviceIDを取得・表示する
 **/
 static int contec_mcs341_deviceID_show(struct device *dev, struct device_attribute *attr,char *buf )
 {
	struct uart_port * uport = dev_get_drvdata(dev);
	//2018.05.11
	int device_ID;
	
	unsigned int devnum = 
		contec_mcs341_device_deviceNum_get( (unsigned long) uport->mapbase) - 1;

	device_ID = contec_mcs341_device_productid_get( devnum );
	
	return sprintf(buf,"%x", device_ID);
 }
 static DEVICE_ATTR(id , S_IRUSR | S_IRGRP | S_IROTH ,contec_mcs341_deviceID_show, NULL );

 /**
	 @~Japanese
	 @brief MCS341 contec_mcs341_led0_status_show間数
	 @param *dev : device 構造体
	 @param *attr : device_attribute 構造体
	 @param buf : buffer
	 @return subunit_power_state : 0 または　1
 	 @detail loraモジュールのled0の状態を表示する
 **/
static int contec_mcs341_led0_status_show(struct device *dev, struct device_attribute *attr,char *buf )
{
	static u16 led0_status_show;
	struct uart_port * uport = dev_get_drvdata(dev);
	unsigned int devnum = 
		contec_mcs341_device_deviceNum_get( (unsigned long) uport->mapbase) - 1;

	led0_status_show = subunit_leds_status[devnum]; 
	led0_status_show &= 0x03; 
	return sprintf(buf,"%d", led0_status_show);
}

/**
	@~Japanese
	 @brief MCS341 contec_mcs341_led0_status_store間数
	 @param *dev : device 構造体
	 @param *attr : device_attribute 構造体
	 @param buf : buffer
	 @param count : count
	 @return buf : 0 または　1
 	 @detail loraモジュールのled0を点灯/消灯する
**/
static int contec_mcs341_led0_status_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count )
{
	unsigned short valb1=0x5004;

	struct uart_port * uport = dev_get_drvdata(dev);
	
	unsigned int devnum = 
		contec_mcs341_device_deviceNum_get( (unsigned long) uport->mapbase) - 1;

	unsigned int addr1 = 0x30;
	unsigned int addr2 = 0x34;

	u8 led_status = ((u8)buf[0] - 0x30 ) & 0x03;

	subunit_leds_status[devnum] = (subunit_leds_status[devnum] & ~0x03) | led_status;
	
	contec_mcs341_device_common_outw(devnum, addr1, valb1);
	contec_mcs341_device_common_outw(devnum, addr2, subunit_leds_status[devnum]);

	return strlen(buf);

}
static DEVICE_ATTR(led0_restore , S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
	contec_mcs341_led0_status_show, contec_mcs341_led0_status_store );

 /**
	 @~Japanese
	 @brief MCS341 contec_mcs341_leds1_status_show間数
	 @param *dev : device 構造体
	 @param *attr : device_attribute 構造体
	 @param buf : buffer
	 @return subunit_power_state : 0 または　1
 	 @detail loraモジュールのled1の状態を表示する
 **/
static int contec_mcs341_led1_status_show(struct device *dev, struct device_attribute *attr,char *buf )
{
	static u16 led1_status_show;
	struct uart_port * uport = dev_get_drvdata(dev);
	unsigned int devnum = 
		contec_mcs341_device_deviceNum_get( (unsigned long) uport->mapbase) - 1;

	led1_status_show = subunit_leds_status[devnum] >> 2; 
	led1_status_show &= 0x03; 
	return sprintf(buf,"%d", led1_status_show);
}

/**
	@~Japanese
	 @brief MCS341 contec_mcs341_led1_status_store間数
	 @param *dev : device 構造体
	 @param *attr : device_attribute 構造体
	 @param buf : buffer
	 @param count : count
	 @return buf : 0 または　1
  	 @detail loraモジュールのled1を点灯/消灯する
**/
static int contec_mcs341_led1_status_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count )
{
	unsigned short valb1=0x5004;

	struct uart_port * uport = dev_get_drvdata(dev);
	
	unsigned int devnum = 
		contec_mcs341_device_deviceNum_get( (unsigned long) uport->mapbase) - 1;

	unsigned int addr1 = 0x30;
	unsigned int addr2 = 0x34;

	u8 led_status = ((u8)buf[0] - 0x30 ) & 0x03;
	
	subunit_leds_status[devnum] = ((subunit_leds_status[devnum] & ~(0x03 << 2)) | led_status << 2);
	
	contec_mcs341_device_common_outw(devnum, addr1, valb1);
	contec_mcs341_device_common_outw(devnum, addr2, subunit_leds_status[devnum]);
	
	return strlen(buf);

}
static DEVICE_ATTR(led1_restore , S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
	contec_mcs341_led1_status_show, contec_mcs341_led1_status_store );

 /**
	 @~Japanese
	 @brief MCS341 contec_mcs341_led2_status_show間数
	 @param *dev : device 構造体
	 @param *attr : device_attribute 構造体
	 @param buf : buffer
	 @return subunit_power_state : 0 または　1
  	 @detail loraモジュールのled2の状態を表示する
 **/
static int contec_mcs341_led2_status_show(struct device *dev, struct device_attribute *attr,char *buf )
{
	static u16 led2_status_show;
	struct uart_port * uport = dev_get_drvdata(dev);
	unsigned int devnum = 
		contec_mcs341_device_deviceNum_get( (unsigned long) uport->mapbase) - 1;

	led2_status_show = subunit_leds_status[devnum] >> 4; 
	led2_status_show &= 0x03; 
	return sprintf(buf,"%d", led2_status_show);
}

/**
	@~Japanese
	 @brief MCS341 contec_mcs341_led2_status_store間数
	 @param *dev : device 構造体
	 @param *attr : device_attribute 構造体
	 @param buf : buffer
	 @param count : count
	 @return buf : 0 または　1
  	 @detail loraモジュールのled2を点灯/消灯する
**/
static int contec_mcs341_led2_status_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count )
{
	unsigned short valb1=0x5004;

	struct uart_port * uport = dev_get_drvdata(dev);
	
	unsigned int devnum = 
		contec_mcs341_device_deviceNum_get( (unsigned long) uport->mapbase) - 1;

	unsigned int addr1 = 0x30;
	unsigned int addr2 = 0x34;

	u8 led_status = ((u8)buf[0] - 0x30 ) & 0x03;
	
	subunit_leds_status[devnum] = ((subunit_leds_status[devnum] & ~(0x03 << 4)) | led_status << 4);
	
	contec_mcs341_device_common_outw(devnum, addr1, valb1);
	contec_mcs341_device_common_outw(devnum, addr2, subunit_leds_status[devnum]);
	
	return strlen(buf);

}
static DEVICE_ATTR(led2_restore , S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
	contec_mcs341_led2_status_show, contec_mcs341_led2_status_store );

 /**
	 @~Japanese
	 @brief MCS341 contec_mcs341_leds3_status_show間数
	 @param *dev : device 構造体
	 @param *attr : device_attribute 構造体
	 @param buf : buffer
	 @return subunit_power_state : 0 または　1
  	 @detail loraモジュールのled3の状態を表示する
 **/
static int contec_mcs341_led3_status_show(struct device *dev, struct device_attribute *attr,char *buf )
{
	static u16 led3_status_show;
	struct uart_port * uport = dev_get_drvdata(dev);
	unsigned int devnum = 
		contec_mcs341_device_deviceNum_get( (unsigned long) uport->mapbase) - 1;

	led3_status_show = subunit_leds_status[devnum] >> 6; 
	led3_status_show &= 0x03; 
	return sprintf(buf,"%d", led3_status_show);
}

/**
	@~Japanese
	 @brief MCS341 contec_mcs341_led3_status_store間数
	 @param *dev : device 構造体
	 @param *attr : device_attribute 構造体
	 @param buf : buffer
	 @param count : count
	 @return buf : 0 または　1
   	 @detail loraモジュールのled3を点灯/消灯する
**/
static int contec_mcs341_led3_status_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count )
{
	unsigned short valb1=0x5004;

	struct uart_port * uport = dev_get_drvdata(dev);
	
	unsigned int devnum = 
		contec_mcs341_device_deviceNum_get( (unsigned long) uport->mapbase) - 1;

	unsigned int addr1 = 0x30;
	unsigned int addr2 = 0x34;

	u8 led_status = ((u8)buf[0] - 0x30 ) & 0x03;
	
	subunit_leds_status[devnum] = ((subunit_leds_status[devnum] & ~(0x03 << 6)) | led_status << 6);
	
	contec_mcs341_device_common_outw(devnum, addr1, valb1);
	contec_mcs341_device_common_outw(devnum, addr2, subunit_leds_status[devnum]);

	return strlen(buf);

}
static DEVICE_ATTR(led3_restore , S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
	contec_mcs341_led3_status_show, contec_mcs341_led3_status_store );

 /**
	 @~Japanese
	 @brief MCS341 contec_mcs341_Revision_show間数
	 @param *dev : device 構造体
	 @param *attr : device_attribute 構造体
	 @param buf : buffer
	 @return buf　
 	 @detail comモジュールのRevisionを取得・表示する
 **/
 static int contec_mcs341_Revision_show(struct device *dev, struct device_attribute *attr,char *buf )
 {
	struct uart_port * uport = dev_get_drvdata(dev);
	int Rev = 0;
	
	unsigned int devnum = 
		contec_mcs341_device_deviceNum_get( (unsigned long) uport->mapbase) - 1;

	Rev = contec_mcs341_device_fpga_version_get( devnum );
	
	return sprintf(buf,"%x", Rev);
 }
 static DEVICE_ATTR(revision , S_IRUSR | S_IRGRP | S_IROTH ,contec_mcs341_Revision_show, NULL );

 /**
	 @~Japanese
	 @brief MCS341 contec_mcs341_BoardVersion_show間数
	 @param *dev : device 構造体
	 @param *attr : device_attribute 構造体
	 @param buf : buffer
	 @return buf 　
 	 @detail comモジュールのBoardVersionを取得・表示する
 **/
 static int contec_mcs341_BoardVersion_show(struct device *dev, struct device_attribute *attr,char *buf )
 {
	struct uart_port * uport = dev_get_drvdata(dev);
	int Board_ver = 0;
	
	unsigned int devnum = 
		contec_mcs341_device_deviceNum_get( (unsigned long) uport->mapbase) - 1;

	Board_ver = contec_mcs341_device_board_version_get( devnum );
	
	return sprintf(buf,"%x", Board_ver);
 }
 static DEVICE_ATTR(board_ver , S_IRUSR | S_IRGRP | S_IROTH ,contec_mcs341_BoardVersion_show, NULL );



/**
	@~Japanese
	@brief MCS341　contec_mcs341_create_8250_device_sysfs関数
	@param *dev : device 構造体
	@return err : 成功:0 ,失敗：0以外
	@detail comモジュール、loraモジュールのデバイスファイルを作成する
**/
 static int contec_mcs341_create_8250_device_sysfs(struct device *devp){
 
	int err = 0;
    //2018.05.11
	int device_ID = 0;
	struct uart_port * uport = dev_get_drvdata(devp);
	unsigned int devnum = 
	contec_mcs341_device_deviceNum_get( (unsigned long) uport->mapbase) - 1;

	device_ID = contec_mcs341_device_productid_get( devnum );

	switch(device_ID){
		case CPS_DEVICE_COM1QL :
			err = device_create_file(devp, &dev_attr_dev_power);
			err |= device_create_file(devp, &dev_attr_interrupt);
			err |= device_create_file(devp, &dev_attr_led0_restore);
			err |= device_create_file(devp, &dev_attr_led1_restore);
			err |= device_create_file(devp, &dev_attr_led2_restore);
			err |= device_create_file(devp, &dev_attr_led3_restore);
#if 1
			err |= device_create_file(devp, &dev_attr_dev_subunit_manual_power);
			err |= device_create_file(devp, &dev_attr_dev_subunit_manual_reset);			
#endif
		break;
		//2018.05.11
		case CPS_DEVICE_COM1LC :
			err = device_create_file(devp, &dev_attr_dev_power);
#if 1
			err |= device_create_file(devp, &dev_attr_dev_subunit_manual_power);
#endif
		break;
	}
	err |= device_create_file(devp,&dev_attr_id);
	err |= device_create_file(devp,&dev_attr_revision);
	err |= device_create_file(devp,&dev_attr_board_ver);

	return err;
}
	
/**
	@~Japanese
	@brief MCS341　contec_mcs341_remove_8250_device_sysfs関数
	@param *devp : driver 構造体
	@detail comモジュール、loraモジュールのデバイスファイルを削除する
**/
static void contec_mcs341_remove_8250_device_sysfs(struct device *devp)
{
	struct uart_port * uport = dev_get_drvdata(devp);
	//2018.05.11
	int device_ID;
	
	unsigned int devnum = 
		contec_mcs341_device_deviceNum_get( (unsigned long) uport->mapbase) - 1;

	device_ID = contec_mcs341_device_productid_get( devnum );

	switch(device_ID){
		case CPS_DEVICE_COM1QL :
			device_remove_file(devp, &dev_attr_dev_power);
			device_remove_file(devp, &dev_attr_interrupt);
			device_remove_file(devp, &dev_attr_led0_restore);
			device_remove_file(devp, &dev_attr_led1_restore);
			device_remove_file(devp, &dev_attr_led2_restore);
			device_remove_file(devp, &dev_attr_led3_restore);
#if 1
			device_remove_file(devp, &dev_attr_dev_subunit_manual_power);
			device_remove_file(devp, &dev_attr_dev_subunit_manual_reset);			
#endif

		break;
		//2018.05.11
		case CPS_DEVICE_COM1LC :
			device_remove_file(devp, &dev_attr_dev_power);
#if 1
			device_remove_file(devp, &dev_attr_dev_subunit_manual_power);
#endif
		break;
	}
	device_remove_file(devp, &dev_attr_id);
	device_remove_file(devp,&dev_attr_revision);
	device_remove_file(devp,&dev_attr_board_ver);	
 }

 static int __init cpscom_init(void)
{
	int ret;

	if (nr_uarts > UART_NR)
		nr_uarts = UART_NR;

	printk(KERN_INFO "CPS-Serial: 16550 driver, "
		"%d ports, IRQ sharing %sabled\n", nr_uarts,
		share_irqs ? "en" : "dis");

#ifdef CONFIG_SPARC
	ret = sunserial_register_minors(&cpscom_reg, UART_NR);
#else
	cpscom_reg.nr = UART_NR;
	ret = uart_register_driver(&cpscom_reg);
#endif
	if (ret)
		goto out;

	ret = cps_fpga_init();

	if(ret){
		goto out;
	}

	ret = cpscom_host_init();

	if(ret){
		goto out;
	}

	cpscom_register_ports(&cpscom_reg, NULL);

	//cps_fpga_init();  // cps_common_io.h 

	ret = platform_driver_register(&cpscom_isa_driver);
	if (ret == 0)
		goto out;

#ifdef CONFIG_SPARC
	sunserial_unregister_minors(&cpscom_reg, UART_NR);
#else
	uart_unregister_driver(&cpscom_reg);
#endif

out:
	return ret;
}

static void __exit cpscom_exit(void)
{
	/*
	 * This tells cpscom_unregister_port() not to re-register
	 * the ports (thereby making cpscom_isa_driver permanently
	 * in use.)
	 */

	platform_driver_unregister(&cpscom_isa_driver);

	//cpscom_unregister_port( 0 );
	cpscom_remove(NULL);

#ifdef CONFIG_SPARC
	sunserial_unregister_minors(&cpscom_reg, UART_NR);
#else
	uart_unregister_driver(&cpscom_reg);
#endif
}

module_init(cpscom_init);
module_exit(cpscom_exit);

EXPORT_SYMBOL(cpscom_suspend_port);
EXPORT_SYMBOL(cpscom_resume_port);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("CONTEC CONPROSYS serial driver");

MODULE_VERSION(DRV_VERSION);

module_param(share_irqs, uint, 0644);
MODULE_PARM_DESC(share_irqs, "Share IRQs with other non-8250/16x50 devices"
	" (unsafe)");

module_param(nr_uarts, uint, 0644);
MODULE_PARM_DESC(nr_uarts, "Maximum number of UARTs supported. (1-" __MODULE_STRING(CONFIG_SERIAL_8250_NR_UARTS) ")");

module_param(skip_txen_test, uint, 0644);
MODULE_PARM_DESC(skip_txen_test, "Skip checking for the TXEN bug at init time");

#ifdef CONFIG_SERIAL_8250_RSA
module_param_array(probe_rsa, ulong, &probe_rsa_count, 0444);
MODULE_PARM_DESC(probe_rsa, "Probe I/O ports for RSA");
#endif
MODULE_ALIAS_CHARDEV_MAJOR(CPSCOM_MAJOR);

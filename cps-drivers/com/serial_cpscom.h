/*
 * serial_cpscom.h
 *
 * Copyright (C) 2015 by Syunsuke Okamoto.
 * 
 * Redistribution of this file is permitted under the terms of the GNU 
 * Public License (GPL)
 * 
 * CONTEC CONPROSYS CPS-xxx-COM device driver. 
*
 */

#ifndef _CONTEC_CONPROSYS_SERIAL_H
#define _CONTEC_CONPROSYS_SERIAL_H

#define CPSCOM_MAJOR			 160
#define CONTEC_SERIAL_IVR		0x3f		/* 0x3f */
#define CONTEC_SERIAL_IVR_MASK		0x3f		
#define CPSCOM_BASE_BAUD		( 14745600 / 16 )
#define CPS_BASE_ADDRESS       		( 0x08000000 )
#define CPSCOM_BASE_ADDRESS		( CPS_BASE_ADDRESS | 0x110 )
#define CPSCOM_IRQ			7
#define SERIAL_IO_CPS 			128

#ifdef CONFIG_CONPROSYS_SDK
 #include "../include/cps_common_io.h"
 #include "../include/cps.h"
 #include "../include/cps_ids.h"
 #include "../include/cps_extfunc.h"
#else
 #include "../../include/cps_common_io.h"
 #include "../../include/cps.h"
 #include "../../include/cps_ids.h"
 #include "../../include/cps_extfunc.h"
#endif

#define UPIO_CPS			SERIAL_IO_CPS
#define PORT_CPS16550	192
#define PORT_CPS16550A		193
/* cpscom_serial_port structure based old_serial_port added mapbase */
struct cpscom_serial_port {
	unsigned int	uart;
	unsigned int	baud_base;
	unsigned int	port;
	unsigned int	irq;
	upf_t						flags;
	unsigned char	hub6;
	unsigned char io_type;
	unsigned char __iomem *iomem_base;
	unsigned short	iomem_reg_shift;
	unsigned long	irqflags;

	resource_size_t	mapbase;		

};


static const struct cpscom_serial_port cpscom_serial_ports[] = {
	{	
			.uart = 0,
			.baud_base = CPSCOM_BASE_BAUD,
			.port = CPSCOM_BASE_ADDRESS,
			.mapbase = (resource_size_t)CPSCOM_BASE_ADDRESS,
			.irq	= CPSCOM_IRQ,
			.flags = ( UPF_SKIP_TEST | UPF_IOREMAP | UPF_SHARE_IRQ),
			.io_type = SERIAL_IO_CPS,
			.iomem_reg_shift = 0,
	},
	{
			.uart = 0,
			.baud_base = CPSCOM_BASE_BAUD,
			.port = CPSCOM_BASE_ADDRESS + 8,
			.mapbase = (resource_size_t)(CPSCOM_BASE_ADDRESS + 8),
			.irq	= CPSCOM_IRQ,
			.flags = ( UPF_SKIP_TEST | UPF_IOREMAP | UPF_SHARE_IRQ),
			.io_type = SERIAL_IO_CPS,
			.iomem_reg_shift = 0,
	},
		
};

#define CPSCOM_SHARE_IRQS	 1
#define CPSCOM_RUNTIME_UARTS 64
#define CPSCOM_NR_UARTS 64



int cpscom_register_port(struct uart_port *);
void cpscom_unregister_port(int line);
void cpscom_suspend_port(int line);
void cpscom_resume_port(int line);

extern int early_serial_setup(struct uart_port *port);

//int cpscom_match_port(struct uart_port *p1, struct uart_port *p2);
extern int cpscom_find_port(struct uart_port *p);
extern int cpscom_find_port_for_earlycon(void);
extern int setup_early_cpscom_console(char *cmdline);
extern void cpscom_do_set_termios(struct uart_port *port,
		struct ktermios *termios, struct ktermios *old);
extern void cpscom_do_pm(struct uart_port *port, unsigned int state,
			     unsigned int oldstate);
int cpscom_handle_irq(struct uart_port *port, unsigned int iir);

extern void cpscom_set_isa_configurator(void (*v)
					(int port, struct uart_port *up,
						unsigned short *capabilities));


#endif

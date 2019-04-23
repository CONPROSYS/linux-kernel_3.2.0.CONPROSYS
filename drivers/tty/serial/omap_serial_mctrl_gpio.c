/*
 * Helpers for controlling modem lines via GPIO
 *
 * Copyright (C) 2014 Paratronic S.A.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/err.h>
#include <linux/device.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/termios.h>
#include <linux/serial_core.h>
#include <linux/module.h>

#include "omap_serial_mctrl_gpio.h"

struct mctrl_gpios {
	struct uart_port *port;
	int irq[UART_GPIO_MAX];
	unsigned int mctrl_prev;
	bool mctrl_on;
};

static const struct {
	const char *name;
	unsigned int mctrl;
	bool dir_out;
} mctrl_gpios_desc[UART_GPIO_MAX] = {
	{ "cts", TIOCM_CTS, false, },
	{ "dsr", TIOCM_DSR, false, },
	{ "dcd", TIOCM_CD, false, },
	{ "rng", TIOCM_RNG, false, },
	{ "rts", TIOCM_RTS, true, },
	{ "dtr", TIOCM_DTR, true, },
};

/**
 * @brief setting gpio for modem ctrl (RTS, DTR)
 * 
 * @param port 
 * @param mctrl 
 */
void omap_mctrl_gpio_set(struct uart_omap_port *port, unsigned int mctrl)
{
	enum mctrl_gpio_idx i;
	unsigned int count = 1;
	int val = 0;

	if (port == NULL)
		return;

	for (i = 0; i < UART_GPIO_MAX; i++)
		if ( ( port->gpios[i] != ARCH_NR_GPIOS ) && mctrl_gpios_desc[i].dir_out) {
			if( !!(mctrl & mctrl_gpios_desc[i].mctrl) )
				val = 0;	// High
			else
				val = 1;	// Low

			dev_dbg(port->port.dev," GPIO-SET (%s) : %d ", mctrl_gpios_desc[i].name, val );

			gpio_set_value(port->gpios[i], val);

			count++;
		}
}
EXPORT_SYMBOL_GPL(omap_mctrl_gpio_set);

// struct gpio_desc *mctrl_gpio_to_gpiod(struct mctrl_gpios *gpios,
// 				      enum mctrl_gpio_idx gidx)
// {
// 	return gpios->gpio[gidx];
// }
// EXPORT_SYMBOL_GPL(mctrl_gpio_to_gpiod);

/**
 * @brief get gpio for modem control( CTS, DSR, CD, RI)
 * 
 * @param port 
 * @param mctrl 
 * @return unsigned int 
 */
unsigned int omap_mctrl_gpio_get(struct uart_omap_port *port, unsigned int *mctrl)
{
	enum mctrl_gpio_idx i;

	if (port == NULL)
		return *mctrl;

	for (i = 0; i < UART_GPIO_MAX; i++) {
		if ( ( port->gpios[i] != ARCH_NR_GPIOS ) && !mctrl_gpios_desc[i].dir_out) {
		
			dev_dbg(port->port.dev," GPIO-GET (%s) : %d ", mctrl_gpios_desc[i].name, gpio_get_value(port->gpios[i]) );

			if ( !gpio_get_value(port->gpios[i]) )
				*mctrl |= mctrl_gpios_desc[i].mctrl;
			else
				*mctrl &= ~mctrl_gpios_desc[i].mctrl;
		}
	}

	return *mctrl;
}
EXPORT_SYMBOL_GPL(omap_mctrl_gpio_get);

/**
 * @brief get gpio for modem control( RTS, DTR )
 * 
 * @param port 
 * @param mctrl 
 * @return unsigned int 
 */
unsigned int
omap_mctrl_gpio_get_outputs(struct uart_omap_port *port, unsigned int *mctrl)
{
	enum mctrl_gpio_idx i;

	if (port == NULL)
		return *mctrl;

	for (i = 0; i < UART_GPIO_MAX; i++) {
		if (( port->gpios[i] != ARCH_NR_GPIOS ) && mctrl_gpios_desc[i].dir_out) {

			dev_dbg(port->port.dev," GPIO-GET (%s) : %d ", mctrl_gpios_desc[i].name, gpio_get_value(port->gpios[i]) );

			if ( !gpio_get_value(port->gpios[i]) )	// 0 ... High
				*mctrl |= mctrl_gpios_desc[i].mctrl;
			else// 1 ... Low
				*mctrl &= ~mctrl_gpios_desc[i].mctrl;
		}
	}

	return *mctrl;
}
EXPORT_SYMBOL_GPL(omap_mctrl_gpio_get_outputs);

// struct mctrl_gpios *mctrl_gpio_init_noauto(struct device *dev, unsigned int idx)
// {
// 	struct mctrl_gpios *gpios;
// 	enum mctrl_gpio_idx i;

// 	gpios = devm_kzalloc(dev, sizeof(*gpios), GFP_KERNEL);
// 	if (!gpios)
// 		return ERR_PTR(-ENOMEM);

// 	for (i = 0; i < UART_GPIO_MAX; i++) {
// 		enum gpiod_flags flags;

// 		if (mctrl_gpios_desc[i].dir_out)
// 			flags = GPIOD_OUT_LOW;
// 		else
// 			flags = GPIOD_IN;

// 		gpios->gpio[i] =
// 			devm_gpiod_get_index_optional(dev,
// 						      mctrl_gpios_desc[i].name,
// 						      idx, flags);

// 		if (IS_ERR(gpios->gpio[i]))
// 			return ERR_CAST(gpios->gpio[i]);
// 	}

// 	return gpios;
// }
// EXPORT_SYMBOL_GPL(mctrl_gpio_init_noauto);

// #define MCTRL_ANY_DELTA (TIOCM_RI | TIOCM_DSR | TIOCM_CD | TIOCM_CTS)
// static irqreturn_t mctrl_gpio_irq_handle(int irq, void *context)
// {
// 	struct mctrl_gpios *gpios = context;
// 	struct uart_port *port = gpios->port;
// 	u32 mctrl = gpios->mctrl_prev;
// 	u32 mctrl_diff;
// 	unsigned long flags;

// 	mctrl_gpio_get(gpios, &mctrl);

// 	spin_lock_irqsave(&port->lock, flags);

// 	mctrl_diff = mctrl ^ gpios->mctrl_prev;
// 	gpios->mctrl_prev = mctrl;

// 	if (mctrl_diff & MCTRL_ANY_DELTA && port->state != NULL) {
// 		if ((mctrl_diff & mctrl) & TIOCM_RI)
// 			port->icount.rng++;

// 		if ((mctrl_diff & mctrl) & TIOCM_DSR)
// 			port->icount.dsr++;

// 		if (mctrl_diff & TIOCM_CD)
// 			uart_handle_dcd_change(port, mctrl & TIOCM_CD);

// 		if (mctrl_diff & TIOCM_CTS)
// 			uart_handle_cts_change(port, mctrl & TIOCM_CTS);

// 		wake_up_interruptible(&port->state->port.delta_msr_wait);
// 	}

// 	spin_unlock_irqrestore(&port->lock, flags);

// 	return IRQ_HANDLED;
// }

/**
 * @brief initialize gpio to use. 
 * @note The gpio's value of ARCH_NR_GPIOS means "Not use gpio".
 * 
 * @param port 
 * @return int 
 */
int omap_mctrl_gpio_init(struct uart_omap_port *port)
{
	int i;

	dev_dbg(port->port.dev, " GPIO-NR = %d", ARCH_NR_GPIOS);

	if( !port )
		return -1;

	for (i = 0; i < UART_GPIO_MAX; i++) {
		int ret;

		dev_dbg(port->port.dev, " GPIO-Init: pin(%d - %d)", port->gpios[i]/32, port->gpios[i]%32);

		if (port->gpios[i] >= ARCH_NR_GPIOS)
			continue;

		ret = gpio_request( port->gpios[i], mctrl_gpios_desc[i].name );

		dev_dbg(port->port.dev, " GPIO-REQUEST : pin(%d - %d) %s ", port->gpios[i]/32, port->gpios[i]%32,  mctrl_gpios_desc[i].name );

		if( ret == 0 ){
			if (mctrl_gpios_desc[i].dir_out){
				dev_dbg(port->port.dev," GPIO-DIR_OUT");
				gpio_direction_output(port->gpios[i], 1);
			}
			else{
				dev_dbg(port->port.dev," GPIO-DIR_IN");				
				gpio_direction_input(port->gpios[i]);	
			}
		}

//		irq_to_gpio( OMAP_GPIO_IRQ(port->gpios[i]) );


		/* irqs should only be enabled in .enable_ms */
		// irq_set_status_flags(gpios->irq[i], IRQ_NOAUTOEN);

		// ret = devm_request_irq(port->dev, gpios->irq[i],
		// 		       mctrl_gpio_irq_handle,
		// 		       IRQ_TYPE_EDGE_BOTH, dev_name(port->dev),
		// 		       port);
		// if (ret) {
		// 	/* alternatively implement polling */
		// 	dev_err(port->dev,
		// 		"failed to request irq for %s (idx=%d, err=%d)\n",
		// 		mctrl_gpios_desc[i].name, idx, ret);
		// 	return ERR_PTR(ret);
		// }
	}

	return 0;
}
EXPORT_SYMBOL_GPL(omap_mctrl_gpio_init);

// void mc341_ctrl_gpio_free(struct device *dev, struct uart_port *port)
// {
// 	int i = 0;

// 	if ( !port )
// 		return;

// 	for (i = 0; i < UART_GPIO_MAX; i++) {
// 		if (gpios->irq[i] < ARCH_NR_GPIOS)
// 			devm_free_irq(gpios->port->dev, gpios->irq[i], gpios);

// 		if (gpios->gpio[i])
// 			devm_gpiod_put(dev, gpios->gpio[i]);
// 	}
// 	devm_kfree(dev, gpios);
// }
// EXPORT_SYMBOL_GPL(mc341_mctrl_gpio_free);

// void mctrl_gpio_enable_ms(struct mctrl_gpios *gpios)
// {
// 	enum mctrl_gpio_idx i;

// 	if (gpios == NULL)
// 		return;

// 	/* .enable_ms may be called multiple times */
// 	if (gpios->mctrl_on)
// 		return;

// 	gpios->mctrl_on = true;

// 	/* get initial status of modem lines GPIOs */
// 	mctrl_gpio_get(gpios, &gpios->mctrl_prev);

// 	for (i = 0; i < UART_GPIO_MAX; ++i) {
// 		if (!gpios->irq[i])
// 			continue;

// 		enable_irq(gpios->irq[i]);
// 	}
// }
// EXPORT_SYMBOL_GPL(mctrl_gpio_enable_ms);

// void mctrl_gpio_disable_ms(struct mctrl_gpios *gpios)
// {
// 	enum mctrl_gpio_idx i;

// 	if (gpios == NULL)
// 		return;

// 	if (!gpios->mctrl_on)
// 		return;

// 	gpios->mctrl_on = false;

// 	for (i = 0; i < UART_GPIO_MAX; ++i) {
// 		if (!gpios->irq[i])
// 			continue;

// 		disable_irq(gpios->irq[i]);
// 	}
// }
// EXPORT_SYMBOL_GPL(mctrl_gpio_disable_ms);

MODULE_LICENSE("GPL");

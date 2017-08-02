/*
 * Memory-mapped interface driver for DW SPI Core
 *
 * Copyright (c) 2010, Octasic semiconductor.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/scatterlist.h>
#include <linux/module.h>
#include <linux/of.h>

#include "spi-dw.h"

#define DRIVER_NAME "dw_spi_mmio"

struct dw_spi_mmio {
	struct dw_spi  dws;
	struct clk     *clk;
};

static int dw_spi_mmio_probe(struct platform_device *pdev)
{
	struct dw_spi_mmio *dwsmmio;
	struct dw_spi *dws;
	struct resource *mem, *ioarea;
	int ret, num_cs, bus_num;

	dwsmmio = kzalloc(sizeof(struct dw_spi_mmio), GFP_KERNEL);
	if (!dwsmmio) {
		ret = -ENOMEM;
		goto err_end;
	}

	dws = &dwsmmio->dws;

	/* Get basic io resource and map it */
	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pdev->dev, "no mem resource?\n");
		ret = -EINVAL;
		goto err_kfree;
	}

	ioarea = request_mem_region(mem->start, resource_size(mem),
			pdev->name);
	if (!ioarea) {
		dev_err(&pdev->dev, "SPI region already claimed\n");
		ret = -EBUSY;
		goto err_kfree;
	}

	dws->regs = ioremap_nocache(mem->start, resource_size(mem));
	if (!dws->regs) {
		dev_err(&pdev->dev, "SPI region already mapped\n");
		ret = -ENOMEM;
		goto err_release_reg;
	}

	dws->irq = platform_get_irq(pdev, 0);
	if (dws->irq < 0) {
		dev_err(&pdev->dev, "no irq resource?\n");
		ret = dws->irq; /* -ENXIO */
		goto err_unmap;
	}

	dwsmmio->clk = clk_get(&pdev->dev, NULL);
	if (IS_ERR(dwsmmio->clk)) {
		ret = PTR_ERR(dwsmmio->clk);
		goto err_irq;
	}
	clk_enable(dwsmmio->clk);

	ret = of_property_read_u32(pdev->dev.of_node, "bus-num", &bus_num);
	if (ret < 0)
		dws->bus_num = 0;
	else
		dws->bus_num = bus_num;

	ret = of_property_read_u32(pdev->dev.of_node, "num-chipselect", &num_cs);
	if (ret < 0)
		dws->num_cs = 4;
	else
		dws->num_cs = num_cs;

	dws->parent_dev = &pdev->dev;
	dws->max_freq = clk_get_rate(dwsmmio->clk);

	ret = dw_spi_add_host(dws);
	if (ret)
		goto err_clk;

	platform_set_drvdata(pdev, dwsmmio);
	return 0;

err_clk:
	clk_disable(dwsmmio->clk);
	clk_put(dwsmmio->clk);
	dwsmmio->clk = NULL;
err_irq:
	free_irq(dws->irq, dws);
err_unmap:
	iounmap(dws->regs);
err_release_reg:
	release_mem_region(mem->start, resource_size(mem));
err_kfree:
	kfree(dwsmmio);
err_end:
	return ret;
}

static int dw_spi_mmio_remove(struct platform_device *pdev)
{
	struct dw_spi_mmio *dwsmmio = platform_get_drvdata(pdev);
	struct resource *mem;

	platform_set_drvdata(pdev, NULL);

	clk_disable(dwsmmio->clk);
	clk_put(dwsmmio->clk);
	dwsmmio->clk = NULL;

	free_irq(dwsmmio->dws.irq, &dwsmmio->dws);
	dw_spi_remove_host(&dwsmmio->dws);
	iounmap(dwsmmio->dws.regs);
	kfree(dwsmmio);

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	release_mem_region(mem->start, resource_size(mem));
	return 0;
}

static struct of_device_id dw_spi_mmio_of_match[] = {
		{ .compatible = "snps,dw-spi-mmio", },
		{ /* sentinel */}
};
MODULE_DEVICE_TABLE(of, dw_spi_mmio_of_match);


static struct platform_driver dw_spi_mmio_driver = {
	.probe		= dw_spi_mmio_probe,
	.remove		= dw_spi_mmio_remove,
	.driver		= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = dw_spi_mmio_of_match,
	},
};
module_platform_driver(dw_spi_mmio_driver);

MODULE_AUTHOR("Jean-Hugues Deschenes <jean-hugues.deschenes@octasic.com>");
MODULE_DESCRIPTION("Memory-mapped I/O interface driver for DW SPI Core");
MODULE_LICENSE("GPL v2");

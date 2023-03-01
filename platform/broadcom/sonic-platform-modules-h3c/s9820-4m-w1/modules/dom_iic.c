/*
 * dom_iic.c: I2C bus driver for H3C dom fpga I2C controller
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2.  This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/slab.h>
/*私有文件*/
#include "pub.h"
#include "bsp_base.h"
#include "static_ktype.h"



#define ADPATER_NAME "domiic"

static ushort Portnum = 0x00;

module_param(Portnum,     ushort, 0644);
MODULE_PARM_DESC(Portnum, "Number of switch ports\n");

struct dom_priv
{
    struct i2c_adapter dom_adap;
    u8 opt_idx;
    bool valid;
} *gpstDomPriv;

void dom_del_adap(void)
{
    int idx;
    struct dom_priv *pPriv;

    for (idx = 0; idx < Portnum; idx ++)
    {
        pPriv = &gpstDomPriv[idx];
        if (true == pPriv->valid)
        {
            i2c_del_adapter(&(pPriv->dom_adap));
        }
    }
}

static int dom_iic_xfer(struct i2c_adapter *adap, struct i2c_msg *msg, int num)
{
    int ret;
    u8 op, internal_addr, opt_idx = 0;
    u16 iic_addr, cnt = 0;
    u8 *msg_buf;
    struct dom_priv *drv_data;

    int(*bsp_get_optic_raw)(int , int , u8*, u8, int );

    if (msg->flags & I2C_M_TEN)
    {
        return -EINVAL;
    }
    if (num > 2)
    {
        return -EIO;
    }

    drv_data = i2c_get_adapdata(adap);
    iic_addr = msg[0].addr;
    opt_idx = drv_data->opt_idx;

    if (0x50 == iic_addr)
    {
        bsp_get_optic_raw = bsp_get_optic_eeprom_raw;
    }
    else
    {
        bsp_get_optic_raw = bsp_get_optic_dom_raw;
    }
    if (num == 2)
    {
        /* read optic */
        internal_addr = *msg[0].buf;
        msg_buf = msg[1].buf;
        cnt = msg[1].len;
        op = 0;
        ret = bsp_get_optic_raw(MAIN_BOARD_SLOT_INDEX, opt_idx, msg_buf, internal_addr, cnt);
    }
    else
    {
        /* write optic */
        internal_addr = msg[0].buf[0];
        msg_buf = &msg[0].buf[1];
        cnt = msg[0].len - 1;
        op = 1;
        ret = bsp_optical_eeprom_write_bytes(MAIN_BOARD_SLOT_INDEX, opt_idx, msg_buf, internal_addr, cnt);
    }
    /*
     * op: bit0 (0 for read, 1 for write)
           bit1 (0 for 0x50, 1 for 0x51 )
     */
    //op |= ((0x50 == iic_addr) ? 0x00 : 0x02);

    if (ret)
    {
        num = -ENODATA;
    }
    return num;
}

static u32 dom_iic_func(struct i2c_adapter *adap)
{
    return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm dom_iic_algorithm =
{
    .master_xfer = dom_iic_xfer,
    .functionality = dom_iic_func,
};

/*
 * wangdongwen : template for all adapter for every single port
 */
static const struct i2c_adapter dom_iic_adapter =
{
    .owner = THIS_MODULE,
    .class = I2C_CLASS_HWMON | I2C_CLASS_SPD,
    .algo = &dom_iic_algorithm,
    .retries = 3,
};

static int __init dom_iic_init(void)
{
    int ret = 0;
    u8 idx = 0;
    struct i2c_adapter *pAdap;
    /*
     * wangdongwen : validate the number of ports
     */
    if (0 == Portnum)   /* uninitialized */
    {
        return -EINVAL;
    }

    /* add i2c adapter to i2c tree */
    gpstDomPriv = kcalloc(Portnum, sizeof(struct dom_priv), GFP_KERNEL);
    for (idx = 0; idx < Portnum; idx ++)
    {
        pAdap = &gpstDomPriv[idx].dom_adap;
        *pAdap = dom_iic_adapter;
        gpstDomPriv[idx].valid = false;
        scnprintf(pAdap->name, sizeof(pAdap->name), ADPATER_NAME"%d", idx + 1);
        gpstDomPriv[idx].opt_idx = idx;
        i2c_set_adapdata(pAdap, &gpstDomPriv[idx]);
        ret = i2c_add_adapter(pAdap);
        if (ret)
        {
            goto err;
        }
        gpstDomPriv[idx].valid = true;
    }
    return 0;
err:
    dom_del_adap();
    kfree(gpstDomPriv);
    return ret;
}

static void __exit dom_iic_exit(void)
{
    /* wangdongwen : remove adapter & data */
    dom_del_adap();
    /*  cleaning stuff */
    kfree(gpstDomPriv);
}

MODULE_AUTHOR("wang.dongwen@h3c.com");
MODULE_DESCRIPTION("Dom IIC Adapter Shell");
MODULE_LICENSE("GPL");
module_init(dom_iic_init);
module_exit(dom_iic_exit);

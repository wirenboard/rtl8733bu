/******************************************************************************
 *
 * Copyright(c) 2007 - 2017 Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 *****************************************************************************/
#define _USB_OPS_LINUX_C_

#include <drv_types.h>
#include <hal_data.h>
#include <rtw_sreset.h>
#include <rtw_io_records.h>

#ifdef CONFIG_RTW_IO_RECORDS
enum usb_type {
	USB_IO_RECORD_REG_READ,
	USB_IO_RECORD_REG_WRITE,
	USB_IO_RECORD_WRITE_PORT,
	USB_IO_RECORD_READ_PORT,
};

static const char *usb_type_str[] = {
	[USB_IO_RECORD_REG_READ]	= "read",
	[USB_IO_RECORD_REG_WRITE]	= "write",
	[USB_IO_RECORD_WRITE_PORT]	= "TX",
	[USB_IO_RECORD_READ_PORT]	= "RX",
};

_lock io_record_usb_lock;

#define DECLARE_IO_RECORD_USB_IRQL(__irqL) _irqL __irqL

#define dbg_rtw_io_record_usb_lock(_lock, _irqL) \
	_enter_critical(_lock, _irqL)

#define dbg_rtw_io_record_usb_unlock(_lock, _irqL) \
	_exit_critical(_lock, _irqL)

#define dbg_rtw_io_record_fill_n_usb_io(_r, _type, _addr, _cnt, _err, _data) \
		if (_type == REALTEK_USB_VENQT_READ) { dbg_rtw_io_record_fill_n(_r, USB_IO_RECORD_REG_READ, _addr, _cnt, _err, _data); } \
		else { dbg_rtw_io_record_fill_n(_r, USB_IO_RECORD_REG_WRITE, _addr, _cnt, _err, _data); }

#define dbg_rtw_io_record_fill_n_usb_trx(_r, _type, _bulkid, _cnt, _err) \
		do { \
			if (_r) { \
				(_r)->type = _type; \
				if (_type == USB_IO_RECORD_WRITE_PORT) { \
					(_r)->etime = rtw_sptime_get_raw(); \
				} \
				(_r)->addr = _bulkid; \
				(_r)->cnt = _cnt; \
				(_r)->err = _err; \
				__dbg_rtw_io_record_fill_cpu_info(_r); \
				__dbg_rtw_io_record_warn(_r); \
			} \
		} while (0)

#define USB_R_TITLE_PRE "=================== USB I/O records =======================\n"
#define USB_R_TITLE_FMT \
		USB_R_TITLE_PRE \
		"%-9s %-11s %-5s %-7s %-5s %-6s"
#define USB_R_TITLE_TFMT \
		USB_R_TITLE_PRE \
		"%s\t%s\t\t%s\t%s\t%s\t%s"
#define USB_R_TITLE_ARG , "seq", "io_time", "type", "addr", "size", "status"
#define USB_IO_R_VALUE_FMT "%9zu %lld.%09lld %-5s 0x%05x %5u %6d"
#define USB_IO_R_VALUE_TFMT "%zu\t%lld.%09lld\t%s\t0x%05x\t%u\t%d"
#define USB_IO_R_VALUE_ARG \
		, seq \
		, rtw_sptime_diff_ns(r->stime, r->etime) / 1000000000, rtw_sptime_diff_ns(r->stime, r->etime) % 1000000000 \
		, usb_type_str[r->type] \
		, r->addr, r->cnt, r->err

#define USB_TRX_R_VALUE_FMT "%9zu %lld.%09lld %-5s %s%02d %5u %6d"
#define USB_TRX_R_VALUE_TFMT "%zu\t%lld.%09lld\t%s\t%s%02d\t%u\t%d"
#define USB_TRX_R_VALUE_ARG \
		, seq \
		, (r->type == USB_IO_RECORD_WRITE_PORT) ? rtw_sptime_diff_ns(r->stime, r->etime) / 1000000000 : 0 \
		, (r->type == USB_IO_RECORD_WRITE_PORT) ? rtw_sptime_diff_ns(r->stime, r->etime) % 1000000000 : 0 \
		, usb_type_str[r->type] \
		, (r->type == USB_IO_RECORD_WRITE_PORT) ? "bkout" : " bkin", r->addr \
		, r->cnt, r->err

#if DBG_RTW_IO_RECORD_DATA_LEN
#define USB_R_TITLE_FMT_DATA " %-2s %-2s %-2s %-2s"
#define USB_R_TITLE_TFMT_DATA "\t%s\t%s\t%s\t%s"
#define USB_R_TITLE_ARG_DATA , "d0", "d1", "d2", "d3"
#define USB_R_VALUE_FMT_DATA " %02x %02x %02x %02x"
#define USB_R_VALUE_TFMT_DATA "\t%02x\t%02x\t%02x\t%02x"
#define USB_R_VALUE_ARG_DATA  ,  r->cnt > 0 ? r->data[0] : 0, r->cnt > 1 ? r->data[1] : 0, r->cnt > 2 ? r->data[2] : 0, r->cnt > 3 ? r->data[3] : 0
#else
#define USB_R_TITLE_FMT_DATA ""
#define USB_R_TITLE_TFMT_DATA ""
#define USB_R_TITLE_ARG_DATA
#define USB_R_VALUE_FMT_DATA ""
#define USB_R_VALUE_TFMT_DATA ""
#define USB_R_VALUE_ARG_DATA
#endif

#if DBG_RTW_IO_RECORD_TASK_INFO
#define USB_R_TITLE_FMT_TASK_INFO " %-15s %-3s"
#define USB_R_TITLE_TFMT_TASK_INFO "\t%s\t%s"
#define USB_R_TITLE_ARG_TASK_INFO , "task_comm", "cpu"
#define USB_IO_R_VALUE_FMT_TASK_INFO " %-15s %-3d"
#define USB_IO_R_VALUE_TFMT_TASK_INFO "\t%s\t%d"
#define USB_IO_R_VALUE_ARG_TASK_INFO , r->task_comm, r->cpu
#define USB_TRX_R_VALUE_FMT_TASK_INFO " --------------- %-3d"
#define USB_TRX_R_VALUE_TFMT_TASK_INFO "\t---------------\t%d"
#define USB_TRX_R_VALUE_ARG_TASK_INFO , r->cpu
#else
#define USB_R_TITLE_FMT_TASK_INFO ""
#define USB_R_TITLE_TFMT_TASK_INFO ""
#define USB_R_TITLE_ARG_TASK_INFO
#define USB_IO_R_VALUE_FMT_TASK_INFO ""
#define USB_IO_R_VALUE_TFMT_TASK_INFO ""
#define USB_IO_R_VALUE_ARG_TASK_INFO
#define USB_TRX_R_VALUE_FMT_TASK_INFO ""
#define USB_TRX_R_VALUE_TFMT_TASK_INFO ""
#define USB_TRX_R_VALUE_ARG_TASK_INFO
#endif
static void usb_io_record_title_dump_tab(void *sel)
{
	RTW_PRINT_SEL(sel, USB_R_TITLE_TFMT
		USB_R_TITLE_TFMT_DATA
		USB_R_TITLE_TFMT_TASK_INFO
		"\n"
		USB_R_TITLE_ARG
		USB_R_TITLE_ARG_DATA
		USB_R_TITLE_ARG_TASK_INFO
		);
}

static void usb_io_record_value_dump_tab(void *sel, struct rtw_io_record *r, size_t seq)
{
	if (r->type == USB_IO_RECORD_REG_READ || r->type == USB_IO_RECORD_REG_WRITE) {
		RTW_PRINT_SEL(sel, USB_IO_R_VALUE_TFMT
			USB_R_VALUE_TFMT_DATA
			USB_IO_R_VALUE_TFMT_TASK_INFO
			"\n"
			USB_IO_R_VALUE_ARG
			USB_R_VALUE_ARG_DATA
			USB_IO_R_VALUE_ARG_TASK_INFO
		);
	} else {
		RTW_PRINT_SEL(sel, USB_TRX_R_VALUE_TFMT
			"\t--\t--\t--\t--"
			USB_TRX_R_VALUE_TFMT_TASK_INFO
			"\n"
			USB_TRX_R_VALUE_ARG
			USB_TRX_R_VALUE_ARG_TASK_INFO
		);
	}
}

static void usb_io_record_title_dump(void *sel)
{
	RTW_PRINT_SEL(sel, USB_R_TITLE_FMT
		USB_R_TITLE_FMT_DATA
		USB_R_TITLE_FMT_TASK_INFO
		"\n"
		USB_R_TITLE_ARG
		USB_R_TITLE_ARG_DATA
		USB_R_TITLE_ARG_TASK_INFO
	);
}

static void usb_io_record_value_dump(void *sel, struct rtw_io_record *r, size_t seq)
{
	if ((r->type == USB_IO_RECORD_REG_READ) || (r->type == USB_IO_RECORD_REG_WRITE)) {
		RTW_PRINT_SEL(sel, USB_IO_R_VALUE_FMT
			USB_R_VALUE_FMT_DATA
			USB_IO_R_VALUE_FMT_TASK_INFO
			"\n"
			USB_IO_R_VALUE_ARG
			USB_R_VALUE_ARG_DATA
			USB_IO_R_VALUE_ARG_TASK_INFO
		);
	} else {
		RTW_PRINT_SEL(sel, USB_TRX_R_VALUE_FMT
			" -- -- -- --"
			USB_TRX_R_VALUE_FMT_TASK_INFO
			"\n"
			USB_TRX_R_VALUE_ARG
			USB_TRX_R_VALUE_ARG_TASK_INFO
		);
	}
}

typedef void (*usb_rec_title_dump)(void *);
typedef void (*usb_rec_value_dump)(void *, struct rtw_io_record *, size_t);

void rtw_usb_io_records_dump_title(void *sel, bool tab)
{
	usb_rec_title_dump title_dump = tab ? usb_io_record_title_dump_tab : usb_io_record_title_dump;

	title_dump(sel);
}

void rtw_usb_io_records_dump_value_by_seq(void *sel, bool tab, size_t seq)
{
	size_t oldest_pos = dbg_rtw_io_records.record[dbg_rtw_io_records.pos].valid ? dbg_rtw_io_records.pos : 0;
	struct rtw_io_record *record = &dbg_rtw_io_records.record[(oldest_pos + seq) % dbg_rtw_io_records.record_num];

	usb_rec_value_dump value_dump = tab ? usb_io_record_value_dump_tab : usb_io_record_value_dump;

	if (record->valid)
		value_dump(sel, record, seq);
}

struct usb_io_records_summary {
	sysptime stime;
	sysptime etime;

	size_t read_io_num;
	size_t write_io_num;
	size_t tx_io_num;

	sysptime read_io_int_total;
	sysptime read_io_int_min;
	sysptime read_io_int_max;
	sysptime read_io_int_avg;
	sysptime write_io_int_total;
	sysptime write_io_int_min;
	sysptime write_io_int_max;
	sysptime write_io_int_avg;
	sysptime tx_io_int_total;
	sysptime tx_io_int_min;
	sysptime tx_io_int_max;
	sysptime tx_io_int_avg;

	u32 read_io_min_addr;
	u32 read_io_max_addr;
	u32 write_io_min_addr;
	u32 write_io_max_addr;
};

void rtw_usb_io_records_summary_dump(void *sel)
{
	struct usb_io_records_summary data;
	struct rtw_io_record *record;
	sysptime io_time;
	size_t oldest_pos = dbg_rtw_io_records.record[dbg_rtw_io_records.pos].valid ? dbg_rtw_io_records.pos : 0;
	size_t i;

	_rtw_memset(&data, 0, sizeof(data));
	data.read_io_int_total = rtw_sptime_zero();
	data.read_io_int_min = rtw_sptime_zero();
	data.read_io_int_max = rtw_sptime_zero();
	data.read_io_int_avg = rtw_sptime_zero();
	data.write_io_int_total = rtw_sptime_zero();
	data.write_io_int_min = rtw_sptime_zero();
	data.write_io_int_max = rtw_sptime_zero();
	data.write_io_int_avg = rtw_sptime_zero();
	data.tx_io_int_total = rtw_sptime_zero();
	data.tx_io_int_min = rtw_sptime_zero();
	data.tx_io_int_max = rtw_sptime_zero();
	data.tx_io_int_avg = rtw_sptime_zero();

	for (i = 0; i < dbg_rtw_io_records.record_num; i++) {
		record = &dbg_rtw_io_records.record[(oldest_pos + i) % dbg_rtw_io_records.record_num];
		if (!record->valid)
			break;

		io_time = rtw_sptime_diff_ns(record->stime, record->etime);

		switch (record->type) {
			case USB_IO_RECORD_REG_READ:
				data.read_io_num++;
				data.read_io_int_total += io_time;

				if (data.read_io_num == 1) {
					data.read_io_int_max = io_time;
					data.read_io_min_addr = record->addr;
					data.read_io_int_min = io_time;
					data.read_io_max_addr = record->addr;
				} else {
					if (io_time < data.read_io_int_min) {
						data.read_io_int_min = io_time;
						data.read_io_min_addr = record->addr;
					} else if (io_time > data.read_io_int_max) {
						data.read_io_int_max = io_time;
						data.read_io_max_addr = record->addr;
					}
				}
				break;
			case USB_IO_RECORD_REG_WRITE:
				data.write_io_num++;
				data.write_io_int_total += io_time;

				if (data.write_io_num == 1) {
					data.write_io_int_max = io_time;
					data.write_io_min_addr = record->addr;
					data.write_io_int_min = io_time;
					data.write_io_max_addr = record->addr;
				} else {
					if (io_time < data.write_io_int_min) {
						data.write_io_int_min = io_time;
						data.write_io_min_addr = record->addr;
					} else if (io_time > data.write_io_int_max) {
						data.write_io_int_max = io_time;
						data.write_io_max_addr = record->addr;
					}
				}
				break;
			case USB_IO_RECORD_WRITE_PORT:
				data.tx_io_num++;
				data.tx_io_int_total += io_time;

				if (data.tx_io_num == 1) {
					data.tx_io_int_max = io_time;
					data.tx_io_int_min = io_time;
				} else {
					if (io_time < data.tx_io_int_min)
						data.tx_io_int_min = io_time;
					else if (io_time > data.tx_io_int_max)
						data.tx_io_int_max = io_time;
				}
				break;
			default:
				break;
		}

	}

	if (data.read_io_num > 0)
		data.read_io_int_avg = rtw_ns_to_sptime(rtw_sptime_to_ns(data.read_io_int_total) / data.read_io_num);
	if (data.write_io_num > 0)
		data.write_io_int_avg = rtw_ns_to_sptime(rtw_sptime_to_ns(data.write_io_int_total) / data.write_io_num);
	if (data.tx_io_num > 0)
		data.tx_io_int_avg = rtw_ns_to_sptime(rtw_sptime_to_ns(data.tx_io_int_total) / data.tx_io_num);

	RTW_PRINT_SEL(sel,
		"=====================================\n"
		" read I/O total: %lld.%09lld\n"
		"            min: %lld.%09lld (addr:0x%05x)\n"
		"            max: %lld.%09lld (addr:0x%05x)\n"
		"            avg: %lld.%09lld\n"
		"=====================================\n"
		"write I/O total: %lld.%09lld\n"
		"            min: %lld.%09lld (addr:0x%05x)\n"
		"            max: %lld.%09lld (addr:0x%05x)\n"
		"            avg: %lld.%09lld\n"
		"=====================================\n"
		"       TX total: %lld.%09lld\n"
		"            min: %lld.%09lld\n"
		"            max: %lld.%09lld\n"
		"            avg: %lld.%09lld\n"
		, rtw_sptime_to_ns(data.read_io_int_total) / 1000000000, rtw_sptime_to_ns(data.read_io_int_total) % 1000000000
		, rtw_sptime_to_ns(data.read_io_int_min) / 1000000000, rtw_sptime_to_ns(data.read_io_int_min) % 1000000000, data.read_io_min_addr
		, rtw_sptime_to_ns(data.read_io_int_max) / 1000000000, rtw_sptime_to_ns(data.read_io_int_max) % 1000000000, data.read_io_max_addr
		, rtw_sptime_to_ns(data.read_io_int_avg) / 1000000000, rtw_sptime_to_ns(data.read_io_int_avg) % 1000000000
		, rtw_sptime_to_ns(data.write_io_int_total) / 1000000000, rtw_sptime_to_ns(data.write_io_int_total) % 1000000000
		, rtw_sptime_to_ns(data.write_io_int_min) / 1000000000, rtw_sptime_to_ns(data.write_io_int_min) % 1000000000, data.write_io_min_addr
		, rtw_sptime_to_ns(data.write_io_int_max) / 1000000000, rtw_sptime_to_ns(data.write_io_int_max) % 1000000000, data.write_io_max_addr
		, rtw_sptime_to_ns(data.write_io_int_avg) / 1000000000, rtw_sptime_to_ns(data.write_io_int_avg) % 1000000000
		, rtw_sptime_to_ns(data.tx_io_int_total) / 1000000000, rtw_sptime_to_ns(data.tx_io_int_total) % 1000000000
		, rtw_sptime_to_ns(data.tx_io_int_min) / 1000000000, rtw_sptime_to_ns(data.tx_io_int_min) % 1000000000
		, rtw_sptime_to_ns(data.tx_io_int_max) / 1000000000, rtw_sptime_to_ns(data.tx_io_int_max) % 1000000000
		, rtw_sptime_to_ns(data.tx_io_int_avg) / 1000000000, rtw_sptime_to_ns(data.tx_io_int_avg) % 1000000000
	);
}

void rtw_usb_io_records_claim_and_enable(struct dvobj_priv *d, bool enable)
{
	DECLARE_IO_RECORD_USB_IRQL(irqL);
	RTW_INFO("%s enable:%d\n", __func__, enable);

#if !CONFIG_RTW_IO_RECORDS_STATIC
	if (!dbg_rtw_io_records.record)
		RTW_ERR("%s io record is not allocated !", __func__);
	else
#endif
	{
		dbg_rtw_io_record_usb_lock(&io_record_usb_lock, &irqL);
		dbg_rtw_io_records.enable = enable;
		dbg_rtw_io_record_usb_unlock(&io_record_usb_lock, &irqL);
	}
}

void rtw_usb_io_records_init(void)
{
	rtw_io_records_init();
	_rtw_spinlock_init(&io_record_usb_lock);
}

void rtw_usb_io_records_deinit(void)
{
	rtw_io_records_deinit();
	_rtw_spinlock_free(&io_record_usb_lock);
}
#else
#define DECLARE_IO_RECORD_USB_IRQL(__irqL) do {} while (0)
#define dbg_rtw_io_record_usb_lock(_lock, _irqL) do {} while (0)
#define dbg_rtw_io_record_usb_unlock(_lock, _irqL) do {} while (0)
#define dbg_rtw_io_record_fill_n_usb_io(_r, _type, _addr, _cnt, _err, _data) do {} while (0)
#define dbg_rtw_io_record_fill_n_usb_trx(_r, _type, _bulkid, _cnt, _err) do {} while (0)
#endif

struct rtw_async_write_data {
	u8 data[VENDOR_CMD_MAX_DATA_LEN];
	struct usb_ctrlrequest dr;
};

int usbctrl_vendorreq(struct intf_hdl *pintfhdl, u8 request, u16 value, u16 index, void *pdata, u16 len, u8 requesttype)
{
	_adapter	*padapter = pintfhdl->padapter;
	struct dvobj_priv  *pdvobjpriv = adapter_to_dvobj(padapter);
	struct usb_device *udev = pdvobjpriv->pusbdev;

	unsigned int pipe;
	int status = 0;
#ifdef CONFIG_USB_VENDOR_REQ_BUFFER_DYNAMIC_ALLOCATE
	u32 tmp_buflen = 0;
#endif
	u8 reqtype;
	u8 *pIo_buf;
	int vendorreq_times = 0;

#if (defined(CONFIG_RTL8822B) || defined(CONFIG_RTL8821C)) \
	|| defined(CONFIG_RTL8822C) || defined(CONFIG_RTL8822E)
#define REG_ON_SEC 0x00
#define REG_OFF_SEC 0x01
#define REG_LOCAL_SEC 0x02
	u8 current_reg_sec = REG_LOCAL_SEC;
#endif

#ifdef CONFIG_USB_VENDOR_REQ_BUFFER_DYNAMIC_ALLOCATE
	u8 *tmp_buf;
#else /* use stack memory */
	#ifndef CONFIG_USB_VENDOR_REQ_BUFFER_PREALLOC
	u8 tmp_buf[MAX_USB_IO_CTL_SIZE];
	#endif
#endif
	DECLARE_DBG_RTW_IO_RECORD_P(record_r);
	DECLARE_IO_RECORD_USB_IRQL(record_irqL);

	/* RTW_INFO("%s %s:%d\n",__FUNCTION__, current->comm, current->pid); */

	if (RTW_CANNOT_IO(padapter)) {
		status = -EPERM;
		goto exit;
	}

	if (len > MAX_VENDOR_REQ_CMD_SIZE) {
		RTW_INFO("[%s] Buffer len error ,vendor request failed\n", __FUNCTION__);
		status = -EINVAL;
		goto exit;
	}

#ifdef CONFIG_USB_VENDOR_REQ_MUTEX
	_enter_critical_mutex_lock(&pdvobjpriv->usb_vendor_req_mutex, NULL);
#endif


	/* Acquire IO memory for vendorreq */
#ifdef CONFIG_USB_VENDOR_REQ_BUFFER_PREALLOC
	pIo_buf = pdvobjpriv->usb_vendor_req_buf;
#else
	#ifdef CONFIG_USB_VENDOR_REQ_BUFFER_DYNAMIC_ALLOCATE
	tmp_buf = rtw_malloc((u32) len + ALIGNMENT_UNIT);
	tmp_buflen = (u32)len + ALIGNMENT_UNIT;
	#else /* use stack memory */
	tmp_buflen = MAX_USB_IO_CTL_SIZE;
	#endif

	/* Added by Albert 2010/02/09 */
	/* For mstar platform, mstar suggests the address for USB IO should be 16 bytes alignment. */
	/* Trying to fix it here. */
	pIo_buf = (tmp_buf == NULL) ? NULL : tmp_buf + ALIGNMENT_UNIT - ((SIZE_PTR)(tmp_buf) & 0x0f);
#endif

	if (pIo_buf == NULL) {
		RTW_INFO("[%s] pIo_buf == NULL\n", __FUNCTION__);
		status = -ENOMEM;
		goto release_mutex;
	}

	while (++vendorreq_times <= MAX_USBCTRL_VENDORREQ_TIMES) {
		_rtw_memset(pIo_buf, 0, len);

		if (requesttype == 0x01) {
			pipe = usb_rcvctrlpipe(udev, 0);/* read_in */
			reqtype =  REALTEK_USB_VENQT_READ;
		} else {
			pipe = usb_sndctrlpipe(udev, 0);/* write_out */
			reqtype =  REALTEK_USB_VENQT_WRITE;
			_rtw_memcpy(pIo_buf, pdata, len);
		}

		dbg_rtw_io_record_usb_lock(&io_record_usb_lock, &record_irqL);
		dbg_rtw_io_record_get_and_advance(record_r);
		dbg_rtw_io_record_usb_unlock(&io_record_usb_lock, &record_irqL);
		dbg_rtw_io_record_fill_stime(record_r);
		status = rtw_usb_control_msg(udev, pipe, request, reqtype, value, index, pIo_buf, len, RTW_USB_CONTROL_MSG_TIMEOUT);
		dbg_rtw_io_record_fill_n_usb_io(record_r, reqtype, (u32)value, (u32)len, status, pIo_buf);

		if (status == len) {  /* Success this control transfer. */
			rtw_reset_continual_io_error(pdvobjpriv);
			if (requesttype == 0x01) {
				/* For Control read transfer, we have to copy the read data from pIo_buf to pdata. */
				_rtw_memcpy(pdata, pIo_buf,  len);
			}
		} else { /* error cases */
			switch (len) {
				case 1:
					RTW_INFO("reg 0x%x, usb %s %u fail, status:%d value=0x%x, vendorreq_times:%d\n"
						, value, (requesttype == 0x01) ? "read" : "write" , len, status, *(u8 *)pdata, vendorreq_times);
				break;
				case 2:
					RTW_INFO("reg 0x%x, usb %s %u fail, status:%d value=0x%x, vendorreq_times:%d\n"
						, value, (requesttype == 0x01) ? "read" : "write" , len, status, *(u16 *)pdata, vendorreq_times);
				break;
				case 4:
					RTW_INFO("reg 0x%x, usb %s %u fail, status:%d value=0x%x, vendorreq_times:%d\n"
						, value, (requesttype == 0x01) ? "read" : "write" , len, status, *(u32 *)pdata, vendorreq_times);
				break;
				default:
					RTW_INFO("reg 0x%x, usb %s %u fail, status:%d, vendorreq_times:%d\n"
						, value, (requesttype == 0x01) ? "read" : "write" , len, status, vendorreq_times);
				break;
				
			}

			if (status < 0) {
				if (status == (-ESHUTDOWN)	|| status == -ENODEV)
					rtw_set_surprise_removed(padapter);
				else {
					#ifdef DBG_CONFIG_ERROR_DETECT
					{
						HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);
						pHalData->srestpriv.Wifi_Error_Status = USB_VEN_REQ_CMD_FAIL;
					}
					#endif
				}
			} else { /* status != len && status >= 0 */
				if (status > 0) {
					if (requesttype == 0x01) {
						/* For Control read transfer, we have to copy the read data from pIo_buf to pdata. */
						_rtw_memcpy(pdata, pIo_buf,  len);
					}
				}
			}

			if (rtw_inc_and_chk_continual_io_error(pdvobjpriv) == _TRUE) {
				rtw_set_surprise_removed(padapter);
				break;
			}

		}

		/* firmware download is checksumed, don't retry */
		if ((value >= FW_START_ADDRESS) || status == len)
			break;

	}

#if (defined(CONFIG_RTL8822B) || defined(CONFIG_RTL8821C)) \
	|| defined(CONFIG_RTL8822C) || defined(CONFIG_RTL8822E)
	if (value < 0xFE00) {
		if (value <= 0xff)
			current_reg_sec = REG_ON_SEC;
		else if (0x1000 <= value && value <= 0x10ff)
			current_reg_sec = REG_ON_SEC;
		else
			current_reg_sec = REG_OFF_SEC;
	} else {
		current_reg_sec = REG_LOCAL_SEC;
	}

	if (current_reg_sec == REG_ON_SEC) {
		unsigned int t_pipe = usb_sndctrlpipe(udev, 0);/* write_out */
		u8 t_reqtype =  REALTEK_USB_VENQT_WRITE;
		u8 t_len = 1;
		u8 t_req = 0x05;
		u16 t_reg = 0;
		u16 t_index = 0;

		t_reg = 0x4e0;

		status = rtw_usb_control_msg(udev, t_pipe, t_req, t_reqtype, t_reg, t_index, pIo_buf, t_len, RTW_USB_CONTROL_MSG_TIMEOUT);

		if (status == t_len)
			rtw_reset_continual_io_error(pdvobjpriv);
		else
			RTW_INFO("reg 0x%x, usb %s %u fail, status:%d\n", t_reg, "write" , t_len, status);

	}
#endif

	/* release IO memory used by vendorreq */
#ifdef CONFIG_USB_VENDOR_REQ_BUFFER_DYNAMIC_ALLOCATE
	rtw_mfree(tmp_buf, tmp_buflen);
#endif

release_mutex:
#ifdef CONFIG_USB_VENDOR_REQ_MUTEX
	_exit_critical_mutex(&pdvobjpriv->usb_vendor_req_mutex, NULL);
#endif
exit:
	return status;

}

#ifdef CONFIG_USB_SUPPORT_ASYNC_VDN_REQ
static void _usbctrl_vendorreq_async_callback(struct urb *urb, struct pt_regs *regs)
{
	if (urb) {
		if (urb->context)
			rtw_mfree(urb->context, sizeof(struct rtw_async_write_data));
		usb_free_urb(urb);
	}
}

int _usbctrl_vendorreq_async_write(struct usb_device *udev, u8 request,
	u16 value, u16 index, void *pdata, u16 len, u8 requesttype)
{
	int rc;
	unsigned int pipe;
	u8 reqtype;
	struct usb_ctrlrequest *dr;
	struct urb *urb;
	struct rtw_async_write_data *buf;


	if (requesttype == VENDOR_READ) {
		pipe = usb_rcvctrlpipe(udev, 0);/* read_in */
		reqtype =  REALTEK_USB_VENQT_READ;
	} else {
		pipe = usb_sndctrlpipe(udev, 0);/* write_out */
		reqtype =  REALTEK_USB_VENQT_WRITE;
	}

	buf = (struct rtl819x_async_write_data *)rtw_zmalloc(sizeof(*buf));
	if (!buf) {
		rc = -ENOMEM;
		goto exit;
	}

	urb = usb_alloc_urb(0, GFP_ATOMIC);
	if (!urb) {
		rtw_mfree((u8 *)buf, sizeof(*buf));
		rc = -ENOMEM;
		goto exit;
	}

	dr = &buf->dr;

	dr->bRequestType = reqtype;
	dr->bRequest = request;
	dr->wValue = cpu_to_le16(value);
	dr->wIndex = cpu_to_le16(index);
	dr->wLength = cpu_to_le16(len);

	_rtw_memcpy(buf, pdata, len);

	usb_fill_control_urb(urb, udev, pipe, (unsigned char *)dr, buf, len,
		_usbctrl_vendorreq_async_callback, buf);

	rc = usb_submit_urb(urb, GFP_ATOMIC);
	if (rc < 0) {
		rtw_mfree((u8 *)buf, sizeof(*buf));
		usb_free_urb(urb);
	}

exit:
	return rc;
}


#endif /* CONFIG_USB_SUPPORT_ASYNC_VDN_REQ */

unsigned int ffaddr2pipehdl(struct dvobj_priv *pdvobj, u32 addr)
{
	unsigned int pipe = 0, ep_num = 0;
	struct usb_device *pusbd = pdvobj->pusbdev;

	if (addr == RECV_BULK_IN_ADDR)
		pipe = usb_rcvbulkpipe(pusbd, pdvobj->RtInPipe[0]);

	else if (addr == RECV_INT_IN_ADDR)
		pipe = usb_rcvintpipe(pusbd, pdvobj->RtInPipe[1]);

#ifdef RTW_HALMAC
         /* halmac already translate queue id to bulk out id (addr 0~3) */
		 /* 8814BU bulk out id range is 0~6 */
        else if (addr < MAX_BULKOUT_NUM) {
                ep_num = pdvobj->RtOutPipe[addr];
                pipe = usb_sndbulkpipe(pusbd, ep_num);
        }
#else
        else if (addr < HW_QUEUE_ENTRY) {
                ep_num = pdvobj->Queue2Pipe[addr];
                pipe = usb_sndbulkpipe(pusbd, ep_num);
        }
#endif


	return pipe;
}

struct zero_bulkout_context {
	void *pbuf;
	void *purb;
	void *pirp;
	void *padapter;
};
#if 0
static void usb_bulkout_zero_complete(struct urb *purb, struct pt_regs *regs)
{
	struct zero_bulkout_context *pcontext = (struct zero_bulkout_context *)purb->context;

	/* RTW_INFO("+usb_bulkout_zero_complete\n"); */

	if (pcontext) {
		if (pcontext->pbuf)
			rtw_mfree(pcontext->pbuf, sizeof(int));

		if (pcontext->purb && (pcontext->purb == purb))
			usb_free_urb(pcontext->purb);


		rtw_mfree((u8 *)pcontext, sizeof(struct zero_bulkout_context));
	}


}

static u32 usb_bulkout_zero(struct intf_hdl *pintfhdl, u32 addr)
{
	int pipe, status, len;
	u32 ret;
	unsigned char *pbuf;
	struct zero_bulkout_context *pcontext;
	PURB	purb = NULL;
	_adapter *padapter = (_adapter *)pintfhdl->padapter;
	struct dvobj_priv *pdvobj = adapter_to_dvobj(padapter);
	struct usb_device *pusbd = pdvobj->pusbdev;

	/* RTW_INFO("%s\n", __func__); */


	if (RTW_CANNOT_TX(padapter))
		return _FAIL;


	pcontext = (struct zero_bulkout_context *)rtw_zmalloc(sizeof(struct zero_bulkout_context));
	if (pcontext == NULL)
		return _FAIL;

	pbuf = (unsigned char *)rtw_zmalloc(sizeof(int));
	purb = usb_alloc_urb(0, GFP_ATOMIC);

	/* translate DMA FIFO addr to pipehandle */
	pipe = ffaddr2pipehdl(pdvobj, addr);

	len = 0;
	pcontext->pbuf = pbuf;
	pcontext->purb = purb;
	pcontext->pirp = NULL;
	pcontext->padapter = padapter;


	/* translate DMA FIFO addr to pipehandle */
	/* pipe = ffaddr2pipehdl(pdvobj, addr);	 */

	usb_fill_bulk_urb(purb, pusbd, pipe,
			  pbuf,
			  len,
			  usb_bulkout_zero_complete,
			  pcontext);/* context is pcontext */

	status = usb_submit_urb(purb, GFP_ATOMIC);

	if (!status)
		ret = _SUCCESS;
	else
		ret = _FAIL;


	return _SUCCESS;

}
#endif
void usb_read_mem(struct intf_hdl *pintfhdl, u32 addr, u32 cnt, u8 *rmem)
{

}

void usb_write_mem(struct intf_hdl *pintfhdl, u32 addr, u32 cnt, u8 *wmem)
{

}


void usb_read_port_cancel(struct intf_hdl *pintfhdl)
{
	int i;
	struct recv_buf *precvbuf;
	_adapter	*padapter = pintfhdl->padapter;
	struct registry_priv *regsty = adapter_to_regsty(padapter);
	precvbuf = (struct recv_buf *)padapter->recvpriv.precv_buf;

	RTW_INFO("%s\n", __func__);

	for (i = 0; i < regsty->recvbuf_nr ; i++) {

		if (precvbuf->purb)	 {
			/* RTW_INFO("usb_read_port_cancel : usb_kill_urb\n"); */
			usb_kill_urb(precvbuf->purb);
		}
		precvbuf++;
	}

#ifdef CONFIG_USB_INTERRUPT_IN_PIPE
	usb_kill_urb(padapter->recvpriv.int_in_urb);
#endif
}

static void usb_write_port_complete(struct urb *purb, struct pt_regs *regs)
{
	_irqL irqL;
	struct xmit_buf *pxmitbuf = (struct xmit_buf *)purb->context;
	/* struct xmit_frame *pxmitframe = (struct xmit_frame *)pxmitbuf->priv_data; */
	/* _adapter			*padapter = pxmitframe->padapter; */
	_adapter	*padapter = pxmitbuf->padapter;
	struct xmit_priv	*pxmitpriv = &padapter->xmitpriv;
	/* struct pkt_attrib *pattrib = &pxmitframe->attrib; */
#if defined(CONFIG_SELF_DIAG_INFO) || defined(CONFIG_RTW_IO_RECORDS)
	u8 bulkout_id = 0;

#ifdef RTW_HALMAC
	bulkout_id = pxmitbuf->bulkout_id;
#else
	bulkout_id = pxmitbuf->flags;
#endif
#endif
	switch (pxmitbuf->flags) {
	case VO_QUEUE_INX:
		pxmitpriv->voq_cnt--;
		break;
	case VI_QUEUE_INX:
		pxmitpriv->viq_cnt--;
		break;
	case BE_QUEUE_INX:
		pxmitpriv->beq_cnt--;
		break;
	case BK_QUEUE_INX:
		pxmitpriv->bkq_cnt--;
		break;
	default:
		break;
	}


	/*
		_enter_critical(&pxmitpriv->lock, &irqL);

		pxmitpriv->txirp_cnt--;

		switch(pattrib->priority)
		{
			case 1:
			case 2:
				pxmitpriv->bkq_cnt--;

				break;
			case 4:
			case 5:
				pxmitpriv->viq_cnt--;

				break;
			case 6:
			case 7:
				pxmitpriv->voq_cnt--;

				break;
			case 0:
			case 3:
			default:
				pxmitpriv->beq_cnt--;

				break;

		}

		_exit_critical(&pxmitpriv->lock, &irqL);


		if(pxmitpriv->txirp_cnt==0)
		{
			_rtw_up_sema(&(pxmitpriv->tx_retevt));
		}
	*/
	/* rtw_free_xmitframe(pxmitpriv, pxmitframe); */

	if (RTW_CANNOT_TX(padapter)) {
		RTW_INFO("%s(): TX Warning! bDriverStopped(%s) OR bSurpriseRemoved(%s) pxmitbuf->buf_tag(%x)\n"
			 , __func__
			 , rtw_is_drv_stopped(padapter) ? "True" : "False"
			 , rtw_is_surprise_removed(padapter) ? "True" : "False"
			 , pxmitbuf->buf_tag);

		goto check_completion;
	}


	dbg_rtw_io_record_fill_n_usb_trx(pxmitbuf->record, USB_IO_RECORD_WRITE_PORT, (u32)bulkout_id, purb->actual_length, purb->status);

	if (purb->status == 0) {
#ifdef CONFIG_SELF_DIAG_INFO
		if (bulkout_id < TX_STATS_MAX_NUM)
			padapter->dvobj->trx_stats.tx_complete_cnt[bulkout_id]++;
#endif
	} else {
		RTW_INFO("###=> urb_write_port_complete status(%d)\n", purb->status);
#ifdef CONFIG_SELF_DIAG_INFO
		if (bulkout_id < TX_STATS_MAX_NUM)
			padapter->dvobj->trx_stats.tx_complete_fail[bulkout_id]++;
#endif
		if ((purb->status == -EPIPE) || (purb->status == -EPROTO)) {
			/* usb_clear_halt(pusbdev, purb->pipe);	 */
			/* msleep(10); */
			sreset_set_wifi_error_status(padapter, USB_WRITE_PORT_FAIL);
		} else if (purb->status == -EINPROGRESS) {
			goto check_completion;

		} else if (purb->status == -ENOENT) {
			RTW_INFO("%s: -ENOENT\n", __func__);
			goto check_completion;

		} else if (purb->status == -ECONNRESET) {
			RTW_INFO("%s: -ECONNRESET\n", __func__);
			goto check_completion;

		} else if (purb->status == -ESHUTDOWN) {
			rtw_set_drv_stopped(padapter);

			goto check_completion;
		} else {
			rtw_set_surprise_removed(padapter);
			RTW_INFO("bSurpriseRemoved=TRUE\n");

			goto check_completion;
		}
	}

	#ifdef DBG_CONFIG_ERROR_DETECT
	{
		HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);
		pHalData->srestpriv.last_tx_complete_time = rtw_get_current_time();
	}
	#endif

check_completion:
	_enter_critical(&pxmitpriv->lock_sctx, &irqL);
	rtw_sctx_done_err(&pxmitbuf->sctx,
		purb->status ? RTW_SCTX_DONE_WRITE_PORT_ERR : RTW_SCTX_DONE_SUCCESS);
	_exit_critical(&pxmitpriv->lock_sctx, &irqL);

	rtw_free_xmitbuf(pxmitpriv, pxmitbuf);

	/* if(rtw_txframes_pending(padapter))	 */
	{
		tasklet_hi_schedule(&pxmitpriv->xmit_tasklet);
	}


}

u32 usb_write_port(struct intf_hdl *pintfhdl, u32 addr, u32 cnt, u8 *wmem)
{
	_irqL irqL;
	unsigned int pipe;
	int status;
	u32 ret = _FAIL;
	PURB	purb = NULL;
	_adapter *padapter = (_adapter *)pintfhdl->padapter;
	struct dvobj_priv	*pdvobj = adapter_to_dvobj(padapter);
	struct xmit_priv	*pxmitpriv = &padapter->xmitpriv;
	struct xmit_buf *pxmitbuf = (struct xmit_buf *)wmem;
	struct xmit_frame *pxmitframe = (struct xmit_frame *)pxmitbuf->priv_data;
	struct usb_device *pusbd = pdvobj->pusbdev;
#ifdef CONFIG_SELF_DIAG_INFO
	u8 bulkout_id = 0;
#endif

	DECLARE_IO_RECORD_USB_IRQL(record_irqL);

	if (RTW_CANNOT_TX(padapter)) {
#ifdef DBG_TX
		RTW_INFO(" DBG_TX %s:%d bDriverStopped%s, bSurpriseRemoved:%s\n", __func__, __LINE__
			 , rtw_is_drv_stopped(padapter) ? "True" : "False"
			, rtw_is_surprise_removed(padapter) ? "True" : "False");
#endif
		rtw_sctx_done_err(&pxmitbuf->sctx, RTW_SCTX_DONE_TX_DENY);
		goto exit;
	}

	_enter_critical(&pxmitpriv->lock, &irqL);

	switch (addr) {
	case VO_QUEUE_INX:
		pxmitpriv->voq_cnt++;
		pxmitbuf->flags = VO_QUEUE_INX;
		break;
	case VI_QUEUE_INX:
		pxmitpriv->viq_cnt++;
		pxmitbuf->flags = VI_QUEUE_INX;
		break;
	case BE_QUEUE_INX:
		pxmitpriv->beq_cnt++;
		pxmitbuf->flags = BE_QUEUE_INX;
		break;
	case BK_QUEUE_INX:
		pxmitpriv->bkq_cnt++;
		pxmitbuf->flags = BK_QUEUE_INX;
		break;
	case HIGH_QUEUE_INX:
		pxmitbuf->flags = HIGH_QUEUE_INX;
		break;
	default:
		pxmitbuf->flags = MGT_QUEUE_INX;
		break;
	}

	_exit_critical(&pxmitpriv->lock, &irqL);

	purb	= pxmitbuf->pxmit_urb[0];

	/* translate DMA FIFO addr to pipehandle */
#ifdef RTW_HALMAC
#ifdef CONFIG_SELF_DIAG_INFO
	bulkout_id = pxmitbuf->bulkout_id;
#endif
	pipe = ffaddr2pipehdl(pdvobj, pxmitbuf->bulkout_id);
#else
#ifdef CONFIG_SELF_DIAG_INFO
	bulkout_id= addr
#endif
	pipe = ffaddr2pipehdl(pdvobj, addr);
#endif

#ifdef CONFIG_REDUCE_USB_TX_INT
	if ((pxmitpriv->free_xmitbuf_cnt % NR_XMITBUFF == 0)
	    || (pxmitbuf->buf_tag > XMITBUF_DATA))
		purb->transfer_flags  &= (~URB_NO_INTERRUPT);
	else {
		purb->transfer_flags  |=  URB_NO_INTERRUPT;
		/* RTW_INFO("URB_NO_INTERRUPT "); */
	}
#endif


	usb_fill_bulk_urb(purb, pusbd, pipe,
			  pxmitframe->buf_addr, /* = pxmitbuf->pbuf */
			  cnt,
			  usb_write_port_complete,
			  pxmitbuf);/* context is pxmitbuf */

#ifdef CONFIG_USE_USB_BUFFER_ALLOC_TX
	purb->transfer_dma = pxmitbuf->dma_transfer_addr;
	purb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
	purb->transfer_flags |= URB_ZERO_PACKET;
#endif /* CONFIG_USE_USB_BUFFER_ALLOC_TX */

#ifdef USB_PACKET_OFFSET_SZ
#if (USB_PACKET_OFFSET_SZ == 0)
	purb->transfer_flags |= URB_ZERO_PACKET;
#endif
#endif

#if 0
	if (bwritezero)
		purb->transfer_flags |= URB_ZERO_PACKET;
#endif

	dbg_rtw_io_record_usb_lock(&io_record_usb_lock, &record_irqL);
	dbg_rtw_io_record_get_and_advance(pxmitbuf->record);
	dbg_rtw_io_record_usb_unlock(&io_record_usb_lock, &record_irqL);
	dbg_rtw_io_record_fill_stime(pxmitbuf->record);

	status = usb_submit_urb(purb, GFP_ATOMIC);
	if (!status) {
#ifdef CONFIG_SELF_DIAG_INFO
		if (bulkout_id < TX_STATS_MAX_NUM)
			pdvobj->trx_stats.tx_submit_cnt[bulkout_id]++;
#endif
		#ifdef DBG_CONFIG_ERROR_DETECT
		{
			HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);
			pHalData->srestpriv.last_tx_time = rtw_get_current_time();
		}
		#endif
	} else {
#ifdef CONFIG_SELF_DIAG_INFO
		if (bulkout_id < TX_STATS_MAX_NUM)
			pdvobj->trx_stats.tx_submit_fail[bulkout_id]++;
#endif
		rtw_sctx_done_err(&pxmitbuf->sctx, RTW_SCTX_DONE_WRITE_PORT_ERR);
		RTW_INFO("usb_write_port, status=%d\n", status);

		switch (status) {
		case -ENODEV:
			rtw_set_drv_stopped(padapter);
			break;
		default:
			break;
		}
		goto exit;
	}

	ret = _SUCCESS;

	/* Commented by Albert 2009/10/13
	 * We add the URB_ZERO_PACKET flag to urb so that the host will send the zero packet automatically. */
	/*
		if(bwritezero == _TRUE)
		{
			usb_bulkout_zero(pintfhdl, addr);
		}
	*/


exit:
	if (ret != _SUCCESS)
		rtw_free_xmitbuf(pxmitpriv, pxmitbuf);
	return ret;

}

void usb_write_port_cancel(struct intf_hdl *pintfhdl)
{
	int i, j;
	_adapter	*padapter = pintfhdl->padapter;
	struct xmit_buf *pxmitbuf = (struct xmit_buf *)padapter->xmitpriv.pxmitbuf;

	RTW_INFO("%s\n", __func__);

	for (i = 0; i < NR_XMITBUFF; i++) {
		for (j = 0; j < 8; j++) {
			if (pxmitbuf->pxmit_urb[j])
				usb_kill_urb(pxmitbuf->pxmit_urb[j]);
		}
		pxmitbuf++;
	}

	pxmitbuf = (struct xmit_buf *)padapter->xmitpriv.pxmit_extbuf;
	for (i = 0; i < NR_XMIT_EXTBUFF ; i++) {
		for (j = 0; j < 8; j++) {
			if (pxmitbuf->pxmit_urb[j])
				usb_kill_urb(pxmitbuf->pxmit_urb[j]);
		}
		pxmitbuf++;
	}
}

void usb_init_recvbuf(_adapter *padapter, struct recv_buf *precvbuf)
{

	precvbuf->transfer_len = 0;

	precvbuf->len = 0;

	precvbuf->ref_cnt = 0;

	if (precvbuf->pbuf) {
		precvbuf->pdata = precvbuf->phead = precvbuf->ptail = precvbuf->pbuf;
		precvbuf->pend = precvbuf->pdata + MAX_RECVBUF_SZ;
	}

}

int recvbuf2recvframe(PADAPTER padapter, void *ptr);

#ifdef CONFIG_USE_USB_BUFFER_ALLOC_RX
void usb_recv_tasklet(unsigned long priv)
{
	struct recv_buf *precvbuf = NULL;
	_adapter	*padapter = (_adapter *)priv;
	struct recv_priv	*precvpriv = &padapter->recvpriv;

	while (NULL != (precvbuf = rtw_dequeue_recvbuf(&precvpriv->recv_buf_pending_queue))) {
		if (RTW_CANNOT_RUN(padapter)) {
			RTW_INFO("recv_tasklet => bDriverStopped(%s) OR bSurpriseRemoved(%s)\n"
				, rtw_is_drv_stopped(padapter)? "True" : "False"
				, rtw_is_surprise_removed(padapter)? "True" : "False");
			break;
		}

		recvbuf2recvframe(padapter, precvbuf);

		rtw_read_port(padapter, precvpriv->ff_hwaddr, 0, (unsigned char *)precvbuf);
	}
}

void usb_read_port_complete(struct urb *purb, struct pt_regs *regs)
{
	struct recv_buf	*precvbuf = (struct recv_buf *)purb->context;
	_adapter			*padapter = (_adapter *)precvbuf->adapter;
	struct recv_priv	*precvpriv = &padapter->recvpriv;

	DECLARE_DBG_RTW_IO_RECORD_P(record_r);
	DECLARE_IO_RECORD_USB_IRQL(record_irqL);

	ATOMIC_DEC(&(precvpriv->rx_pending_cnt));

	if (RTW_CANNOT_RX(padapter)) {
		RTW_INFO("%s() RX Warning! bDriverStopped(%s) OR bSurpriseRemoved(%s)\n"
			 , __func__
			 , rtw_is_drv_stopped(padapter) ? "True" : "False"
			, rtw_is_surprise_removed(padapter) ? "True" : "False");
		return;
	}

	dbg_rtw_io_record_usb_lock(&io_record_usb_lock, &record_irqL);
	dbg_rtw_io_record_get_and_advance(record_r);
	dbg_rtw_io_record_usb_unlock(&io_record_usb_lock, &record_irqL);
	dbg_rtw_io_record_fill_n_usb_trx(record_r, USB_IO_RECORD_READ_PORT, (u32)precvbuf->bulkin_id, purb->actual_length, purb->status);

	if (purb->status == 0) {
#ifdef CONFIG_SELF_DIAG_INFO
		if (precvbuf->bulkin_id < RX_STATS_MAX_NUM)
			padapter->dvobj->trx_stats.rx_complete_cnt[precvbuf->bulkin_id]++;
#endif
		if ((purb->actual_length > MAX_RECVBUF_SZ) || (purb->actual_length < RXDESC_SIZE)) {
			RTW_INFO("%s()-%d: urb->actual_length:%u, MAX_RECVBUF_SZ:%u, RXDESC_SIZE:%u\n"
				, __FUNCTION__, __LINE__, purb->actual_length, MAX_RECVBUF_SZ, RXDESC_SIZE);
			rtw_read_port(padapter, precvpriv->ff_hwaddr, 0, (unsigned char *)precvbuf);
		} else {
			rtw_reset_continual_io_error(adapter_to_dvobj(padapter));

			precvbuf->transfer_len = purb->actual_length;

			rtw_enqueue_recvbuf(precvbuf, &precvpriv->recv_buf_pending_queue);

			tasklet_schedule(&precvpriv->recv_tasklet);
		}
	} else {

		RTW_INFO("###=> usb_read_port_complete => urb.status(%d)\n", purb->status);
#ifdef CONFIG_SELF_DIAG_INFO
		if (precvbuf->bulkin_id < RX_STATS_MAX_NUM)
			padapter->dvobj->trx_stats.rx_complete_fail[precvbuf->bulkin_id]++;
#endif
		if (rtw_inc_and_chk_continual_io_error(adapter_to_dvobj(padapter)) == _TRUE)
			rtw_set_surprise_removed(padapter);

		switch (purb->status) {
		case -EINVAL:
		case -EPIPE:
		case -ENODEV:
		case -ESHUTDOWN:
		case -ENOENT:
			rtw_set_drv_stopped(padapter);
			break;
		case -EPROTO:
		case -EILSEQ:
		case -ETIME:
		case -ECOMM:
		case -EOVERFLOW:
			#ifdef DBG_CONFIG_ERROR_DETECT
			{
				HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);
				pHalData->srestpriv.Wifi_Error_Status = USB_READ_PORT_FAIL;
			}
			#endif
			rtw_read_port(padapter, precvpriv->ff_hwaddr, 0, (unsigned char *)precvbuf);
			break;
		case -EINPROGRESS:
			RTW_INFO("ERROR: URB IS IN PROGRESS!/n");
			break;
		default:
			break;
		}
	}

}

u32 usb_read_port(struct intf_hdl *pintfhdl, u32 addr, u32 cnt, u8 *rmem)
{
	int err;
	unsigned int pipe;
	u32 ret = _SUCCESS;
	PURB purb = NULL;
	struct recv_buf	*precvbuf = (struct recv_buf *)rmem;
	_adapter		*adapter = pintfhdl->padapter;
	struct dvobj_priv	*pdvobj = adapter_to_dvobj(adapter);
	struct pwrctrl_priv *pwrctl = dvobj_to_pwrctl(pdvobj);
	struct recv_priv	*precvpriv = &adapter->recvpriv;
	struct usb_device	*pusbd = pdvobj->pusbdev;


	if (RTW_CANNOT_RX(adapter) || (precvbuf == NULL)) {
		return _FAIL;
	}

	usb_init_recvbuf(adapter, precvbuf);
#if defined(CONFIG_SELF_DIAG_INFO) || defined(CONFIG_RTW_IO_RECORDS)
	precvbuf->bulkin_id = addr & 0x1;
#endif

	if (precvbuf->pbuf) {
		ATOMIC_INC(&(precvpriv->rx_pending_cnt));
		purb = precvbuf->purb;

		/* translate DMA FIFO addr to pipehandle */
		pipe = ffaddr2pipehdl(pdvobj, addr);

		usb_fill_bulk_urb(purb, pusbd, pipe,
			precvbuf->pbuf,
			MAX_RECVBUF_SZ,
			usb_read_port_complete,
			precvbuf);/* context is precvbuf */

		purb->transfer_dma = precvbuf->dma_transfer_addr;
		purb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

		err = usb_submit_urb(purb, GFP_ATOMIC);
		if ((err) && (err != (-EPERM))) {
			RTW_INFO("cannot submit rx in-token(err = 0x%08x),urb_status = %d\n", err, purb->status);
#ifdef CONFIG_SELF_DIAG_INFO
			if (precvbuf->bulkin_id < RX_STATS_MAX_NUM)
				pdvobj->trx_stats.rx_submit_fail[precvbuf->bulkin_id]++;
#endif
			ret = _FAIL;
		}
#ifdef CONFIG_SELF_DIAG_INFO
		else {
			if (precvbuf->bulkin_id < RX_STATS_MAX_NUM)
				pdvobj->trx_stats.rx_submit_cnt[precvbuf->bulkin_id]++;
		}
#endif

	}


	return ret;
}
#else	/* CONFIG_USE_USB_BUFFER_ALLOC_RX */

void usb_recv_tasklet(unsigned long priv)
{
	_pkt			*pskb;
	_adapter		*padapter = (_adapter *)priv;
	struct recv_priv	*precvpriv = &padapter->recvpriv;
	struct recv_buf	*precvbuf = NULL;
#ifdef CONFIG_USB_PROTECT_RX_CLONED_SKB
	u8 cloned_skb_num;
	u8 need_sche = _FALSE;
#endif

	while (NULL != (pskb = skb_dequeue(&precvpriv->rx_skb_queue))) {

		if (RTW_CANNOT_RUN(padapter)) {
			RTW_INFO("recv_tasklet => bDriverStopped(%s) OR bSurpriseRemoved(%s)\n"
				, rtw_is_drv_stopped(padapter) ? "True" : "False"
				, rtw_is_surprise_removed(padapter) ? "True" : "False");
			#ifdef CONFIG_PREALLOC_RX_SKB_BUFFER
			if (rtw_free_skb_premem(pskb) != 0)
			#endif /* CONFIG_PREALLOC_RX_SKB_BUFFER */
			{
				skb_reset_tail_pointer(pskb);
				pskb->len = 0;
				skb_queue_tail(&precvpriv->free_recv_skb_queue, pskb);
			}
			continue;
		}

		recvbuf2recvframe(padapter, pskb);

#ifdef CONFIG_USB_PROTECT_RX_CLONED_SKB
		if (skb_cloned(pskb)) {
			skb_queue_tail(&precvpriv->rx_cloned_skb_queue, pskb);
			need_sche = _TRUE;
		} else
#endif
		{
			skb_reset_tail_pointer(pskb);
			pskb->len = 0;
			skb_queue_tail(&precvpriv->free_recv_skb_queue, pskb);

			precvbuf = rtw_dequeue_recvbuf(&precvpriv->recv_buf_pending_queue);
			if (NULL != precvbuf) {
				precvbuf->pskb = NULL;
				rtw_read_port(padapter, precvpriv->ff_hwaddr, 0, (unsigned char *)precvbuf);
			}
		}
	}

#ifdef CONFIG_USB_PROTECT_RX_CLONED_SKB
	cloned_skb_num = skb_queue_len(&precvpriv->rx_cloned_skb_queue);
	while (cloned_skb_num--) {
		pskb = skb_dequeue(&precvpriv->rx_cloned_skb_queue);
		if (pskb) {
			if (skb_cloned(pskb)) {
				skb_queue_tail(&precvpriv->rx_cloned_skb_queue, pskb);
				need_sche = _TRUE;
			} else {
				skb_reset_tail_pointer(pskb);
				pskb->len = 0;
				skb_queue_tail(&precvpriv->free_recv_skb_queue, pskb);

				precvbuf = rtw_dequeue_recvbuf(&precvpriv->recv_buf_pending_queue);
				if (NULL != precvbuf) {
					precvbuf->pskb = NULL;
					rtw_read_port(padapter, precvpriv->ff_hwaddr, 0, (unsigned char *)precvbuf);
				}
			}
		}
	}

	if (need_sche)
		tasklet_schedule(&precvpriv->recv_tasklet);
#endif
	if (RTW_CANNOT_RUN(padapter)) {
			while (rtw_dequeue_recvbuf(&precvpriv->recv_buf_pending_queue));
	}
}

void usb_read_port_complete(struct urb *purb, struct pt_regs *regs)
{
	struct recv_buf	*precvbuf = (struct recv_buf *)purb->context;
	_adapter			*padapter = (_adapter *)precvbuf->adapter;
	struct recv_priv	*precvpriv = &padapter->recvpriv;

	DECLARE_DBG_RTW_IO_RECORD_P(record_r);
	DECLARE_IO_RECORD_USB_IRQL(record_irqL);

	ATOMIC_DEC(&(precvpriv->rx_pending_cnt));

	if (RTW_CANNOT_RX(padapter)) {
		RTW_INFO("%s() RX Warning! bDriverStopped(%s) OR bSurpriseRemoved(%s)\n"
			, __func__
			, rtw_is_drv_stopped(padapter) ? "True" : "False"
			, rtw_is_surprise_removed(padapter) ? "True" : "False");
		goto exit;
	}

	dbg_rtw_io_record_usb_lock(&io_record_usb_lock, &record_irqL);
	dbg_rtw_io_record_get_and_advance(record_r);
	dbg_rtw_io_record_usb_unlock(&io_record_usb_lock, &record_irqL);
	dbg_rtw_io_record_fill_n_usb_trx(record_r, USB_IO_RECORD_READ_PORT, (u32)precvbuf->bulkin_id, purb->actual_length, purb->status);

	if (purb->status == 0) {
#ifdef CONFIG_SELF_DIAG_INFO
		if (precvbuf->bulkin_id < RX_STATS_MAX_NUM)
			padapter->dvobj->trx_stats.rx_complete_cnt[precvbuf->bulkin_id]++;
#endif
		if ((purb->actual_length > MAX_RECVBUF_SZ) || (purb->actual_length < RXDESC_SIZE)) {
			RTW_INFO("%s()-%d: urb->actual_length:%u, MAX_RECVBUF_SZ:%u, RXDESC_SIZE:%u\n"
				, __FUNCTION__, __LINE__, purb->actual_length, MAX_RECVBUF_SZ, RXDESC_SIZE);
			rtw_read_port(padapter, precvpriv->ff_hwaddr, 0, (unsigned char *)precvbuf);
		} else {
			rtw_reset_continual_io_error(adapter_to_dvobj(padapter));

			precvbuf->transfer_len = purb->actual_length;
			skb_put(precvbuf->pskb, purb->actual_length);
			skb_queue_tail(&precvpriv->rx_skb_queue, precvbuf->pskb);

			#ifndef CONFIG_FIX_NR_BULKIN_BUFFER
			if (skb_queue_len(&precvpriv->rx_skb_queue) <= 1)
			#endif
				tasklet_schedule(&precvpriv->recv_tasklet);

			precvbuf->pskb = NULL;
			rtw_read_port(padapter, precvpriv->ff_hwaddr, 0, (unsigned char *)precvbuf);
		}
	} else {

		RTW_INFO("###=> usb_read_port_complete => urb.status(%d)\n", purb->status);
#ifdef CONFIG_SELF_DIAG_INFO
		if (precvbuf->bulkin_id < RX_STATS_MAX_NUM)
			padapter->dvobj->trx_stats.rx_complete_fail[precvbuf->bulkin_id]++;
#endif
		if (rtw_inc_and_chk_continual_io_error(adapter_to_dvobj(padapter)) == _TRUE)
			rtw_set_surprise_removed(padapter);

		switch (purb->status) {
		case -EINVAL:
		case -EPIPE:
		case -ENODEV:
		case -ESHUTDOWN:
		case -ENOENT:
			rtw_set_drv_stopped(padapter);
			break;
		case -EPROTO:
		case -EILSEQ:
		case -ETIME:
		case -ECOMM:
		case -EOVERFLOW:
			#ifdef DBG_CONFIG_ERROR_DETECT
			{
				HAL_DATA_TYPE	*pHalData = GET_HAL_DATA(padapter);
				pHalData->srestpriv.Wifi_Error_Status = USB_READ_PORT_FAIL;
			}
			#endif
			rtw_read_port(padapter, precvpriv->ff_hwaddr, 0, (unsigned char *)precvbuf);
			break;
		case -EINPROGRESS:
			RTW_INFO("ERROR: URB IS IN PROGRESS!/n");
			break;
		default:
			break;
		}
	}

exit:
	return;
}

u32 usb_read_port(struct intf_hdl *pintfhdl, u32 addr, u32 cnt, u8 *rmem)
{
	int err;
	unsigned int pipe;
	u32 ret = _FAIL;
	PURB purb = NULL;
	struct recv_buf	*precvbuf = (struct recv_buf *)rmem;
	_adapter		*adapter = pintfhdl->padapter;
	struct dvobj_priv	*pdvobj = adapter_to_dvobj(adapter);
	struct recv_priv	*precvpriv = &adapter->recvpriv;
	struct usb_device	*pusbd = pdvobj->pusbdev;


	if (RTW_CANNOT_RX(adapter) || (precvbuf == NULL)) {
		goto exit;
	}

	usb_init_recvbuf(adapter, precvbuf);
#if defined(CONFIG_SELF_DIAG_INFO) || defined(CONFIG_RTW_IO_RECORDS)
	precvbuf->bulkin_id = addr & 0x1;
#endif

	if (precvbuf->pskb == NULL) {
		SIZE_PTR tmpaddr = 0;
		SIZE_PTR alignment = 0;

		precvbuf->pskb = skb_dequeue(&precvpriv->free_recv_skb_queue);
		if (NULL != precvbuf->pskb)
			goto recv_buf_hook;

		#ifndef CONFIG_FIX_NR_BULKIN_BUFFER
		precvbuf->pskb = rtw_skb_alloc(MAX_RECVBUF_SZ + RECVBUFF_ALIGN_SZ);
		#endif

		if (precvbuf->pskb == NULL) {
			if (0)
				RTW_INFO("usb_read_port() enqueue precvbuf=%p\n", precvbuf);
			/* enqueue precvbuf and wait for free skb */
			rtw_enqueue_recvbuf(precvbuf, &precvpriv->recv_buf_pending_queue);
			goto exit;
		}

		tmpaddr = (SIZE_PTR)precvbuf->pskb->data;
		alignment = tmpaddr & (RECVBUFF_ALIGN_SZ - 1);
		skb_reserve(precvbuf->pskb, (RECVBUFF_ALIGN_SZ - alignment));
	}

recv_buf_hook:
	precvbuf->phead = precvbuf->pskb->head;
	precvbuf->pdata = precvbuf->pskb->data;
	precvbuf->ptail = skb_tail_pointer(precvbuf->pskb);
	precvbuf->pend = skb_end_pointer(precvbuf->pskb);
	precvbuf->pbuf = precvbuf->pskb->data;

	purb = precvbuf->purb;

	/* translate DMA FIFO addr to pipehandle */
	pipe = ffaddr2pipehdl(pdvobj, addr);

	usb_fill_bulk_urb(purb, pusbd, pipe,
		precvbuf->pbuf,
		MAX_RECVBUF_SZ,
		usb_read_port_complete,
		precvbuf);

	err = usb_submit_urb(purb, GFP_ATOMIC);
	if (err && err != (-EPERM)) {
		RTW_INFO("cannot submit rx in-token(err = 0x%08x),urb_status = %d\n"
			, err, purb->status);
#ifdef CONFIG_SELF_DIAG_INFO
		if (precvbuf->bulkin_id < RX_STATS_MAX_NUM)
			pdvobj->trx_stats.rx_submit_fail[precvbuf->bulkin_id]++;
#endif
		goto exit;
	}
#ifdef CONFIG_SELF_DIAG_INFO
	else {
		if (precvbuf->bulkin_id < RX_STATS_MAX_NUM)
			pdvobj->trx_stats.rx_submit_cnt[precvbuf->bulkin_id]++;
	}
#endif

	ATOMIC_INC(&(precvpriv->rx_pending_cnt));
	ret = _SUCCESS;

exit:


	return ret;
}
#endif /* CONFIG_USE_USB_BUFFER_ALLOC_RX */

#ifdef CONFIG_USB_INTERRUPT_IN_PIPE
void usb_read_interrupt_complete(struct urb *purb, struct pt_regs *regs)
{
	int	err;
	_adapter	*padapter = (_adapter *)purb->context;

	if (RTW_CANNOT_RX(padapter)) {
		RTW_INFO("%s() RX Warning! bDriverStopped(%s) OR bSurpriseRemoved(%s)\n"
			, __func__
			, rtw_is_drv_stopped(padapter) ? "True" : "False"
			, rtw_is_surprise_removed(padapter) ? "True" : "False");

		return;
	}

	if (purb->status == 0) {/*SUCCESS*/
		if (purb->actual_length > INTERRUPT_MSG_FORMAT_LEN)
			RTW_INFO("usb_read_interrupt_complete: purb->actual_length > INTERRUPT_MSG_FORMAT_LEN(%d)\n", INTERRUPT_MSG_FORMAT_LEN);

		rtw_hal_interrupt_handler(padapter, purb->actual_length, purb->transfer_buffer);

		err = usb_submit_urb(purb, GFP_ATOMIC);
		if ((err) && (err != (-EPERM)))
			RTW_INFO("cannot submit interrupt in-token(err = 0x%08x),urb_status = %d\n", err, purb->status);
	} else {
		RTW_INFO("###=> usb_read_interrupt_complete => urb status(%d)\n", purb->status);

		switch (purb->status) {
		case -EINVAL:
		case -EPIPE:
		case -ENODEV:
		case -ESHUTDOWN:
		case -ENOENT:
			rtw_set_drv_stopped(padapter);
			break;
		case -EPROTO:
			break;
		case -EINPROGRESS:
			RTW_INFO("ERROR: URB IS IN PROGRESS!/n");
			break;
		default:
			break;
		}
	}
}

u32 usb_read_interrupt(struct intf_hdl *pintfhdl, u32 addr)
{
	int	err;
	unsigned int pipe;
	u32	ret = _SUCCESS;
	_adapter			*adapter = pintfhdl->padapter;
	struct dvobj_priv	*pdvobj = adapter_to_dvobj(adapter);
	struct recv_priv	*precvpriv = &adapter->recvpriv;
	struct usb_device	*pusbd = pdvobj->pusbdev;


	if (RTW_CANNOT_RX(adapter)) {
		return _FAIL;
	}

	/*translate DMA FIFO addr to pipehandle*/
	pipe = ffaddr2pipehdl(pdvobj, addr);

	usb_fill_int_urb(precvpriv->int_in_urb, pusbd, pipe,
			precvpriv->int_in_buf,
			INTERRUPT_MSG_FORMAT_LEN,
			usb_read_interrupt_complete,
			adapter,
			1);

	err = usb_submit_urb(precvpriv->int_in_urb, GFP_ATOMIC);
	if ((err) && (err != (-EPERM))) {
		RTW_INFO("cannot submit interrupt in-token(err = 0x%08x), urb_status = %d\n", err, precvpriv->int_in_urb->status);
		ret = _FAIL;
	}

	return ret;
}
#endif /* CONFIG_USB_INTERRUPT_IN_PIPE */

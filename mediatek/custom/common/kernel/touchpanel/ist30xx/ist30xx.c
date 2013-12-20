/*
 *  Copyright (C) 2010,Imagis Technology Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include <cust_eint.h>
#include <linux/rtpm_prio.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>

#include "ist30xx.h"
#include "ist30xx_update.h"

#if IST30XX_DEBUG
#include "ist30xx_misc.h"
#endif

#if IST30XX_TRACKING_MODE
#include "ist30xx_tracking.h"
#endif

#include "tpd.h"
#include "cust_gpio_usage.h"

#define MAX_ERR_CNT             (100)

struct task_struct *thread = NULL;

static int tpd_flag = 0;
static int is_key_pressed = 0;
static int pressed_keycode = 0;
#define CANCEL_KEY 0xff

#ifdef TPD_HAVE_BUTTON
static int tpd_keys_local[TPD_KEY_COUNT] = TPD_KEYS;
static int tpd_keys_dim_local[TPD_KEY_COUNT][4] = TPD_KEYS_DIM;
#endif

static DECLARE_WAIT_QUEUE_HEAD ( waiter );

DEFINE_MUTEX ( ist30xx_mutex );

#if IST30XX_DETECT_TA
static int ist30xx_ta_status = -1;
#endif

static bool ist30xx_initialized = 0;
struct ist30xx_data *ts_data;
static struct delayed_work work_reset_check;
#if IST30XX_INTERNAL_BIN && IST30XX_UPDATE_BY_WORKQUEUE
static struct delayed_work work_fw_update;
#endif

static void clear_input_data(struct ist30xx_data *data);


#if IST30XX_EVENT_MODE
bool get_event_mode = true;

static struct timer_list idle_timer;
static struct timespec t_event, t_current;      // ns
#define EVENT_TIMER_INTERVAL     (HZ / 2)       // 0.5sec

#if IST30XX_NOISE_MODE
#define IST30XX_IDLE_STATUS     (0x1D4E0000)
#define IDLE_ALGORITHM_MODE     (1U << 15)
#endif // IST30XX_NOISE_MODE

#endif  // IST30XX_EVENT_MODE

static const struct i2c_device_id tpd_id[] = { { "ist30xx", 0 }, {} };
unsigned short force[] = {
	TPD_I2C_NUMBER, TPD_I2C_ADDR, I2C_CLIENT_END, I2C_CLIENT_END
};
static const unsigned short *const forces[] = { force, NULL };
//jin static struct i2c_client_address_data addr_data = { .forces = forces, };

static struct i2c_board_info __initdata ist30xx_i2c_tpd = { I2C_BOARD_INFO ( "ist30xx", ( 0xA0 >> 1 ) ) };

extern struct tpd_device *tpd;

extern void mt65xx_eint_mask ( unsigned int line );
extern void mt65xx_eint_unmask ( unsigned int line );
extern kal_uint32 mt65xx_eint_set_sens ( kal_uint8 eintno, kal_bool sens );
extern void mt65xx_eint_set_hw_debounce ( kal_uint8 eintno, kal_uint32 ms );
extern void mt65xx_eint_registration ( kal_uint8 eintno, kal_bool Dbounce_En, kal_bool ACT_Polarity, void ( EINT_FUNC_PTR ) ( void ), kal_bool auto_umask );


void ist30xx_disable_irq ( struct ist30xx_data *data )
{
	if ( data->irq_enabled )
	{
		mt65xx_eint_mask ( CUST_EINT_TOUCH_PANEL_NUM );
		data->irq_enabled = 0;
	}
}

void ist30xx_enable_irq ( struct ist30xx_data *data )
{
	if ( !data->irq_enabled )
	{
		mt65xx_eint_unmask ( CUST_EINT_TOUCH_PANEL_NUM );
		msleep ( 50 );
		data->irq_enabled = 1;
	}
}


int ist30xx_max_error_cnt = MAX_ERR_CNT;
int ist30xx_error_cnt = 0;
static void ist30xx_request_reset ( void )
{
	TPD_FUN ();
	
	ist30xx_error_cnt++;
	if ( ist30xx_error_cnt >= ist30xx_max_error_cnt )
	{
		schedule_delayed_work ( &work_reset_check, 0 );
		TPD_LOG ( "ist30xx_request_reset!\n" );
		ist30xx_error_cnt = 0;
	}
}

void ist30xx_start ( struct ist30xx_data *data )
{
	TPD_FUN ();
#if IST30XX_DETECT_TA
	if ( ist30xx_ta_status > -1 )
	{
		ist30xx_write_cmd ( data->client, CMD_SET_TA_MODE, ist30xx_ta_status );

		TPD_LOG ( "ist30xx_start, ta_mode : %d\n", ist30xx_ta_status );
	}
#endif

	ist30xx_cmd_start_scan ( data->client );

#if IST30XX_EVENT_MODE
	if ((data->status.update != 1) && (data->status.calib != 1))
		ktime_get_ts(&t_event);
#endif
}


int ist30xx_get_ver_info(struct ist30xx_data *data)
{
	int ret;

	data->fw.pre_ver = data->fw.ver;
	data->fw.ver = 0;

	ret = ist30xx_read_cmd ( data->client, CMD_GET_CHIP_ID, &data->chip_id );
	if ( ret )
		return -EIO;

	ret = ist30xx_read_cmd ( data->client, CMD_GET_FW_VER, &data->fw.ver );
	if ( ret )
		return -EIO;

	ret = ist30xx_read_cmd ( data->client, CMD_GET_PARAM_VER, &data->param_ver );
	if ( ret )
		return -EIO;

	TPD_LOG ( "Chip ID: %x, Core: %x, F/W: %x\n", data->chip_id, data->fw.ver, data->param_ver );

	if ( ( data->chip_id != IST30XX_CHIP_ID ) && ( data->chip_id != IST30XXA_CHIP_ID ) )
		return -EPERM;

	return 0;
}

int ist30xx_init_touch_driver ( struct ist30xx_data *data )
{
	TPD_FUN ();
	int ret = 0;

	mutex_lock(&ist30xx_mutex);
	ist30xx_disable_irq ( data );

	ret = ist30xx_cmd_run_device ( data->client );
	if ( ret )
		goto init_touch_end;

	ret = ist30xx_get_ver_info ( data );
	if ( ret )
		goto init_touch_end;

init_touch_end:
	ist30xx_start ( data );

	ist30xx_enable_irq ( data );
	mutex_unlock(&ist30xx_mutex);

	return ret;
}


#define CALIB_MSG_MASK          (0xF0000FFF)
#define CALIB_MSG_VALID         (0x80000CAB)
int ist30xx_get_info(struct ist30xx_data *data)
{
	int ret;
	u32 calib_msg;

	mutex_lock(&ist30xx_mutex);
	ist30xx_disable_irq(data);

	ret = ist30xx_cmd_run_device(data->client);
	if (ret) goto get_info_end;

#if !(IST30XX_INTERNAL_BIN)
	ret = ist30xx_get_ver_info(data);
	if (ret) goto get_info_end;
#endif  // !(IST30XX_INTERNAL_BIN)

#if IST30XX_DEBUG
# if IST30XX_INTERNAL_BIN
	ist30xx_get_tsp_info();
	ist30xx_get_tkey_info();
# else
	ret = ist30xx_tsp_update_info();
	if (ret) goto get_info_end;

	ret = ist30xx_tkey_update_info();
	if (ret) goto get_info_end;
# endif
#endif  // IST30XX_DEBUG

	ret = ist30xx_read_cmd(ts_data->client, CMD_GET_CALIB_RESULT, &calib_msg);
	if (ret == 0)
	{
		TPD_LOG("[ TSP ] calib status: 0x%08x\n", calib_msg);
		if ((calib_msg & CALIB_MSG_MASK) != CALIB_MSG_VALID || CALIB_TO_STATUS(calib_msg) > 0)
		{
			ist30xx_calibrate(1);

			ist30xx_cmd_run_device(data->client);
		}
	}

	ist30xx_start(ts_data);

#if IST30XX_EVENT_MODE
	ktime_get_ts(&t_event);
#endif

	data->status.calib = 0;

get_info_end:
	if (ret == 0)
		ist30xx_enable_irq(data);
	mutex_unlock(&ist30xx_mutex);

	return ret;
}


#define PRESS_MSG_MASK          (0x01)
#define MULTI_MSG_MASK          (0x02)
#define PRESS_MSG_KEY           (0x6)

#define TOUCH_DOWN_MESSAGE      ("Touch down")
#define TOUCH_UP_MESSAGE        ("Touch up  ")
#define TOUCH_MOVE_MESSAGE      ("          ")
bool tsp_touched[IST30XX_MAX_MT_FINGERS] = { 0, };

void print_tsp_event(finger_info *finger)
{
#if PRINT_TOUCH_EVENT
	int idx = finger->bit_field.id - 1;
	int press = finger->bit_field.udmg & PRESS_MSG_MASK;

	if (press == PRESS_MSG_MASK) {
		if (tsp_touched[idx] == 0) { // touch down
			TPD_LOG("[ TSP ] %s - %d (%d, %d)\n", TOUCH_DOWN_MESSAGE,
				finger->bit_field.id,
				finger->bit_field.x, finger->bit_field.y);
			tsp_touched[idx] = 1;
		} else {              // touch move
			TPD_LOG("[ TSP ] %s - %d (%d,%d)\n", TOUCH_MOVE_MESSAGE,
				finger->bit_field.id,
				finger->bit_field.x, finger->bit_field.y);
		}
	} else {
		if (tsp_touched[idx] == 1) { // touch up
			TPD_LOG("[ TSP ] %s - %d (%d, %d)\n", TOUCH_UP_MESSAGE,
				finger->bit_field.id,
				finger->bit_field.x, finger->bit_field.y);
			tsp_touched[idx] = 0;
		}
	}
#endif  // PRINT_TOUCH_EVENT
}

static void tpd_down ( int x, int y, int id )
{
//	input_report_key ( tpd->dev, BTN_TOUCH, 1 );
	input_report_abs ( tpd->dev, ABS_MT_TOUCH_MAJOR, 1 );
	input_report_abs ( tpd->dev, ABS_MT_POSITION_X, x );
	input_report_abs ( tpd->dev, ABS_MT_POSITION_Y, y );
	input_report_abs ( tpd->dev, ABS_MT_TRACKING_ID, id );
	input_report_abs ( tpd->dev, ABS_MT_PRESSURE, 200 );
	input_mt_sync ( tpd->dev );
	TPD_DOWN_DEBUG_TRACK ( x, y );
}

static void tpd_up ( int x, int y, int id )
{
//	input_report_key ( tpd->dev, BTN_TOUCH, 0 );
	input_report_abs ( tpd->dev, ABS_MT_TOUCH_MAJOR, 0 );
	input_report_abs ( tpd->dev, ABS_MT_POSITION_X, x );
	input_report_abs ( tpd->dev, ABS_MT_POSITION_Y, y );
	input_report_abs ( tpd->dev, ABS_MT_TRACKING_ID, id );
	input_report_abs ( tpd->dev, ABS_MT_PRESSURE, 0 );
	input_mt_sync ( tpd->dev );
	TPD_UP_DEBUG_TRACK ( x, y );
}

static void release_finger(finger_info *finger)
{
	input_report_abs ( tpd->dev, ABS_MT_TRACKING_ID, finger->bit_field.id - 1 );
	input_report_abs ( tpd->dev, ABS_MT_POSITION_X, finger->bit_field.x );
	input_report_abs ( tpd->dev, ABS_MT_POSITION_Y, finger->bit_field.y );
	input_report_abs ( tpd->dev, ABS_MT_TOUCH_MAJOR, finger->bit_field.w );
	input_report_abs ( tpd->dev, ABS_MT_PRESSURE, 0 );
	input_mt_sync ( tpd->dev );

	printk("[ TSP ] force release %d(%d, %d)\n", finger->bit_field.id,
	       finger->bit_field.x, finger->bit_field.y);

	finger->bit_field.udmg &= ~(PRESS_MSG_MASK);
	print_tsp_event(finger);

	finger->bit_field.id = 0;

	input_sync(ts_data->input_dev);
}

static void release_key(finger_info *key)
{
	int id = key->bit_field.id - 1;

	tpd_button(tpd_keys_dim_local[id][0], tpd_keys_dim_local[id][1], 0);

	printk("[ TSP ] %s() key%d, event: %d\n", __func__, id, key->bit_field.w);

	key->bit_field.id = 0;

	input_sync(ts_data->input_dev);
}

static void clear_input_data(struct ist30xx_data *data)
{
	TPD_FUN();
	int i, pressure;
	finger_info *fingers = (finger_info *)data->prev_fingers;
	finger_info *keys = (finger_info *)data->prev_keys;

	for (i = 0; i < data->num_fingers; i++)
	{
		if (fingers[i].bit_field.id == 0)
			continue;

		if (fingers[i].bit_field.udmg & PRESS_MSG_MASK)
			release_finger(&fingers[i]);
	}

    for (i = 0; i < data->num_keys; i++)
	{
		if (keys[i].bit_field.id == 0)
			continue;

		if (keys[i].bit_field.w == PRESS_MSG_KEY)
			release_key(&keys[i]);
	}
}

#if IST30XX_DEBUG
extern TSP_INFO ist30xx_tsp_info;
extern TKEY_INFO ist30xx_tkey_info;
#endif
static int check_report_data(struct ist30xx_data *data, int finger_counts, int key_counts)
{
	int i, j;
	bool valid_id;

	/* current finger info */
	for (i = 0; i < finger_counts; i++) {
		if ((data->fingers[i].bit_field.id == 0) ||
		    (data->fingers[i].bit_field.id > ist30xx_tsp_info.finger_num) ||
		    (data->fingers[i].bit_field.x > IST30XX_MAX_X) ||
		    (data->fingers[i].bit_field.y > IST30XX_MAX_Y)) {
			pr_err("[ TSP ] Error, %d[%d] - (%d, %d)\n", i,
			       data->fingers[i].bit_field.id,
			       data->fingers[i].bit_field.x,
			       data->fingers[i].bit_field.y);

			data->fingers[i].bit_field.id = 0;
			return -EPERM;
		}
	}

	/* previous finger info */
	if (data->num_fingers >= finger_counts) {
		for (i = 0; i < IST30XX_MAX_MT_FINGERS; i++) { // prev_fingers
			if (data->prev_fingers[i].bit_field.id != 0 &&
			    (data->prev_fingers[i].bit_field.udmg & PRESS_MSG_MASK)) {
				valid_id = false;
				for (j = 0; j < ist30xx_tsp_info.finger_num; j++) { // fingers
					if ((data->prev_fingers[i].bit_field.id) ==
					    (data->fingers[j].bit_field.id)) {
						valid_id = true;
						break;
					}
				}
				if (valid_id == false)
					release_finger(&data->prev_fingers[i]);
			}
		}
	}

	return 0;
}


static void report_input_data ( struct ist30xx_data *data, int finger_counts, int key_counts )
{
	int i, pressure, count;

	memset(data->prev_fingers, 0, sizeof(data->prev_fingers));

	for ( i = 0, count = 0 ; i < finger_counts ; i++ )
	{
		pressure = data->fingers[i].bit_field.udmg & PRESS_MSG_MASK;

		//print_tsp_event(&data->fingers[i]);

		if(is_key_pressed == 1)
		{
			//TPD_LOG("++++++++ KEY_CANCEL!!!!!!!!\n\n");
			input_report_key(tpd->dev, pressed_keycode, CANCEL_KEY);
			input_sync(tpd->dev);
			is_key_pressed = CANCEL_KEY;
		}

        //                                               
		input_report_abs ( tpd->dev, ABS_MT_TRACKING_ID, data->fingers[i].bit_field.id - 1 );
		input_report_abs ( tpd->dev, ABS_MT_POSITION_X, data->fingers[i].bit_field.x );
		input_report_abs ( tpd->dev, ABS_MT_POSITION_Y, data->fingers[i].bit_field.y );
		input_report_abs ( tpd->dev, ABS_MT_TOUCH_MAJOR, data->fingers[i].bit_field.w );
		input_report_abs ( tpd->dev, ABS_MT_PRESSURE, pressure );
		input_mt_sync ( tpd->dev );

		data->prev_fingers[i].full_field = data->fingers[i].full_field;
		count++;
	}


#ifdef TPD_HAVE_BUTTON
	for ( i = finger_counts ; i < finger_counts + key_counts ; i++ )
	{
		int id;
		id = data->fingers[i].bit_field.id - 1;
		is_key_pressed = ( data->fingers[i].bit_field.w == PRESS_MSG_KEY ) ? 1 : 0;
		pressed_keycode = tpd_keys_local[id];
		tpd_button ( tpd_keys_dim_local[id][0], tpd_keys_dim_local[id][1], is_key_pressed );

		if ( is_key_pressed == 1 )
			TPD_LOG ( "Touch Key pressed: keyID = 0x%x\n", id );
		else
			TPD_LOG("Touch Key released: keyID = 0x%x\n", id);
			
		data->prev_keys[finger_counts - i] = data->fingers[i];
		count++;
	}
#endif  // TPD_HAVE_BUTTON

	if (count > 0 && tpd != NULL && tpd->dev != NULL)
		input_sync(tpd->dev);

	data->num_fingers = finger_counts;
	data->num_keys = key_counts;
	ist30xx_error_cnt = 0;
}


/*
 * CMD : CMD_GET_COORD
 *               [31:30]  [29:26]  [25:16]  [15:10]  [9:0]
 *   Multi(1st)  UDMG     Rsvd.    NumOfKey Rsvd.    NumOfFinger
 *    Single &   UDMG     ID       X        Area     Y
 *   Multi(2nd)
 *
 *   UDMG [31] 0/1 : single/multi
 *   UDMG [30] 0/1 : unpress/press
 */
static int touch_event_handler ( void *unused )
{
	int i, ret;
	int key_cnt, finger_cnt, read_cnt;
	struct ist30xx_data *data = ts_data;
	u32 msg[IST30XX_MAX_MT_FINGERS];
	bool unknown_idle = false;

#if IST30XX_TRACKING_MODE
	u32 ms;
#endif

	struct sched_param param = { .sched_priority = RTPM_PRIO_TPD };

	sched_setscheduler ( current, SCHED_RR, &param );

	do
	{
		set_current_state ( TASK_INTERRUPTIBLE );
		wait_event_interruptible ( waiter, ( tpd_flag != 0 ) );
		tpd_flag = 0;

		set_current_state ( TASK_RUNNING );

		if ( !data->irq_enabled )
			continue;

		memset(msg, 0, sizeof(msg));

		ret = ist30xx_get_position ( data->client, msg, 1 );
		if ( ret )
			goto irq_err;

		//DMSG("[ TSP] irq msg: 0x%08x\n", msg[0]); /* i2c, int test */
		if (msg[0] == 0) {  /* ESD avoid code */
            schedule_delayed_work ( &work_reset_check, 0 );
			continue;
        }

#if IST30XX_EVENT_MODE
		if ((data->status.update != 1) && (data->status.calib != 1))
			ktime_get_ts(&t_event);
#endif

#if IST30XX_TRACKING_MODE
		ms = t_event.tv_sec * 1000 + t_event.tv_nsec / 1000000;
		ist30xx_put_track(ms, msg[0]);
#endif

#if IST30XX_NOISE_MODE
		unknown_idle = false;
		
        if ( get_event_mode )
		{
            if ( ( msg[0] & 0xFFFF0000 ) == IST30XX_IDLE_STATUS )
			{
				//DMSG("[ TSP ] idle status: 0x%08x\n", *msg);
                if ( msg[0] & IDLE_ALGORITHM_MODE )
                    continue;

                for ( i = 0 ; i < IST30XX_MAX_MT_FINGERS ; i++ )
				{
                    if ( data->prev_fingers[i].bit_field.id == 0 )
                        continue;

                    if ( data->prev_fingers[i].bit_field.udmg & PRESS_MSG_MASK )
					{
						TPD_LOG("prev_fingers: %08x\n",	data->prev_fingers[i].full_field);
						release_finger(&data->prev_fingers[i]);
						unknown_idle = true;
					}
				}
				
				for (i = 0; i < 4; i++)
				{
					if (data->prev_keys[i].bit_field.id == 0)
						continue;

					if (data->prev_keys[i].bit_field.w == PRESS_MSG_KEY)
					{
						TPD_LOG("prev_keys: %08x\n", data->prev_keys[i].full_field);
						release_key(&data->prev_keys[i]);
						unknown_idle = true;
					}
				}

				if (unknown_idle)
				{
					schedule_delayed_work(&work_reset_check, 0);
					TPD_LOG("Find unknown pressure\n");
				}

                continue;
            }
        }
#endif  // IST30XX_NOISE_MODE

		if ( ( msg[0] & CALIB_MSG_MASK ) == CALIB_MSG_VALID )
		{
			data->status.calib_msg = msg[0];
			DMSG("[ TSP ] calib status: 0x%08x\n", data->status.calib_msg);
			continue;
		}

		for ( i = 0 ; i < IST30XX_MAX_MT_FINGERS ; i++ )
			data->fingers[i].full_field = 0;

		key_cnt = 0;
		finger_cnt = 1;
		read_cnt = 1;
		data->fingers[0].full_field = msg[0];

		if ( data->fingers[0].bit_field.udmg & MULTI_MSG_MASK )
		{
			key_cnt = data->fingers[0].bit_field.x;
			finger_cnt = data->fingers[0].bit_field.y;
			read_cnt = finger_cnt + key_cnt;

			if (finger_cnt > ist30xx_tsp_info.finger_num ||
			    key_cnt > ist30xx_tkey_info.key_num) {
				pr_err("[ TSP ] Invalid touch count - finger: %d(%d), key: %d(%d)\n",
				       finger_cnt, ist30xx_tsp_info.finger_num,
			    	   key_cnt, ist30xx_tkey_info.key_num);
					goto irq_err;
			}

#if I2C_BURST_MODE
			ret = ist30xx_get_position ( data->client, msg, read_cnt );
			if ( ret )
				goto irq_err;

			for ( i = 0 ; i < read_cnt ; i++ )
				data->fingers[i].full_field = msg[i];
#else
			for ( i = 0 ; i < read_cnt ; i++ )
			{
				ret = ist30xx_get_position ( data->client, &msg[i], 1 );
				if ( ret )
					goto irq_err;

				data->fingers[i].full_field = msg[i];
			}
#endif          // I2C_BURST_MODE

#if IST30XX_TRACKING_MODE
			for (i = 0; i < read_cnt; i++)
				ist30xx_put_track(ms, msg[i]);
#endif
		}

		if (check_report_data(data, finger_cnt, key_cnt))
			continue;
		
		if ( read_cnt > 0 )
			report_input_data ( data, finger_cnt, key_cnt );

		continue;

irq_err:
		TPD_ERR ( "intr msg[0]: 0x%08x, ret: %d\n", msg[0], ret );
		ist30xx_request_reset ();
	} while ( !kthread_should_stop () );

	return 0;
}

static void tpd_eint_interrupt_handler ( void )
{
	TPD_DEBUG("[ TSP ] TPD interrupt has been triggered\n");
	tpd_flag = 1;
	wake_up_interruptible ( &waiter );
}

static void tpd_eint_init ( void )
{
	// TODO : Implements this function to meet your system.
	mt65xx_eint_set_sens ( CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_SENSITIVE );
	mt65xx_eint_set_hw_debounce ( CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_CN );
	mt65xx_eint_registration ( CUST_EINT_TOUCH_PANEL_NUM, CUST_EINT_TOUCH_PANEL_DEBOUNCE_EN, CUST_EINT_TOUCH_PANEL_POLARITY, tpd_eint_interrupt_handler, 1 );
}


void ist30xx_set_ta_mode ( bool charging )
{
	TPD_LOG ( "ist30xx_set_ta_mode, %d\n", charging );
#if IST30XX_DETECT_TA
	if ( ( ist30xx_ta_status == -1 ) || ( charging == ist30xx_ta_status ) )
		return;
	ist30xx_ta_status = charging ? 1 : 0;
	schedule_delayed_work ( &work_reset_check, 0 );
#endif
}
EXPORT_SYMBOL ( ist30xx_set_ta_mode );


static void reset_work_func ( struct work_struct *work )
{
	TPD_FUN ();
	if ( ( ts_data == NULL ) || ( ts_data->client == NULL ) )
		return;

	DMSG("[ TSP ] Request reset function\n");

	if ((ts_data->status.power == 1) &&
	    (ts_data->status.update != 1) && (ts_data->status.calib != 1))
	{
		mutex_lock ( &ist30xx_mutex );
		ist30xx_disable_irq ( ts_data );

		clear_input_data ( ts_data );

		ist30xx_cmd_run_device ( ts_data->client );

		ist30xx_start ( ts_data );

		ist30xx_enable_irq ( ts_data );
		mutex_unlock ( &ist30xx_mutex );
	}
}

#if IST30XX_INTERNAL_BIN && IST30XX_UPDATE_BY_WORKQUEUE
static void fw_update_func(struct work_struct *work)
{
	if ((ts_data == NULL) || (ts_data->client == NULL))
		return;

	DMSG("[ TSP ] FW update function\n");

	if (ist30xx_auto_bin_update(ts_data))
		ist30xx_disable_irq(ts_data);
}
#endif // IST30XX_INTERNAL_BIN && IST30XX_UPDATE_BY_WORKQUEUE


#if IST30XX_EVENT_MODE
void timer_handler(unsigned long data)
{
	int event_ms;
	int curr_ms;

	if ( get_event_mode )
	{
		if ( (ts_data->status.power == 1) && (ts_data->status.update != 1) )
		{
			ktime_get_ts ( &t_current );

			curr_ms = t_current.tv_sec * 1000 + t_current.tv_nsec / 1000000;
			event_ms = t_event.tv_sec * 1000 + t_event.tv_nsec / 1000000;

			//printk ( "[ TSP ] event_ms %d, current: %d\n", event_ms, curr_ms );

			if ( ts_data->status.calib == 1 )
			{
				if (curr_ms - event_ms >= 2000)
				{
					ts_data->status.calib = 0;
					//printk("[ TSP ] calibration timeout over 3sec\n");
					schedule_delayed_work(&work_reset_check, 0);
					ktime_get_ts(&t_event);
				}
			}
#if IST30XX_NOISE_MODE
			else if ( curr_ms - event_ms >= 5000 )
			{
				pr_err("[ TSP ] idle timeout over 5sec\n");
				schedule_delayed_work(&work_reset_check, 0);
			}
#endif // IST30XX_NOISE_MODE
		}
	}

	mod_timer(&idle_timer, get_jiffies_64() + EVENT_TIMER_INTERVAL);
}
#endif // IST30XX_EVENT_MODE

static int __devinit tpd_probe ( struct i2c_client *client, const struct i2c_device_id *id )
{
	int ret;
	int retval = TPD_OK;

	struct ist30xx_data *data;

	DMSG("[ TSP ] %s() ,the i2c addr=0x%x", __func__, client->addr);
	
	data = kzalloc ( sizeof ( *data ), GFP_KERNEL );
	if ( !data )
		return -ENOMEM;

	data->num_fingers = 5;
	data->num_keys = 4;
	data->irq_enabled = 1;
	data->client = client;
	data->input_dev = tpd->dev;
	i2c_set_clientdata ( client, data );

	ts_data = data;

	ret = ist30xx_init_system ();
	if ( ret )
	{
		dev_err ( &client->dev, "chip initialization failed\n" );
		goto err_init_drv;
	}

	ret = ist30xx_init_update_sysfs ();
	if ( ret )
		goto err_init_drv;

#if IST30XX_DEBUG
	ret = ist30xx_init_misc_sysfs ();
	if ( ret )
		goto err_init_drv;
#endif

# if IST30XX_FACTORY_TEST
	ret = ist30xx_init_factory_sysfs();
	if (ret)
		goto err_init_drv;
#endif

#if IST30XX_TRACKING_MODE
	ret = ist30xx_init_tracking_sysfs();
	if (ret)
		goto err_init_drv;
#endif

	tpd_eint_init ();
	thread = kthread_run ( touch_event_handler, 0, TPD_DEVICE );
	if ( IS_ERR ( thread ) )
	{
		retval = PTR_ERR ( thread );
		TPD_ERR ("failed to create kernel thread: %d\n", retval );
	}

	ist30xx_disable_irq ( data );

#if IST30XX_INTERNAL_BIN
# if IST30XX_UPDATE_BY_WORKQUEUE
	INIT_DELAYED_WORK(&work_fw_update, fw_update_func);
	schedule_delayed_work(&work_fw_update, IST30XX_UPDATE_DELAY);
# else
	ret = ist30xx_auto_bin_update(data);
	if (ret != 0)
		goto err_irq;
# endif
#endif  // IST30XX_INTERNAL_BIN

	ret = ist30xx_get_info(data);
	printk("[ TSP ] Get info: %s\n", (ret == 0 ? "success" : "fail"));

	INIT_DELAYED_WORK ( &work_reset_check, reset_work_func );

#if IST30XX_DETECT_TA
	ist30xx_ta_status = 0;
#endif

#if IST30XX_EVENT_MODE
	init_timer ( &idle_timer );
	idle_timer.function = timer_handler;
	idle_timer.expires = jiffies_64 + ( EVENT_TIMER_INTERVAL );

	mod_timer ( &idle_timer, get_jiffies_64 () + EVENT_TIMER_INTERVAL );

	ktime_get_ts ( &t_event );
#endif


	ist30xx_initialized = 1;
	tpd_load_status = 1;

	TPD_LOG ( "IST30XX driver probe %s\n", ( retval < TPD_OK ) ? "FAIL" : "PASS" );

	return 0;

err_irq:
	ist30xx_disable_irq ( data );
err_init_drv:
#if IST30XX_EVENT_MODE
	get_event_mode = false;
#endif
	TPD_ERR ( "Error, ist30xx init driver\n" );
	ist30xx_power_off ();

	return 0;
}

static int tpd_detect ( struct i2c_client *client, int kind, struct i2c_board_info *info )
{
	TPD_FUN ();
	strcpy ( info->type, TPD_DEVICE );
	return 0;
}

static int __devexit tpd_remove ( struct i2c_client *client )
{
	TPD_FUN ();
	ist30xx_power_off ();
	return 0;
}

static struct i2c_driver tpd_i2c_driver = {
//	.driver		={
//		.name	= "mtk-tpd",
//		.owner	= THIS_MODULE,
//	},
	.driver.name	= "mtk-tpd",
	.probe		= tpd_probe,
	.remove		= __devexit_p(tpd_remove),
	.id_table	= tpd_id,
	.detect		= tpd_detect,
//jin	.address_data	= &addr_data,
};

static int tpd_local_init ( void )
{
	if ( i2c_add_driver ( &tpd_i2c_driver ) != 0 )
	{
		TPD_ERR ( "i2c_add_driver failed\n" );
		return -1;
	}

#ifdef TPD_HAVE_BUTTON
	tpd_button_setting ( TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local );
#endif
#if (defined(TPD_WARP_START) && defined(TPD_WARP_END))
	TPD_DO_WARP = 1;
	memcpy ( tpd_wb_start, tpd_wb_start_local, TPD_WARP_CNT * 4 );
	memcpy ( tpd_wb_end, tpd_wb_start_local, TPD_WARP_CNT * 4 );
#endif
#if (defined(TPD_HAVE_CALIBRATION) && !defined(TPD_CUSTOM_CALIBRATION))
	memcpy ( tpd_calmat, tpd_def_calmat_local, 8 * 4 );
	memcpy ( tpd_def_calmat, tpd_def_calmat_local, 8 * 4 );
#endif

	tpd_type_cap = 1;

	return 0;
}

static int tpd_resume(void)
{
	TPD_FUN ();

	mutex_lock( &ist30xx_mutex );

	ist30xx_power_on ();
	ist30xx_write_cmd ( ts_data->client, CMD_RUN_DEVICE, 0 );
	msleep ( 10 );

	ist30xx_start ( ts_data );
	ist30xx_enable_irq ( ts_data );

	mutex_unlock( &ist30xx_mutex );

	return TPD_OK;
}

static int tpd_suspend ( pm_message_t message )
{
	TPD_FUN ();

	mutex_lock( &ist30xx_mutex );

	ist30xx_disable_irq ( ts_data );
	ist30xx_power_off ();

	clear_input_data ( ts_data );
	mutex_unlock( &ist30xx_mutex );

	return TPD_OK;
}

static struct tpd_driver_t tpd_device_driver = {
	.tpd_device_name	= "ist30xx_driver",
	.tpd_local_init		= tpd_local_init,
	.suspend		= tpd_suspend,
	.resume			= tpd_resume,
#ifdef TPD_HAVE_BUTTON
	.tpd_have_button	= 1,
#else
	.tpd_have_button	= 0,
#endif
};

static int __init tpd_driver_init ( void )
{
	TPD_FUN ();
#if 1   /*                                                                                            */
	int gpio_status = 0;

    gpio_status = mt_get_gpio_in ( GPIO_TOUCH_IC_ID );
    TPD_LOG ( "TOUCH_IC_ID[%d]\n", gpio_status );

	if ( gpio_status )
	{
		TPD_LOG ( "Imagis driver add\n" );
	}
	else
	{
		TPD_LOG ( "melpas driver add\n" );
		return 0;
	}
#endif  /*                                                                   */

	i2c_register_board_info ( 0, &ist30xx_i2c_tpd, 1 );
	if ( tpd_driver_add ( &tpd_device_driver ) < 0 )
		TPD_ERR ( "ist30xx driver add failed\n" );

	return 0;
}

static void __exit tpd_driver_exit(void)
{
	TPD_FUN ();

	tpd_driver_remove ( &tpd_device_driver );
}

module_init ( tpd_driver_init );
module_exit ( tpd_driver_exit );

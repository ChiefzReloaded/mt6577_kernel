
/*             
  
                                        
                                             
  
                             
 */
#include "mount.h"
#include "ext4/ext4.h"
#include "sreadahead_prof.h"

static struct sreadahead_prof prof_buf;
static struct timer_list prof_timer;
#ifdef DEBUG
int eq, dq;
#endif
//--------------------------------------------------------------
// functions - circular queue buffer for profiling data
//--------------------------------------------------------------

static int is_full(void)
{
	int ret = 0;

	mutex_lock(&prof_buf.ulock);
	if (((prof_buf.rear+1) % PROF_BUF_SIZE) == prof_buf.front)
		ret = 1;
	else
		ret = 0;
	mutex_unlock(&prof_buf.ulock);

	return ret;
}

static int enqueue_profdata(struct sreadahead_profdata data)
{
	int buf_offset;

	if (is_full()) {
		printk(KERN_DEBUG "SRA-enqueue: buffer is full, %s\n", data.name);
		return 0;
	}

	mutex_lock(&prof_buf.ulock);
	buf_offset = (prof_buf.rear + 1) % PROF_BUF_SIZE;
	prof_buf.data[buf_offset] = data;
	prof_buf.rear = buf_offset;
	mutex_unlock(&prof_buf.ulock);

#ifdef DEBUG
	eq++;
	printk(KERN_DEBUG "SRA-enqueue: %d == %s:%lld:%lld#%s, buf rear: %d\n",
			eq, data.name, data.pos[0], data.len, data.procname, prof_buf.rear);
#endif
	complete(&prof_buf.completion);
	return 1;
}

static int dequeue_profdata(struct sreadahead_profdata *data)
{
	if (!wait_for_completion_timeout(&prof_buf.completion, msecs_to_jiffies(BUF_WAIT_TIMEOUT))) {
		printk(KERN_DEBUG "SRA-dequeue TIMEOUT!\n");
		return 0;
	}

	mutex_lock(&prof_buf.ulock);
	prof_buf.front = (prof_buf.front + 1) % PROF_BUF_SIZE;
	*data = prof_buf.data[prof_buf.front];
	mutex_unlock(&prof_buf.ulock);

	memset(&prof_buf.data[prof_buf.front], 0x00, sizeof(struct sreadahead_profdata));
#ifdef DEBUG
	dq++;
	printk(KERN_DEBUG "SRA-dequeue: %d, buf front: %d\n", dq, prof_buf.front);
#endif
	return 1;
}

//--------------------------------------------------------------
// functions - timer
//--------------------------------------------------------------
static void prof_timer_handler(unsigned long arg)
{
#ifdef DEBUG
	printk(KERN_DEBUG "SRA - timer handler: %d\n", prof_buf.state);
#endif
	prof_buf.state = PROF_NOT;
}


//--------------------------------------------------------------
// functions - initialization of debugfs
//--------------------------------------------------------------

#define DBGFS_BUFLEN (FILE_PATHLEN + FILE_NAMELEN + FILE_SIZELEN + FILE_SIZELEN + PROC_NAMELEN)
static ssize_t sreadahead_dbgfs_read(
		struct file *file,
		char __user *buff,
		size_t buff_count,
		loff_t *ppos)
{
	struct sreadahead_profdata data;

	if (dequeue_profdata(&data)) {
#ifdef DEBUG
		printk(KERN_DEBUG "SRA - %s:%lld:%lld#%s\n",
				data.name, data.pos[0], data.len, data.procname);
#endif
		if (copy_to_user(buff, &data, sizeof(struct sreadahead_profdata)))
			return 0;
	} else {
		return 0;
	}

	(*ppos) = 0;
	return 1;
}
#undef DBGFS_BUFLEN

static ssize_t sreadaheadflag_dbgfs_read(
		struct file *file,
		char __user *buff,
		size_t buff_count,
		loff_t *ppos)
{
	if (copy_to_user(buff, &prof_buf.state, sizeof(int)))
		return 0;
	(*ppos) = 0;
	return sizeof(int);
}

static ssize_t sreadaheadflag_dbgfs_write(
		struct file *file,
		const char __user *buff,
		size_t count,
		loff_t *ppos)
{
	if (copy_from_user(&prof_buf.state, buff, sizeof(int)))
		return 0;

	if (prof_buf.state == PROF_RUN) {
#ifdef DEBUG
		printk(KERN_DEBUG "SRA - add timer\n");
#endif
		prof_timer.expires = get_jiffies_64() + (PROF_TIMEOUT * HZ);
		add_timer(&prof_timer);
	} else if (prof_buf.state == PROF_DONE) {
#ifdef DEBUG
		printk(KERN_DEBUG "SRA - del timer: profiling is done by user daemon\n");
#endif
		del_timer(&prof_timer);
	}

	(*ppos) = 0;
	return sizeof(int);
}

static const struct file_operations sreadaheadflag_dbgfs_fops = {
	.read = sreadaheadflag_dbgfs_read,
	.write = sreadaheadflag_dbgfs_write,
};

static const struct file_operations sreadahead_dbgfs_fops = {
	.read = sreadahead_dbgfs_read,
};

static int __init sreadahead_init(void)
{
	struct dentry *dbgfs_dir;

	/* state init */
	prof_buf.state = PROF_NOT;

	/* lock init */
	init_completion(&prof_buf.completion);
	mutex_init(&prof_buf.ulock);

	/* timer init */
	init_timer(&prof_timer);
	prof_timer.function = prof_timer_handler;

	/* debugfs init for sreadahead */
	dbgfs_dir = debugfs_create_dir("sreadahead", NULL);
	if (!dbgfs_dir)
		return (-1);
	debugfs_create_file("profilingdata",
			0444, dbgfs_dir, NULL,
			&sreadahead_dbgfs_fops);
	debugfs_create_file("profilingflag",
			0644, dbgfs_dir, NULL,
			&sreadaheadflag_dbgfs_fops);
	return 0;
}

__initcall(sreadahead_init);

//--------------------------------------------------------------
// functions - sreadahead profiling
//--------------------------------------------------------------

static int get_absolute_path(unsigned char* buf, int buflen, struct file *filp)
{
	unsigned char tmpstr[FILE_PATHLEN+FILE_NAMELEN];
	struct dentry* tmpdentry = 0;
	struct mount* tmpmnt;
	struct mount* tmpoldmnt;
	tmpmnt = real_mount(filp->f_vfsmnt);

	tmpdentry = filp->f_path.dentry->d_parent;
	do {
		tmpoldmnt = tmpmnt;
		while (!IS_ROOT(tmpdentry)) {
			strcpy(tmpstr, buf);
			//                       
			// make codes robust
			strncpy(buf, tmpdentry->d_name.name, buflen - 1);
			buf[buflen - 1] = '\0';
			if (strlen(buf) + strlen("/") > buflen -1)
				return -1;
			else
				strcat(buf, "/");

			if (strlen(buf) + strlen(tmpstr) > buflen -1)
				return -1;
			else
				strcat(buf, tmpstr);

			tmpdentry = tmpdentry->d_parent;
		}
		tmpdentry = tmpmnt->mnt_mountpoint;
		tmpmnt = tmpmnt->mnt_parent;
	} while (tmpmnt != tmpoldmnt);
	strcpy(tmpstr, buf);
	strcpy(buf, "/");
	//                       
	// make codes robust
	if (strlen(buf) + strlen(tmpstr) > buflen -1)
		return -1;
	strcat(buf, tmpstr);

	return 0;
}

static int sreadahead_prof_RUN(struct file *filp, size_t len, loff_t pos)
{
	struct sreadahead_profdata data;
	memset(&data, 0x00, sizeof(struct sreadahead_profdata));
	data.len = (long long)len;
	data.pos[0] = pos;
	data.pos[1] = 0;
	data.procname[0] = '\0';
	get_task_comm(data.procname, current);

	if (get_absolute_path(data.name, FILE_PATHLEN + 1, filp) < 0) return -1;
	strcat(data.name, filp->f_path.dentry->d_name.name);

	enqueue_profdata(data);

	return 0;
}

int sreadahead_prof(struct file *filp, size_t len, loff_t pos)
{
	if (prof_buf.state == PROF_NOT) // minimize overhead in case of not profiling
		return 0;
	if (filp->f_op == &ext4_file_operations){
		if (prof_buf.state == PROF_RUN)
			sreadahead_prof_RUN(filp, len, pos);
	}
	return 0;
}
/*              */


/*             
  
                                        
                                             
  
                             
 */
#include <linux/semaphore.h>
#include <linux/pagemap.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mount.h>
#include <linux/debugfs.h>
#include <linux/vmalloc.h>
#include <linux/pid.h>
#include <linux/kernel.h>
#include <linux/completion.h>
#include <linux/timer.h>

#define PROF_FILENUM	 2000
#define FILE_PATHLEN	 200
#define FILE_NAMELEN	 100
#define FILE_SIZELEN	 20 // in digit
#define PROC_NAMELEN	 16
#define PROF_BUF_SIZE	100
#define PROF_TIMEOUT	60
#define BUF_WAIT_TIMEOUT 3000
//-------------------------------------------------------------------------------
#define PROF_NOT   0
#define PROF_INIT  1
#define PROF_RUN   2
#define PROF_OPT   3
#define PROF_DONE  4

struct sreadahead_profdata {
	char procname[PROC_NAMELEN];
	unsigned char name[FILE_PATHLEN+FILE_NAMELEN];
	long long len; // same as pos[][1] - pos[][0]
	long long pos[2]; // 0: start position 1: end position
};

struct sreadahead_prof {
	struct sreadahead_profdata data[PROF_BUF_SIZE];
	int state;
	int front;
	int rear;
	struct completion	completion;
	struct mutex		ulock;
};

int sreadahead_prof(struct file *filp, size_t len, loff_t pos);
/*              */

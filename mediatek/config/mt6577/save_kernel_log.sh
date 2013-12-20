touch /data/logger/kernel.log
chmod 666 /data/logger/kernel.log
cat /proc/kmsg >> /data/logger/kernel.log

1.insmod后，可以cat /proc/devices查看设备
2.insmod后,需要手动创建设备节点:mknod /dev/chrdevbase c 200 0
3.编译app：arm-none-linux-gnueabihf-gcc chrdevbaseApp.c -o chrdevbaseApp

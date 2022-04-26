CC = arm-none-linux-gnueabihf-gcc
obj = v4l2_camera

all: v4l2_camera.o base64.o
	$(CC) v4l2_camera.o base64.o -o $(obj)

v4l2_camera.o: v4l2_camera.c
	$(CC)  -c v4l2_camera.c

base64.o: base64.c base64.h
	$(CC)  -c base64.c

clean:
	rm -rf *.o $(obj)

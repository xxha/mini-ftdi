/*
	Simple example to open a maximum of 4 devices - write some data then read it back.
	Shows one method of using list devices also.
	Assumes the devices have a loopback connector on them and they also have a serial number


*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>

#include <ftdi.h>

#include <sys/wait.h>
#include <sys/stat.h>        /* For mode constants */
#include <semaphore.h>

#define	UX400VENDOR	0x0403
#define	UX400PRODUCT	0x6010
#define	UX400DES	"USB <-> Serial Converter"

#define BUF_SIZE 0x10

#define MAX_DEVICES		5
#define TEST_BUFFER		32

#define INIT_CYCLE              50

#define	FAN_SLEEP		20

#define VFL_ON          1
#define VFL_OFF         0
#define VFL_BLINK       1
#define	VFL_STABLE	0

#define	POWERKEY		0x01

#define	BUZZER_DELAY		250000

#define	UX400_LOCAL_SN		"USBLOCALBUS01"

#define	BACKLIGHT_OFFSET	0x01
#define	POWERKEY_OFFSET		0x08
#define	KEYOFFSET		0x0A

#define	REG1_OFFSET		0x00
#define	REG_GPIO_OFFSET		0x03
#define	REG_CPLDVER		0x15

#define	VEEX_KEY_BACKLIGHT	0x1A
#define	VEEX_KEY_SUMARY		0x16
#define	VEEX_KEY_HELP		0x0E
#define	VEEX_KEY_FILE		0x19
#define	VEEX_KEY_HISTORY	0x15

#define	UX400_SEM_CPLD	"UX400_SEM_CPLD"


struct ftdi_context ux400_ftdic;

int Read_bus(struct ftdi_context * handle, unsigned char haddr, unsigned char laddr, unsigned char * buff, unsigned int len);
int Write_bus(struct ftdi_context * handle, unsigned char haddr, unsigned char laddr, unsigned char * buff, unsigned int len);

int powerkey(void);
int buzzer(int on);
int fancontrol(unsigned char fandata);
int sys_init();
int powercut(void);


int uinput_fd;

int send_event(int fd, __u16 type, __u16 code, __s32 value)
{
    struct input_event event;

    memset(&event, 0, sizeof(event));
    event.type = type;
    event.code = code;
    event.value = value;

    if (write(fd, &event, sizeof(event)) != sizeof(event)) {
        fprintf(stderr, "Error on send_event");
        return -1;
    }

    return 0;
}

void uinput_init()
{
    int i;
    struct uinput_user_dev device;

    memset(&device, 0, sizeof device);

    uinput_fd=open("/dev/uinput",O_WRONLY|O_NDELAY);

    strcpy(device.name,"uinput key");

    device.id.bustype=BUS_USB;
    device.id.vendor=1;
    device.id.product=1;
    device.id.version=1;

    for (i=0; i < ABS_MAX; i++) {
        device.absmax[i] = -1;
        device.absmin[i] = -1;
        device.absfuzz[i] = -1;
        device.absflat[i] = -1;
    }

    device.absmin[ABS_X]=0;
    device.absmax[ABS_X]=255;
    device.absfuzz[ABS_X]=0;
    device.absflat[ABS_X]=0;

    if (write(uinput_fd,&device,sizeof(device)) != sizeof(device))
    {
        fprintf(stderr, "error setup\n");
    }

    if (ioctl(uinput_fd,UI_SET_EVBIT, EV_KEY) < 0)
        fprintf(stderr, "error evbit key\n");

    if (ioctl(uinput_fd,UI_SET_KEYBIT, KEY_F1) < 0)
        fprintf(stderr, "error evbit key\n");

    if (ioctl(uinput_fd,UI_SET_KEYBIT, KEY_F3) < 0)
        fprintf(stderr, "error evbit key\n");

    if (ioctl(uinput_fd,UI_SET_KEYBIT, KEY_F4) < 0)
        fprintf(stderr, "error evbit key\n");

    if (ioctl(uinput_fd,UI_SET_KEYBIT, KEY_F5) < 0)
        fprintf(stderr, "error evbit key\n");

    if (ioctl(uinput_fd,UI_SET_KEYBIT, KEY_F6) < 0)
        fprintf(stderr, "error evbit key\n");

    if (ioctl(uinput_fd,UI_SET_KEYBIT, KEY_POWER) < 0)
        fprintf(stderr, "error evbit key\n");

    if (ioctl(uinput_fd,UI_DEV_CREATE) < 0)
    {
        fprintf(stderr, "error create\n");
    }

}

void key_send(__u16 code, __s32 value)
{
    send_event(uinput_fd, EV_KEY, code, value);
    send_event(uinput_fd, EV_SYN, SYN_REPORT, 0);
}

inline char keyscan(void)
{
	unsigned char data;
	unsigned int ret;

	ret = Read_bus(&ux400_ftdic, 0x00, KEYOFFSET, &data, 1);
	return data;
}

char cpldver(void)
{
	unsigned char data;
	int ret = 0;

	if((ret = Read_bus(&ux400_ftdic, 0x00, REG_CPLDVER, &data, 1) < 0))
	{
		printf("Read CPLD version failed.\n");
		return -1;
	}else{
		printf("CPLD VERSION: 0x%x\n", data);
	}

	return data;
}


int buzzer(int on)
{
	unsigned char buffer[BUF_SIZE];
	int ret = 0;

	if(on == 1) buffer[0] = 0x04;
	else buffer[0] = 0x00;

	if(( ret = Write_bus(&ux400_ftdic, 0x00, 0x00, buffer, 1)) < 0)
	{
		printf("Write buzz reg failed!\n");
		return -1;
	}

	return 0;
}

int powercut(void)
{
	unsigned char buffer[BUF_SIZE];
	int ret = 0;

	buffer[0] = 0x02;
	if(( ret = Write_bus(&ux400_ftdic, 0x00, 0x00, buffer, 1)) < 0)
	{
		printf("Write power off reg failed!\n");
		return -1;
	}

	return 0;
}

int backlight(unsigned char data)
{
	unsigned char temp = 0;
	int ret = 0;

	temp = data & 0x3F;

	if(( ret = Write_bus(&ux400_ftdic, 0x00, BACKLIGHT_OFFSET, &temp, 1)) < 0)
	{
		printf("Write backlight reg failed!\n");

		return -1;
	}

	return 0;
}

int vfl_pwr(unsigned int on, unsigned int blink)
{
	unsigned char data, temp = 0;
	int ret;

	if((ret = Read_bus(&ux400_ftdic, 0x00, REG1_OFFSET, &data, 1)) < 0)
	{
		printf("Read VFL reg failed.\n");

		return -1;
	}

	if( on == VFL_ON){
		data |= 0x08;
		if(blink == VFL_BLINK){
			data |= 0x10;
		}else{
			data &= (~0x10);
		}
	}else{
		data &= (~0x08);
	}

//	data &= (~0x02);
//	data &= (~0x04);

	if((ret = Write_bus(&ux400_ftdic, 0x00, REG1_OFFSET, &data, 1)) < 0){
		printf("Write VFL reg failed.\n");
		return -1;
	}

	return 0;
}

int gpio_set(unsigned char gpio)
{	
	int ret = 0;

        if(( ret = Write_bus(&ux400_ftdic, 0x00, REG_GPIO_OFFSET, &gpio, 1)) < 0)
	{
		printf("Write GPIO reg failed!\n");
		return -1;
	}

	return 0;
}


int powerkey(void)
{
	unsigned char data;
	unsigned int ret;
	static int keypressed = 0;

	ret = Read_bus(&ux400_ftdic, 0x00, POWERKEY_OFFSET, &data, 1);
	if((data == 0x1F)||(data == 0x15)||(data == 0x19)||(data == 0xFF)) return 0;

	if((data & POWERKEY) == POWERKEY) {
		printf("POWER KEY FOUND (0x%x)!!!\n", data);

		if(keypressed == 0){
			keypressed = 1;
			buzzer(1);
			usleep(BUZZER_DELAY);
			buzzer(0);
			usleep(BUZZER_DELAY);
			buzzer(1);
			usleep(BUZZER_DELAY);
			buzzer(0);
		}
		return 1;
	}

	return 0;

}

static long GetBinStartAddr(unsigned char *pBuff, long len)
{
	long i;

	for (i=0; i<len-3; i++)
	{	
		if (pBuff[i] == 0xFF && pBuff[i+1] == 0xFF && pBuff[i+2] == 0xFF && pBuff[i+3] == 0xFF)
		{
			return i;
		}
	}

	return 0;
}

int fancontrol(unsigned char fandata)
{
	unsigned char buffer[2];
	int ret = 0;

	buffer[0] = fandata;
	if(( ret = Write_bus(&ux400_ftdic, 0x00, 0x02, buffer, 1)) < 0)
	{
		printf("Write FAN reg failed!\n");
		return -1;
	}

	return 0;
}

int Read_bus(struct ftdi_context * handle, unsigned char haddr, unsigned char laddr, unsigned char * buff, unsigned int len)
{
	unsigned char cmd [4] = { 0x00 };
	int ret = 0;

	cmd[0] = 0x91;
	cmd[1] = haddr;
	cmd[2] = laddr;
	cmd[3] = 0x87;

	if((ret = ftdi_usb_purge_rx_buffer (handle)) < 0)
	{
	    fprintf(stderr, "ftdi_usb_purge_rx_buffer failed: %d (%s)\n", ret, ftdi_get_error_string(&ux400_ftdic));
	    return EXIT_FAILURE;
	}

	if((ret = ftdi_usb_purge_tx_buffer (handle)) < 0)
	{
	    fprintf(stderr, "ftdi_usb_purge_tx_buffer failed: %d (%s)\n", ret, ftdi_get_error_string(&ux400_ftdic));
	    return EXIT_FAILURE;
	}

	if((ret = ftdi_write_data (handle, cmd, 4)) < 0)
	{
	    fprintf(stderr, "ftdi_write_data failed: %d (%s)\n", ret, ftdi_get_error_string(&ux400_ftdic));
	    return EXIT_FAILURE;
	}

	if((ret = ftdi_read_data (handle, buff, len)) < 0)
	{
	    fprintf(stderr, "ftdi_read_data failed: %d (%s)\n", ret, ftdi_get_error_string(&ux400_ftdic));
	    return EXIT_FAILURE;
	}

	return ret;
}

int Write_bus(struct ftdi_context * handle, unsigned char haddr, unsigned char laddr, unsigned char * buff, unsigned int len)
{
	unsigned char cmd [4] = { 0x00 };
	int ret = 0;
	int i = 0;

	if((ret = ftdi_usb_purge_tx_buffer (handle)) < 0)
	{
	    fprintf(stderr, "ftdi_usb_purge_tx_buffer failed: %d (%s)\n", ret, ftdi_get_error_string(&ux400_ftdic));
	    return EXIT_FAILURE;
	}

	for(i = 0; i < len; i ++)
	{
		cmd[0] = 0x93;
		cmd[1] = haddr;
		cmd[2] = laddr;
		cmd[3] = buff[i];

		if((ret = ftdi_write_data (handle, cmd, 4)) < 0)
		{
		    fprintf(stderr, "ftdi_write_data failed: %d (%s)\n", ret, ftdi_get_error_string(&ux400_ftdic));
		    return EXIT_FAILURE;
		}
	}

	return ret;
}


int sys_init()
{
	int ret, i;
	struct ftdi_device_list *devlist, *curdev;
	char manufacturer[128], description[128], serialno[128];

	if (ftdi_init(&ux400_ftdic) < 0)
	{
		fprintf(stderr, "ftdi_init failed\n");
		return EXIT_FAILURE;
	}

#if 0
	if ((ret = ftdi_usb_find_all(&ux400_ftdic, &devlist, UX400VENDOR, UX400PRODUCT)) < 0)
	{
		fprintf(stderr, "ftdi_usb_find_all failed: %d (%s)\n", ret, ftdi_get_error_string(&ux400_ftdic));
		return EXIT_FAILURE;
	}

	printf("Number of FTDI devices found: %d\n", ret);

	i = 0;
	for (curdev = devlist; curdev != NULL; i++)
	{
		printf("Checking device: %d\n", i);
		if ((ret = ftdi_usb_get_strings(&ux400_ftdic, curdev->dev, manufacturer, 128, description, 128, serialno, 128)) < 0)
		{
		    fprintf(stderr, "ftdi_usb_get_strings failed: %d (%s)\n", ret, ftdi_get_error_string(&ux400_ftdic));
		    return EXIT_FAILURE;
		}
		printf("Manufacturer: %s, Description: %s, Serial number: %s\n\n", manufacturer, description, serialno);
		curdev = curdev->next;
	}

	ftdi_list_free(&devlist);
#endif

	if((ret = ftdi_usb_open_desc(&ux400_ftdic, UX400VENDOR, UX400PRODUCT, UX400DES, UX400_LOCAL_SN)) < 0)
	{
	    fprintf(stderr, "ftdi_usb_open_desc failed: %d (%s)\n", ret, ftdi_get_error_string(&ux400_ftdic));
	    return EXIT_FAILURE;
	}

	if((ret = ftdi_usb_reset(&ux400_ftdic)) < 0)
	{
	    fprintf(stderr, "ftdi_usb_reset failed: %d (%s)\n", ret, ftdi_get_error_string(&ux400_ftdic));
	    return EXIT_FAILURE;
	}

	if((ret = ftdi_set_baudrate(&ux400_ftdic, 9600)) < 0)
	{
	    fprintf(stderr, "ftdi_set_baudrate failed: %d (%s)\n", ret, ftdi_get_error_string(&ux400_ftdic));
	    return EXIT_FAILURE;
	}

	if((ret = ftdi_set_bitmode(&ux400_ftdic, 0x00, BITMODE_RESET)) < 0)
	{
	    fprintf(stderr, "ftdi_set_bitmode failed: %d (%s)\n", ret, ftdi_get_error_string(&ux400_ftdic));
	    return EXIT_FAILURE;
	}

	usleep(10000);

	if((ret = ftdi_set_bitmode(&ux400_ftdic, 0x00, BITMODE_MCU)) < 0)
	{
	    fprintf(stderr, "ftdi_set_bitmode failed: %d (%s)\n", ret, ftdi_get_error_string(&ux400_ftdic));
	    return EXIT_FAILURE;
	}

	return 0;
}

void keys()
{
	char temp;
	int stat = 0;
	__u16 keycode;
	sem_t * sem_id;

	fancontrol(0x33);

	sem_id = sem_open(UX400_SEM_CPLD, O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH, 1);
	if(sem_id == SEM_FAILED) {
		perror("UX400 KEY sem_open");
		exit(1);
	}
	
	system("/usr/bin/ux400-vga");

	for(;;){
		if(sem_wait(sem_id) < 0) {
			perror("UX400 KEY sem_wait");
			exit(1);
		}

		if(powerkey() == 1){
			key_send(KEY_POWER, 1 );
			key_send(KEY_POWER, 0 );

			key_send(KEY_POWER, 1 );
			key_send(KEY_POWER, 0 );
		}
		temp = keyscan();

		if(sem_post(sem_id) < 0) {
			perror("UX400 KEY sem_post");
		}

		switch(temp){
			case 0x1F:
			case 0x00:
				if(stat == 1){
					key_send(keycode, 0);
				}
				stat = 0;
			break;
			case VEEX_KEY_BACKLIGHT:
				if(stat != 1) key_send(KEY_F5, 1);
				stat = 1;
				keycode = KEY_F5;
			break;
			case VEEX_KEY_SUMARY:
				if(stat != 1) key_send(KEY_F6, 1 );
				stat = 1;
				keycode = KEY_F6;
			break;
			case VEEX_KEY_HELP:
				if(stat != 1) key_send(KEY_F3, 1);
				stat = 1;
				keycode = KEY_F3;
			break;
			case VEEX_KEY_FILE:
				if(stat != 1) key_send(KEY_F1, 1);
				stat = 1;
				keycode = KEY_F1;
			break;
			case VEEX_KEY_HISTORY:
				if(stat != 1) key_send(KEY_F4, 1);
				stat = 1;
				keycode = KEY_F4;
			break;
			default:
				if(stat == 1){
					key_send(keycode, 0);
				}
				stat = 0;
			break;
		}
		usleep(100000);
	}
}

int main(int argc, char *argv[] )
{
	unsigned char fan = 0;
	char temp = 0;
	int ret = 0;
	sem_t * sem_id;


	if(argc != 3){
		printf("usage: ux400-vfl power blink\n");
		exit(0);
	}

	if((ret = sys_init())<0)
	{
		printf("LIBFTDI init failed, exit\n");
		exit(1);
	}


	sem_id = sem_open(UX400_SEM_CPLD, O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH, 1);
	if(sem_id == SEM_FAILED) {
		perror("UX400 VFL sem_open");
		exit(1);
	}

	if(sem_wait(sem_id) < 0) {
		perror("UX400 VFL sem_wait");
		exit(1);
	}

	if(atoi(argv[1]) == 0){
		vfl_pwr(1, 0);
	}

	vfl_pwr(atoi(argv[1]), atoi(argv[2]));

	if(sem_post(sem_id) < 0) {
		perror("UX400 VFL sem_post");
	}
}





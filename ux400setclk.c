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

#include "ftdi.h"

#include <sys/wait.h>
#include <sys/stat.h>	/* For mode constants */
#include <semaphore.h>

#define	UX400VENDOR	0x0403
#define	UX400PRODUCT	0x6010
#define	UX400DES	"USB <-> Serial Converter"
#define	UX400_SEM_CPLD	"UX400_SEM_CPLD"
#define	UX400_LOCAL_SN		"USBLOCALBUS01"

#define	CONT_REG1		0x00
#define	CONT_REG3		0x02
#define CONT_REG5	       0x04

#define ATOMIC_10M  1
#define ATOMIC_1PPS 2
#define GPS_1PPS    3

#define OPMPW_ON		5
#define	REG_CPLDVER		0x15

struct ftdi_context ux400_ftdic;

int Read_bus(struct ftdi_context * handle, unsigned char haddr, unsigned char laddr, unsigned char * buff, unsigned int len);
int Write_bus(struct ftdi_context * handle, unsigned char haddr, unsigned char laddr, unsigned char * buff, unsigned int len);

int sys_init();


char cpldver(void)
{
	unsigned char data;
	int ret = 0;

	if((ret = Read_bus(&ux400_ftdic, 0x00, REG_CPLDVER, &data, 1) < 0))
	{
		printf("Read CPLD version failed.\n");
		return -1;
	}else{
		/*printf("CPLD VERSION: 0x%x\n", data);*/
	}

	return data;
}


int opm_pwr(unsigned int on)
{

	unsigned char data, temp;
	int ret;

	if((ret = Read_bus(&ux400_ftdic, 0x00, CONT_REG1, &data, 1)) < 0)
	{
		printf("Read OPM reg failed!\n");
		return -1;
	}
	/*printf("old data = 0x%x\n", data);*/
	if(on == 1){
		data |= 0x01<<OPMPW_ON;
	}else{
		data &= ~(0x01<<OPMPW_ON);
	}

	if((ret = Write_bus(&ux400_ftdic, 0x00, CONT_REG1, &data, 1)) < 0)
	{
		printf("Write OPM reg failed!\n");
		return -1;
	}

	if((ret = Read_bus(&ux400_ftdic, 0x00, CONT_REG1, &data, 1)) < 0)
	{
		printf("Read OPM reg failed!\n");
		return -1;
	}
	/*printf("new data = 0x%x\n", data);*/
	
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

int gpio_set(unsigned char gpio, unsigned char reg)
{
	int ret = 0;

	if(( ret = Write_bus(&ux400_ftdic, 0x00, reg, &gpio, 1)) < 0)
	{
		printf("Write GPIO reg failed!\n");
		return -1;
	}

	return 0;
}

int gpio_get(unsigned char *gpio, unsigned char reg)
{
	int ret = 0;

	if(( ret = Read_bus(&ux400_ftdic, 0x00, reg, gpio, 1)) < 0)
	{
		printf("Read GPIO reg failed!\n");
		return -1;
	}

	return 0;
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

int clock_select(unsigned char reg, unsigned char bit, int clock)
{
	unsigned char temp = 0;
	int ret;

	if((ret = gpio_get(&temp, reg))<0){
		printf("Read gpio failed, exit\n");
		return -1;
	}
	/*printf("old temp = 0x%x\n", temp);*/

	switch (clock) {
		case ATOMIC_10M:
			temp |= (0x1<<bit);
			break;
		case ATOMIC_1PPS:
			temp &= ~(0x1<<bit);
			temp &= ~(0x1<<(bit + 1));
			break;
		case GPS_1PPS:
			temp &= ~(0x1<<bit);
			temp |= (0x1<<(bit + 1));
			break;	
		default:
		printf("Undefined clock, please select correct clock!\n");
		return -1;
	}
	/*printf("calculate temp = 0x%x\n", temp);*/

	if((ret = gpio_set(temp, reg))<0){
		printf("Read gpio failed, exit\n");
		return -1;
	}

	if((ret = gpio_get(&temp, reg))<0){
		printf("Read gpio failed, exit\n");
		return -1;
	}
	/*printf("new read temp = 0x%x\n", temp);*/

}

int main(int argc, char *argv[] )
{
	unsigned char reg = 0;
	unsigned char bit = 0;
	char temp = 0;
	int ret = 0;
	int clock = 0;
	sem_t * sem_id;

	if(argc != 3){
		printf("usage: ux400setclk LA/LB/LC/RA/RB/RC 1/2/3\n");
		printf("       1:ATOMIC_10M\n");
		printf("       2:ATOMIC_1PPS\n");
		printf("       3:GPS_1PPS\n");
		exit(1);
	}

	if((ret = sys_init())<0)
	{
		printf("LIBFTDI init failed, exit\n");
	}

	temp = cpldver();
	if(temp < 0 )
	{
		printf("Read CPLD version failed, exit\n");
		exit(1);
	}

	/*printf("argv[1] = %s\n", argv[1]);*/

	if (strcmp(argv[1], "LA") == 0) {	 /*1g-ge*/
		reg = CONT_REG5;
		bit = 4;
	} else if (strcmp(argv[1], "LB") == 0) {  /*10g*/
		reg = CONT_REG5;
		bit = 2;
	} else if (strcmp(argv[1], "LC") == 0) {  /*40g*/
		reg = CONT_REG5;
		bit = 0;
	} else if (strcmp(argv[1], "RA") == 0) {  /*Memory*/
		reg = CONT_REG3;
		bit = 6;
	} else if (strcmp(argv[1], "RB") == 0) {  /*SDH*/
		reg = CONT_REG5;
		bit = 6;
	} else if (strcmp(argv[1], "RC") == 0) {  /*Spare*/
		reg = CONT_REG3;
		bit = 2;
	} else {
		printf("Unknow slot, usage: ux400setclk LA/LB/LC/RA/RB/RC 1/2/3\n", temp);
		exit(1);
	}

	clock = atoi(argv[2]);
	/*printf("clock = 0x%x\n", clock);*/

	sem_id = sem_open(UX400_SEM_CPLD, O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH, 1);
	if(sem_id == SEM_FAILED) {
		perror("UX400 OPM sem_open");
		exit(1);
	}

	if(sem_wait(sem_id) < 0) {
		perror("UX400 OPM sem_wait");
		exit(1);
	}

	if ((ret = clock_select(reg, bit, clock)) < 0 ) {
		printf("Clock select error\n");
		if(sem_post(sem_id) < 0) {
			perror("UX400 OPM sem_post");
		}
		exit(1);
	}

	if(sem_post(sem_id) < 0) {
		perror("UX400 OPM sem_post");
		exit(1);
	}

	ftdi_usb_close(&ux400_ftdic);
	ftdi_deinit(&ux400_ftdic);

	return 0;
}

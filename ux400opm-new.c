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


#define	UX400VENDOR	0x0403
#define	UX400PRODUCT	0x6010
#define	UX400DES	"USB <-> Serial Converter"

#define	OPM_ON		1
#define	OPM_OFF		0


#define	UX400_LOCAL_SN		"USBLOCALBUS01"

#define	BACKLIGHT_OFFSET	0x01
#define	POWERKEY_OFFSET		0x08
#define	KEYOFFSET		0x0A

#define	REG1_OFFSET		0x00
#define	REG_GPIO_OFFSET		0x03
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
		printf("CPLD VERSION: 0x%x\n", data);
	}

	return data;
}


int opm_pwr(unsigned int on)
{

	unsigned char data, temp;
	int ret;

	if((ret = Read_bus(&ux400_ftdic, 0x00, REG1_OFFSET, &data, 1)) < 0)
	{
		printf("Read OPM reg failed!\n");
		return -1;
	}

	if(on == 1){
		data |= 0x20;
	}else{
		data &= ~(0x20);
	}

	if((ret = Write_bus(&ux400_ftdic, 0x00, REG1_OFFSET, &data, 1)) < 0)
	{
		printf("Write OPM reg failed!\n");
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


int main(int argc, char *argv[] )
{
	unsigned char fan = 0;
	char temp = 0;
	int ret = 0;

	if(argc != 2){
		printf("usage: ux400opm 1/0\n");
		exit(0);
	}

	if((ret = sys_init())<0)
	{
		printf("LIBFTDI init failed, exit\n");
		exit(1);
	}

	temp = cpldver();
	if(temp < 0 )
	{
		printf("Read CPLD version failed, exit\n");
		exit(1);
	}

	if(atoi(argv[1]) == 1){
		opm_pwr(OPM_ON);
	}else{
		opm_pwr(OPM_OFF);
	}

	ftdi_usb_close(&ux400_ftdic);
	ftdi_deinit(&ux400_ftdic);
}





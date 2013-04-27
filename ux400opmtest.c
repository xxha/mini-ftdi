/*
	UX400 OPM module simple access.


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


#define	UX400_RS232_SN		"USB2RS232"

#define	UX400_RS232_A		1
#define	UX400_RS232_B		2

#define	OPM_BAUDRATE		9600


struct ftdi_context ux400_opm_ftdic;

static int Read_bus(struct ftdi_context * handle, unsigned char haddr, unsigned char laddr, unsigned char * buff, unsigned int len);
static int Write_bus(struct ftdi_context * handle, unsigned char haddr, unsigned char laddr, unsigned char * buff, unsigned int len);

static int ux400_opm_sys_init();

static int Read_bus(struct ftdi_context * handle, unsigned char haddr, unsigned char laddr, unsigned char * buff, unsigned int len)
{
	unsigned char cmd [4] = { 0x00 };
	int ret = 0;

	cmd[0] = 0x91;
	cmd[1] = haddr;
	cmd[2] = laddr;
	cmd[3] = 0x87;

	if((ret = ftdi_usb_purge_rx_buffer (handle)) < 0)
	{
	    fprintf(stderr, "ftdi_usb_purge_rx_buffer failed: %d (%s)\n", ret, ftdi_get_error_string(&ux400_opm_ftdic));
	    return EXIT_FAILURE;
	}

	if((ret = ftdi_usb_purge_tx_buffer (handle)) < 0)
	{
	    fprintf(stderr, "ftdi_usb_purge_tx_buffer failed: %d (%s)\n", ret, ftdi_get_error_string(&ux400_opm_ftdic));
	    return EXIT_FAILURE;
	}

	if((ret = ftdi_write_data (handle, cmd, 4)) < 0)
	{
	    fprintf(stderr, "ftdi_write_data failed: %d (%s)\n", ret, ftdi_get_error_string(&ux400_opm_ftdic));
	    return EXIT_FAILURE;
	}

	if((ret = ftdi_read_data (handle, buff, len)) < 0)
	{
	    fprintf(stderr, "ftdi_read_data failed: %d (%s)\n", ret, ftdi_get_error_string(&ux400_opm_ftdic));
	    return EXIT_FAILURE;
	}

	return ret;
}

static int Write_bus(struct ftdi_context * handle, unsigned char haddr, unsigned char laddr, unsigned char * buff, unsigned int len)
{
	unsigned char cmd [4] = { 0x00 };
	int ret = 0;
	int i = 0;

	if((ret = ftdi_usb_purge_tx_buffer (handle)) < 0)
	{
	    fprintf(stderr, "ftdi_usb_purge_tx_buffer failed: %d (%s)\n", ret, ftdi_get_error_string(&ux400_opm_ftdic));
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
		    fprintf(stderr, "ftdi_write_data failed: %d (%s)\n", ret, ftdi_get_error_string(&ux400_opm_ftdic));
		    return EXIT_FAILURE;
		}
	}

	return ret;
}


static int ux400_opm_sys_init()
{
	int ret, i;
	struct ftdi_device_list *devlist, *curdev;
	char manufacturer[128], description[128], serialno[128];

	if (ftdi_init(&ux400_opm_ftdic) < 0)
	{
		fprintf(stderr, "ftdi_init failed\n");
		return EXIT_FAILURE;
	}

	ftdi_set_interface(&ux400_opm_ftdic, UX400_RS232_B);

	if((ret = ftdi_usb_open_desc(&ux400_opm_ftdic, UX400VENDOR, UX400PRODUCT, UX400DES, UX400_RS232_SN)) < 0)
	{
	    fprintf(stderr, "ftdi_usb_open_desc failed: %d (%s)\n", ret, ftdi_get_error_string(&ux400_opm_ftdic));
	    return EXIT_FAILURE;
	}

	// Set baudrate
	ret = ftdi_set_baudrate(&ux400_opm_ftdic, OPM_BAUDRATE);
	if (ret < 0)
	{
		fprintf(stderr, "unable to set baudrate: %d (%s)\n", ret, ftdi_get_error_string(&ux400_opm_ftdic));
		return EXIT_FAILURE;
	}

	return 0;
}

static void ux400_opm_sys_exit()
{
	ftdi_usb_close(&ux400_opm_ftdic);
	ftdi_deinit(&ux400_opm_ftdic);
}

static int ux400_opm_read_data(unsigned char * buf, int size)
{
	int ret = 0;

	ret = ftdi_read_data(&ux400_opm_ftdic, buf, size);

	return ret;
}

static int ux400_opm_write_data(unsigned char * buf, int size)
{
	int ret = 0;

	ret = ftdi_write_data(&ux400_opm_ftdic, buf, size);

	return ret;
}

int main(int argc, char *argv[] )
{
	int ret = 0;
	char buf[1024];

	if((ret = ux400_opm_sys_init()) < 0)
	{
		printf("LIBFTDI init failed, exit\n");
		exit(1);
	}

	if ((ret = ux400_opm_write_data("PWR_?>", sizeof("PWR_?>"))) >= 0)
	{
		if(ret > 0){
			fprintf(stderr, "write %d bytes\n", ret);
		}
	}

	usleep(100000);

	if ((ret = ftdi_read_data(&ux400_opm_ftdic, buf, sizeof(buf))) >= 0) {
		if(ret > 0){
			fprintf(stderr, "read %d bytes\n", ret);
			fwrite(buf, ret, 1, stdout);
			fflush(stderr);
			fflush(stdout);
		}
		printf("\n");
	}

	if ((ret = ux400_opm_write_data("WAV_?>", sizeof("WAV_?>"))) >= 0)
	{
		if(ret > 0){
			fprintf(stderr, "write %d bytes\n", ret);
		}
	}

	usleep(100000);

	if ((ret = ftdi_read_data(&ux400_opm_ftdic, buf, sizeof(buf))) >= 0) {
		if(ret > 0){
			fprintf(stderr, "read %d bytes\n", ret);
			fwrite(buf, ret, 1, stdout);
			fflush(stderr);
			fflush(stdout);
		}
		printf("\n");
	}

	ux400_opm_sys_exit();

}





/*
	H-EMbed-TKU Q6 Module
	LED Control
	
	Label  | GPIO
	--------------
	BACK   | 21
	HOME   | 16
	MENU   | 20
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define GPIO(BANK, IO) (BANK-1)*32+IO
#define SYSFS_GPIO_DIR "/sys/class/gpio"
#define MAX_BUF 64

#define LED_BACK GPIO(1, 21)
#define LED_HOME GPIO(1, 16)
#define LED_MENU GPIO(1, 20)

#define DELAY 50000

int gpio_export(unsigned int gpio);
int gpio_unexport(unsigned int gpio);
int gpio_set_dir(unsigned int gpio, int value);
int gpio_set_value(unsigned int gpio, int value);

int gpio_export(unsigned int gpio) {
	int fd, len;
	char buf[MAX_BUF];

	fd = open(SYSFS_GPIO_DIR "/export", O_WRONLY);
	if (fd < 0) {
		printf("gpio/export\n");
		return 1;
	}

	len = snprintf(buf, sizeof(buf), "%d", gpio);
	write(fd, buf, len);
	close(fd);

	return 0;
}

int gpio_unexport(unsigned int gpio) {
	int fd, len;
	char buf[MAX_BUF];

	fd = open(SYSFS_GPIO_DIR "/unexport", O_WRONLY);
	if (fd < 0) {
		printf("gpio/unexport\n");
		return 1;
	}

	len = snprintf(buf, sizeof(buf), "%d", gpio);
	write(fd, buf, len);
	close(fd);

	return 0;
}

int gpio_set_dir(unsigned int gpio, int dir) {
	int fd;
	char buf[MAX_BUF];

	snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/direction", gpio);
	fd = open(buf, O_WRONLY);
	if (fd < 0) {
		printf("gpio/direction\n");
		return 1;
	}
	
	if (dir)
		write(fd, "out\n", 4);
	else
		write(fd, "in\n", 3);

	close(fd);
	return 0;
}

int gpio_set_value(unsigned int gpio, int value) {
	int fd;
	char buf[MAX_BUF];

	snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", gpio);
	fd = open(buf, O_WRONLY);
	if (fd < 0) {
		printf("gpio/set-value\n");
		return 1;
	}

	if (value == 1) {
		write(fd, "1\n", 2);
	} else {
		write(fd, "0\n", 2);
	}

	close(fd);
	return 0;
}

int main(int argc, char *argv[])
{
	printf("Application Started!\n");

	int led[3] = {LED_BACK, LED_HOME, LED_MENU};

	for (int i = 0; i < 3; i++) {
		gpio_export(led[i]);
		gpio_set_dir(led[i], 1);
	}
	
	while (1)
	{
		for (int i = 0; i < 3; i++) {
			usleep(DELAY);
			gpio_set_value(led[i], 1);
		}

		for (int i = 0; i < 3; i++) {
			usleep(DELAY);
			gpio_set_value(led[i], 0);
		}
	}
}
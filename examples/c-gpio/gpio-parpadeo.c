// SPDX-License-Identifier: Apache-2.0
/*
 * gpio-parpadeo - libgpiod 2.x example for the Waveshare ESP32-P4-WIFI6.
 *
 *   gpio-parpadeo GPIO [VECES [MS]]          blink a header GPIO (10 x 250 ms)
 *   gpio-parpadeo -l GPIO [pullup|pulldown]  read it as an input once
 *
 * GPIO is the chip pin number printed on the board (2..52).  The P4 has two
 * GPIO banks, each its own gpiochip: GPIO0..31 -> bank 0 (line = GPIO),
 * GPIO32..56 -> bank 1 (line = GPIO - 32).  The gpiochip number Linux gives
 * each bank depends on probe order, so it is looked up from sysfs.
 *
 * libgpiod 2 speaks the kernel's GPIO character-device ABI v2 (this kernel
 * has no ABI v1: CONFIG_GPIO_CDEV_V1 is off, libgpiod 1.x gets EINVAL).
 *
 * Wiring for the LED test: GPIO -> 330 ohm -> LED anode, LED cathode -> GND.
 */
#include <dirent.h>
#include <errno.h>
#include <gpiod.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t stop;

static void on_signal(int sig)
{
	(void)sig;
	stop = 1;
}

/* "/dev/gpiochipN" of bank 0 or 1, from /sys/bus/platform/devices/... */
static int bank_chip_path(int bank, char *path, size_t len)
{
	char dir[96];
	struct dirent *e;
	DIR *d;

	snprintf(dir, sizeof(dir),
		 "/sys/bus/platform/devices/500e0000.gpio-bank%d", bank);
	d = opendir(dir);
	if (!d)
		return -1;
	while ((e = readdir(d))) {
		if (!strncmp(e->d_name, "gpiochip", 8)) {
			snprintf(path, len, "/dev/%s", e->d_name);
			closedir(d);
			return 0;
		}
	}
	closedir(d);
	return -1;
}

/*
 * Request one header GPIO as input (bias) or output (initial value 0).
 * Returns the request (one line, offset in *line) or NULL.
 */
static struct gpiod_line_request *request_gpio(int gpio, int output,
					       enum gpiod_line_bias bias,
					       unsigned int *line)
{
	struct gpiod_line_request *req = NULL;
	struct gpiod_request_config *rcfg = NULL;
	struct gpiod_line_settings *ls = NULL;
	struct gpiod_line_config *lcfg = NULL;
	struct gpiod_chip *chip = NULL;
	char path[PATH_MAX];
	int bank = gpio >= 32;

	if (gpio < 0 || gpio > 56) {
		fprintf(stderr, "GPIO %d out of range (0..56)\n", gpio);
		return NULL;
	}
	if (bank_chip_path(bank, path, sizeof(path))) {
		fprintf(stderr, "GPIO bank %d is not registered (DTB without it?)\n", bank);
		return NULL;
	}
	*line = gpio - 32 * bank;

	chip = gpiod_chip_open(path);
	if (!chip) {
		fprintf(stderr, "%s: %s\n", path, strerror(errno));
		return NULL;
	}
	ls = gpiod_line_settings_new();
	lcfg = gpiod_line_config_new();
	rcfg = gpiod_request_config_new();
	if (!ls || !lcfg || !rcfg) {
		fprintf(stderr, "out of memory\n");
		goto out;
	}
	if (output) {
		gpiod_line_settings_set_direction(ls, GPIOD_LINE_DIRECTION_OUTPUT);
		gpiod_line_settings_set_output_value(ls, GPIOD_LINE_VALUE_INACTIVE);
	} else {
		gpiod_line_settings_set_direction(ls, GPIOD_LINE_DIRECTION_INPUT);
		gpiod_line_settings_set_bias(ls, bias);
	}
	if (gpiod_line_config_add_line_settings(lcfg, line, 1, ls)) {
		fprintf(stderr, "line config: %s\n", strerror(errno));
		goto out;
	}
	gpiod_request_config_set_consumer(rcfg, "gpio-parpadeo");
	req = gpiod_chip_request_lines(chip, rcfg, lcfg);
	if (!req)
		fprintf(stderr, "GPIO%d: %s (reserved or in use?)\n", gpio, strerror(errno));
out:
	if (rcfg)
		gpiod_request_config_free(rcfg);
	if (lcfg)
		gpiod_line_config_free(lcfg);
	if (ls)
		gpiod_line_settings_free(ls);
	gpiod_chip_close(chip);	/* the request keeps its own file descriptor */
	return req;
}

int main(int argc, char **argv)
{
	struct gpiod_line_request *req;
	unsigned int line;

	if (argc >= 3 && !strcmp(argv[1], "-l")) {
		enum gpiod_line_bias bias = GPIOD_LINE_BIAS_AS_IS;
		enum gpiod_line_value v;

		if (argc > 3 && !strcmp(argv[3], "pullup"))
			bias = GPIOD_LINE_BIAS_PULL_UP;
		else if (argc > 3 && !strcmp(argv[3], "pulldown"))
			bias = GPIOD_LINE_BIAS_PULL_DOWN;
		req = request_gpio(atoi(argv[2]), 0, bias, &line);
		if (!req)
			return 1;
		v = gpiod_line_request_get_value(req, line);
		gpiod_line_request_release(req);
		if (v == GPIOD_LINE_VALUE_ERROR) {
			fprintf(stderr, "read: %s\n", strerror(errno));
			return 1;
		}
		printf("GPIO%s = %d\n", argv[2], v == GPIOD_LINE_VALUE_ACTIVE);
		return 0;
	}

	if (argc < 2 || argc > 4) {
		fprintf(stderr, "usage: %s GPIO [TIMES [MS]] | -l GPIO [pullup|pulldown]\n", argv[0]);
		return 2;
	}
	int gpio = atoi(argv[1]);
	int times = argc > 2 ? atoi(argv[2]) : 10;
	int ms = argc > 3 ? atoi(argv[3]) : 250;

	req = request_gpio(gpio, 1, GPIOD_LINE_BIAS_AS_IS, &line);
	if (!req)
		return 1;
	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);
	printf("blinking GPIO%d: %d times, %d ms (Ctrl-C stops)\n", gpio, times, ms);
	for (int i = 0; i < times && !stop; i++) {
		gpiod_line_request_set_value(req, line, GPIOD_LINE_VALUE_ACTIVE);
		usleep(ms * 1000);
		gpiod_line_request_set_value(req, line, GPIOD_LINE_VALUE_INACTIVE);
		usleep(ms * 1000);
	}
	gpiod_line_request_release(req);	/* output stays low */
	return 0;
}

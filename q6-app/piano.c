/*
	H-EMbed-TKU Q6 Module
	Piano Program
	
	Button Input Device
	/dev/input/event1
*/

#include <linux/input.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>

#define IOCTL_START_BUZZER      _IOW('b', 0x07, int) // 부저 켜기
#define IOCTL_END_BUZZER        _IOW('b', 0x09, int) // 부저 끄기
#define IOCTL_SET_TONE          _IOW('b', 0x0b, int) // 부저 톤 설정
#define IOCTL_SET_VOLUME        _IOW('b', 0x0c, int) // 부저 볼륨 설정
#define IOCTL_GET_TONE          _IOW('b', 0x0d, int) // 부저 톤 읽기
#define IOCTL_GET_VOLUME        _IOW('b', 0x0e, int) // 부저 볼륨 일기

long freqToTone(double freq);
void startBuzzer();
void changeTone(long tone, int volume);
void stopBuzzer();

int buzzer_fd;

long freqToTone(double freq) {
	double tone;
	tone = (1.0f / freq) * 1000000000;
	return (long) tone;
}

void startBuzzer() {
	// 부져 켜기
	ioctl(buzzer_fd, IOCTL_START_BUZZER, 0);
};

void stopBuzzer() {
	// 부저 끄기
	ioctl(buzzer_fd, IOCTL_END_BUZZER, 0);
};

void changeTone(long tone, int volume) {
	// 명령줄 입력값 출력 및 설정
	ioctl(buzzer_fd, IOCTL_SET_VOLUME, volume);
 	ioctl(buzzer_fd, IOCTL_SET_TONE, tone);
	printf("Tone: %lu, Volume: %d\n", tone, volume);
}

int main(int argc, char **argv)
{
	// 부저 디바이스의 File Descriptor 읽기
	buzzer_fd = open("/dev/buzzer", O_RDONLY);

	// File descriptor 변수 선언
	int fd;

	// 해당 디바이스의 File descriptor 가져오기
	fd = open("/dev/input/event1", O_RDONLY);

	// input_event 구조체 변수 선언
	struct input_event ev;

	// 프로그램 시작 알림 출력
	printf("Application Started!\n");

	// 무한 루프
	while (1)
	{
		// 해당 디바이스에서 input_event 구조체 만큼 읽기
		read(fd, &ev, sizeof(struct input_event));

		// Event Type 이 1 (EV_KEY) 일 경우
		if (ev.type == 1) {
			// 해당 Event 의 Code 와 Value 를 출력한다.
			printf("Key Code [%i] = State is %i\n", ev.code, ev.value);

			long tone;

			switch (ev.code)
			{
				case 158:
					tone = freqToTone(523);
					break;
				case 172:
					tone = freqToTone(587);
					break;
				case 139:
					tone = freqToTone(659);
					break;
			}

			changeTone(tone, 10000);

			if (ev.value == 1) {
				startBuzzer(); 
			} else {
				stopBuzzer();
			}
		}
	}
}
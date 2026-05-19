/*
	H-EMbed-TKU Q6 Module
	Buzzer Control
	
	Button Input Device
	/dev/buzzer
*/


#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define IOCTL_START_BUZZER      _IOW('b', 0x07, int) // 부저 켜기
#define IOCTL_END_BUZZER        _IOW('b', 0x09, int) // 부저 끄기
#define IOCTL_SET_TONE          _IOW('b', 0x0b, int) // 부저 톤 설정
#define IOCTL_SET_VOLUME        _IOW('b', 0x0c, int) // 부저 볼륨 설정
#define IOCTL_GET_TONE          _IOW('b', 0x0d, int) // 부저 톤 읽기
#define IOCTL_GET_VOLUME        _IOW('b', 0x0e, int) // 부저 볼륨 일기

long freqToTone(double freq);
void playTone(long tone, int volume, int time);

long freqToTone(double freq) {
	double tone;
	tone = (1.0f / freq) * 1000000000;
	return (long) tone;
}

void playTone(long tone, int volume, int time) {
	// 부저 디바이스의 File Descriptor 읽기
	int buzzer_fd = open("/dev/buzzer", O_RDONLY);

	// 명령줄 입력값 출력 및 설정
	ioctl(buzzer_fd, IOCTL_SET_VOLUME, volume);
 	ioctl(buzzer_fd, IOCTL_SET_TONE, tone);
	printf("Tone: %lu, Volume: %d\n", tone, volume);
	
	// 부져 켜기
	ioctl(buzzer_fd, IOCTL_START_BUZZER, 0);

	// 딜레이
	usleep(time);

	// 부저 끄기
	ioctl(buzzer_fd, IOCTL_END_BUZZER, 0);

	// File descriptor 닫기
	close(buzzer_fd);
}

int main(int argc, char **argv)
{
	double TONEHZ[8] = {523, 587, 659, 698, 783, 880, 987, 1046};

	for (int i = 0; i < 8; i++) {
		long tone = freqToTone((TONEHZ[i]));
		playTone(tone, 25000, 100 * 1000); 
	}

	return 0;
}
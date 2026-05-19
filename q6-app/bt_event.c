/*
	H-EMbed-TKU Q6 Module
	Button Control
	
	Button Input Device
	/dev/input/event1
*/

#include <linux/input.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	// File descriptor 변수 선언
	int fd;

	// 인자 체크
	if (argc < 2) {
		printf("./button <device>\n Example: ./button /dev/input/event1\n");
		return 1;
	}

	// 해당 디바이스의 File descriptor 가져오기
	fd = open(argv[1], O_RDONLY);

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
		if(ev.type == 1)
			// 해당 Event 의 Code 와 Value 를 출력한다.
			printf("Key Code [%i] = State is %i\n", ev.code, ev.value);
	}
}

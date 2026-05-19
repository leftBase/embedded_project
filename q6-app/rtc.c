/*
	H-EMbed-TKU Q6 Module
	Clock Program
	
	Button Input Device
	/dev/input/event1
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <linux/input.h>

/*
  linux_rtc_time 구조체 선언  
*/

struct linux_rtc_time {
    int tm_sec;         // Seconds          [0-59]
    int tm_min;         // Minutes          [0-59]
    int tm_hour;        // Hours            [0-23]
    int tm_mday;        // Day              [0-31]   
    int tm_mon;         // Month            [0-11]
    int tm_year;        // Year             [1900 ~]
    int tm_wday;        // Day of week      [0-6]
    int tm_yday;        // Days in year     [0-365]
    int tm_isdst;       // DST              [-1/0/1]
};

/*
    IOCTL 통신을 위한 명령 정의

    _IOW( 매직번호, 구분번호, 변수형 ) : 디바이스 드라이버에서 데이터를 쓰기
    위한 명령을 만드는 매크로 이다.
*/

#define RTC_RD_TIME     _IOR('p', 0x09, struct linux_rtc_time)

int main(int argc, char **argv)
{
    // FIle Descriptor, IOCTL 변수 선언
    int fd, rc;

    // linux_rtc_time 구조체 변수 선언
    struct linux_rtc_time st;

    // RTC 디바이스 열기
    fd = open("/dev/rtc", O_RDONLY);

    // RTC 로 부터 데이터 읽기
    rc = ioctl(fd, RTC_RD_TIME, &st);

    // 데이터 출력하기
    printf("%d년 %d월 %d일 %d시 %d분 %d초\n", (st.tm_year + 1900), (st.tm_mon + 1), st.tm_mday, (st.tm_hour + 9), st.tm_min, st.tm_sec);
}
/* Zero-order test for looking at the DCF77 module and see if it gives us a signal.
 * Works by sampling CTS at 100 Hz and drawing ascii representation (one line/sec).
 * Only requires an FTDI cable plus this program running on the host computer.
 * Note: there is no need to use this program if you have a LED and a resistor.
 * But I did not...
 */

#include <termios.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <strings.h>

#define BAUDRATE B115200
#define DEVICE "/dev/ttyUSB0"

int main()
{
   int fd, c, res;
   struct termios oldtio, newtio;
   char buf[255];

   fd = open(DEVICE, O_RDWR | O_NOCTTY);
   if (fd < 0)
   {
      perror("open");
      exit(-1);
   }

   tcgetattr(fd, &oldtio);

   bzero(&newtio, sizeof(newtio));
   newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
   newtio.c_iflag = IGNPAR | IGNBRK;
   newtio.c_oflag = 0;

   // set input mode (non-canonical, no echo, ...)
   newtio.c_lflag = 0;
   newtio.c_cc[VTIME] = 0;
   newtio.c_cc[VMIN]= 0;

   tcsetattr(fd, TCSANOW, &newtio);
   tcflush(fd, TCIFLUSH);
   tcflush(fd, TCOFLUSH);

   const int count = 100;
   long delay = 1000000000 / count;
   struct timeval tv_prev;
   gettimeofday(&tv_prev, NULL);

   int j = 0;
   int n_high = 0;
   while (1) {

      if (++j == 100)
      {
         printf("    %3d/%3d\n", n_high, j);
         n_high = j = 0;

         char outstr[200];
         struct timeval tv;
         gettimeofday(&tv, NULL);

         long delta_s = tv.tv_sec - tv_prev.tv_sec;
         long delta_us = tv.tv_usec - tv_prev.tv_usec;
         long error_us = 1E6 * (delta_s - 1) + delta_us;
         delay -= 1000 * error_us / count;
         tv_prev = tv;

         struct tm *tmp = localtime(&tv.tv_sec);
         if (strftime(outstr, sizeof(outstr), "%T", tmp) == 0)
         {
            perror("strftime");
            exit(-1);
         }
         printf("%s.%06d    ", outstr, tv.tv_usec);
      }

      int stat;
      if (ioctl (fd, TIOCMGET, &stat) < 0)
      {
         perror("ioctl TIOCMGET");
         exit(-1);
      }
      if (stat & TIOCM_CTS)
      {
         ++n_high;
         printf("#");
      }
      else
         printf("_");
      //fflush(NULL);

      struct timespec ts;
      ts.tv_sec = 0;
      ts.tv_nsec = delay;
      nanosleep (&ts, NULL);
   }

   tcsetattr(fd, TCSANOW, &oldtio);
}

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <libgen.h>
#include <pthread.h>

#include <simavr/sim_avr.h>
#include <simavr/sim_elf.h>
#include <simavr/avr_ioport.h>
#include <simavr/avr_timer.h>
#include <simavr/sim_hex.h>
#include <simavr/sim_gdb.h>
#include <simavr/sim_vcd_file.h>

#include <simavr/parts/uart_pty.h>
#include "button.h"
#include "dcf77_rcvr.h"
#include "dht22_sensor.h"
#include "lcd_display.h"

enum {
   B_MODE = 0, B_SET, B_ADJ,
   B_COUNT
};
button_t button [B_COUNT];
volatile int button_state [B_COUNT] = { 0, 0, 0 };

avr_t * avr = NULL;
uart_pty_t uart_pty;
dcf77_rcvr_t dcf77_rcvr;
dht22_sensor_t dht22_sensor;
lcd_display_t lcd_display;

void
pwm_changed_hook (struct avr_irq_t * irq, uint32_t value, void * param)
{
   printf ("pwm: %s=%d\n", (char*)param, value);
}

void *
avr_run_thread(void * ignore)
{
   int button_state_local [3] = { 0, 0, 0 };

   while (1) {
      int state = avr_run (avr);
      if (state == cpu_Done || state == cpu_Crashed)
         break;

      for (int i = 0; i < B_COUNT; i++) {
         if (button_state [i] != button_state_local [i]) {
            button_state_local [i] = button_state [i];
            if (button_state_local [i])
            {
               printf ("Button %i pressed\n", i);
               button_press (&button [i], 0);
            }
            else
            {
               printf ("Button %i released\n", i);
               button_release (&button [i]);
            }
         }
      }
   }
   return NULL;
}

int
main (int argc, char *argv[])
{
   int sim2gui_fds [2];
   int gui2sim_fds [2];
   pipe (sim2gui_fds);
   pipe (gui2sim_fds);
   pid_t pid = fork ();
   if (pid < 0)
   {
      perror ("fork");
      exit (pid);
   }
   else if (pid == 0) // child
   {
      dup2 (sim2gui_fds [0], STDIN_FILENO);
      dup2 (gui2sim_fds [1], STDOUT_FILENO);
      close (sim2gui_fds [1]);
      close (gui2sim_fds [0]);
      char* args [] = { (char*)"gui", NULL };
      if (execvp ("./gui", args) < 0)
      {
         perror ("execvp");
         exit (-1);
      }
   }

   // parent
   FILE* gui_in = fdopen (sim2gui_fds [1], "w");
   FILE* gui_out = fdopen (gui2sim_fds [0], "r");
   close (sim2gui_fds [0]);
   close (gui2sim_fds [1]);

   elf_firmware_t f;
   const char * fname = "firmware.axf";
   int debug = 0;
   elf_read_firmware (fname, &f);

   printf ("firmware %s f=%d mmcu=%s\n", fname, (int) f.frequency, f.mmcu);

   avr = avr_make_mcu_by_name (f.mmcu);
   if (!avr) {
      fprintf (stderr, "%s: AVR '%s' not known\n", argv[0], f.mmcu);
      exit (1);
   }

   avr_init (avr);
   avr_load_firmware (avr, &f);

   // initialize virtual hardware

   button_init (avr, &button [B_MODE], "button.mode");
   avr_connect_irq (button [B_MODE].irq + IRQ_BUTTON_OUT,
                    avr_io_getirq (avr, AVR_IOCTL_IOPORT_GETIRQ ('B'), 4));
   button_init (avr, &button [B_SET], "button.set");
   avr_connect_irq (button [B_SET].irq + IRQ_BUTTON_OUT,
                    avr_io_getirq (avr, AVR_IOCTL_IOPORT_GETIRQ ('B'), 2));
   button_init (avr, &button[B_ADJ], "button.adj");
   avr_connect_irq (button [B_ADJ].irq + IRQ_BUTTON_OUT,
                    avr_io_getirq (avr, AVR_IOCTL_IOPORT_GETIRQ ('B'), 1));

   uart_pty_init (avr, &uart_pty);
   uart_pty_connect (&uart_pty, '0');

   dcf77_rcvr_init (avr, &dcf77_rcvr);
   avr_irq_t* i_dcf77_power = avr_io_getirq (avr, AVR_IOCTL_IOPORT_GETIRQ ('D'), 7);
   avr_irq_t* i_dcf77_data = avr_io_getirq (avr, AVR_IOCTL_IOPORT_GETIRQ ('B'), 0);
   avr_connect_irq (i_dcf77_power, dcf77_rcvr.irq + IRQ_DCF77_POWER);
   avr_connect_irq (dcf77_rcvr.irq + IRQ_DCF77_DATA, i_dcf77_data);

   avr_irq_t* i_dht22_power = avr_io_getirq (avr, AVR_IOCTL_IOPORT_GETIRQ ('D'), 4);
   avr_irq_t* i_dht22_data = avr_io_getirq (avr, AVR_IOCTL_IOPORT_GETIRQ ('D'), 2);
   dht22_sensor_init (avr, &dht22_sensor, i_dht22_data);

   //dht22_sensor_set (&dht22_sensor, 652, 351);  //  65.2% RH,  35.1'C
   dht22_sensor_set (&dht22_sensor, 356, 237);  //  35.6% RH,  23.7'C
   //dht22_sensor_set (&dht22_sensor, 1000, -157);  // 100.0% RH, -15.7'C

   // we connect the MCU pins -> sensor module pins, but the sensor module
   // will drive the MCU pin for DATA directly.
   avr_connect_irq (i_dht22_power, dht22_sensor.irq + IRQ_DHT22_POWER);
   avr_connect_irq (i_dht22_data, dht22_sensor.irq + IRQ_DHT22_DATA);

   lcd_display_init (avr, &lcd_display, gui_in);
   avr_irq_t* i_lcd_d0 = avr_io_getirq (avr, AVR_IOCTL_IOPORT_GETIRQ ('C'), 0);
   avr_irq_t* i_lcd_d1 = avr_io_getirq (avr, AVR_IOCTL_IOPORT_GETIRQ ('C'), 1);
   avr_irq_t* i_lcd_d2 = avr_io_getirq (avr, AVR_IOCTL_IOPORT_GETIRQ ('C'), 2);
   avr_irq_t* i_lcd_clk = avr_io_getirq (avr, AVR_IOCTL_IOPORT_GETIRQ ('C'), 3);
   avr_irq_t* i_lcd_com = avr_io_getirq (avr, AVR_IOCTL_IOPORT_GETIRQ ('C'), 4);
   avr_irq_t* i_lcd_oe_ = avr_io_getirq (avr, AVR_IOCTL_IOPORT_GETIRQ ('C'), 5);
   avr_connect_irq (i_lcd_d0, lcd_display.irq + IRQ_LCD_D0);
   avr_connect_irq (i_lcd_d1, lcd_display.irq + IRQ_LCD_D1);
   avr_connect_irq (i_lcd_d2, lcd_display.irq + IRQ_LCD_D2);
   avr_connect_irq (i_lcd_clk, lcd_display.irq + IRQ_LCD_CLK);
   avr_connect_irq (i_lcd_com, lcd_display.irq + IRQ_LCD_COM);
   avr_connect_irq (i_lcd_oe_, lcd_display.irq + IRQ_LCD_OE_);

   avr_irq_t * i_pwm_g =
      avr_io_getirq (avr, AVR_IOCTL_TIMER_GETIRQ ('0'), TIMER_IRQ_OUT_PWM1);
   avr_irq_register_notify (i_pwm_g, pwm_changed_hook, "green");
   avr_irq_t * i_pwm_b =
      avr_io_getirq (avr, AVR_IOCTL_TIMER_GETIRQ ('2'), TIMER_IRQ_OUT_PWM0);
   avr_irq_register_notify (i_pwm_b, pwm_changed_hook, "blue");
   avr_irq_t * i_pwm_r =
      avr_io_getirq (avr, AVR_IOCTL_TIMER_GETIRQ ('2'), TIMER_IRQ_OUT_PWM1);
   avr_irq_register_notify (i_pwm_r, pwm_changed_hook, "red");

   // even if not setup at startup, activate gdb if crashing
   avr->gdb_port = 1234;
   if (debug) {
      avr->state = cpu_Stopped;
      avr_gdb_init (avr);
   }

   avr_vcd_add_signal (avr->vcd, button [B_MODE].irq + IRQ_BUTTON_OUT,
                       1/*bits*/, "BTN_Mode");
   avr_vcd_add_signal (avr->vcd, button [B_SET].irq + IRQ_BUTTON_OUT,
                       1/*bits*/, "BTN_Set");
   avr_vcd_add_signal (avr->vcd, button [B_ADJ].irq + IRQ_BUTTON_OUT,
                       1/*bits*/, "BTN_Adj");

   avr_vcd_add_signal (avr->vcd, i_dcf77_data, 1/*bits*/, "DCF77_DATA");
   avr_vcd_add_signal (avr->vcd, i_dht22_data, 1/*bits*/, "DHT22_DATA");

   avr_vcd_add_signal (avr->vcd, i_pwm_r, 8/*bits*/, "PWM_R");
   avr_vcd_add_signal (avr->vcd, i_pwm_g, 8/*bits*/, "PWM_G");
   avr_vcd_add_signal (avr->vcd, i_pwm_b, 8/*bits*/, "PWM_B");
   avr_vcd_start (avr->vcd);

   avr_raise_irq(button[B_MODE].irq + IRQ_BUTTON_OUT, 1);
   avr_raise_irq(button[B_SET].irq + IRQ_BUTTON_OUT, 1);
   avr_raise_irq(button[B_ADJ].irq + IRQ_BUTTON_OUT, 1);

   pthread_t run;
   pthread_create(&run, NULL, avr_run_thread, NULL);

   char buf [128];
   while (fgets (buf, sizeof (buf) - 1, gui_out))
   {
      size_t len = strlen (buf);
      if (len && buf [len - 1] == '\n')
         buf [len - 1] = '\0';

      if (!strcmp (buf, "button press 1"))
         button_state [B_MODE] = 1;
      else if (!strcmp (buf, "button release 1"))
         button_state [B_MODE] = 0;
      else if (!strcmp (buf, "button press 2"))
         button_state [B_SET] = 1;
      else if (!strcmp (buf, "button release 2"))
         button_state [B_SET] = 0;
      else if (!strcmp (buf, "button press 3"))
         button_state [B_ADJ] = 1;
      else if (!strcmp (buf, "button release 3"))
         button_state [B_ADJ] = 0;
      else
         printf ("gui: %s\n", buf);
   }

   fclose (gui_in); // send EOF to child
   fclose (gui_out);
}

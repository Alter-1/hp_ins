/* 

Copyright (c) 2022 Alexandr A. Telyatnikov (Alter)

Abstract:
    This is user-mode keyboard filter with clears HP-specific keyboard input from Ctrl/Alt/Meta around
    special keys and replaces Hangup with Insert

    Must be run as root

Author:
    Alexander A. Telyatnikov (Alter)

Licence:
    GPLv2

*/

#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libevdev/libevdev-uinput.h>

int g_verbose = 0;

void send_key_event(struct libevdev_uinput *dev,
		    unsigned int code, int value) {

	if(g_verbose) {
    	    printf("     type: 0x%x code: %d value: %x\n",
    	       EV_KEY, code, value
    	       );
    	}

	libevdev_uinput_write_event(dev,
				    EV_KEY,
				    code,
				    value);
	libevdev_uinput_write_event(dev,
				    EV_SYN,
				    SYN_REPORT,
				    0);

}

#define SCAN_SCREEN    0x6d    // Ctrl_L + Alt_L + Screen
#define SCAN_ANSWER    0x66    // Ctrl_L + Answer + Meta_L
#define SCAN_HANGUP    0x65    // Ctrl_L + Alt_L + Hangup

#define TIME_HUMAN_THRESHOLD   1000  // 1ms, 1000us

int main(int argc, char **argv) {
	struct input_event event;

	int kbd_fd;
	int uinput_fd;

	struct libevdev *kbd_dev;
	struct libevdev_uinput *virtkbd_dev;

	const char *virtkbd_path;

	int i=1;
	for(; i<argc; i++)
	{
	    if(!strcmp(argv[i], "-v"))
	    {
	        g_verbose = 1;
	    } else {
	        break;
	    }
	}
	if(i >= argc)
	{
	    printf("Missing keyboard event device\n"
	           "  check /proc/bus/input/devices\n" 
	           "    Handlers=sysrq kbd event4 leds \n"
	           "  means /dev/input/event4\n"
	        );
	    return -1;
	}
	
	kbd_fd = open(argv[i], O_RDONLY);
	if(kbd_fd <= 0)
	{
	    printf("Can't open %s\n",
	        argv[i]);
	    return -1;
	}
	libevdev_new_from_fd(kbd_fd, &kbd_dev);

	uinput_fd = open("/dev/uinput", O_RDWR);	
	if(uinput_fd <= 0)
	{
	    printf("Can't create virtual keyboard event instance\n");
	    return -1;
	}
	libevdev_uinput_create_from_device(kbd_dev,
					   uinput_fd,
					   &virtkbd_dev);

	virtkbd_path = libevdev_uinput_get_devnode(virtkbd_dev);
	printf("Virtual keyboard device: %s\n",
	       virtkbd_path);

	usleep(1000000);
	int grab = 1;
	ioctl(kbd_fd, EVIOCGRAB, &grab);

	struct timeval cur_event_time = {0};
	struct timeval prev_event_time = {0};
	long long delta;

	// queued press
	int alt_state = 0;
	int ctrl_state = 0;

	int drop_meta = 0;
	int drop_ctrl = 0;  // Ctrl may be not sent with EV_KEY, only with EV_MSC after special key release
	int drop_alt = 0;  // Alt may be not sent with EV_KEY, only with EV_MSC after special key release
	int hp_insert_pressed = 0;
	int hp_num_pressed = 0;
	int hp_break_pressed = 0;

	// current (expected) state
	int alt_pressed = 0;
	int ctrl_pressed = 0;

	int is_our, is_special;
	int sz;
	
	for (;;) {
	        event.type = 0;
		sz = read(kbd_fd, &event, sizeof(event));
		/*
		if (event.type != EV_KEY || event.value > 1 ||
		    event.code == KEY_CAPSLOCK) {
			continue;
		}
		*/

		if(sz<=0 || event.type == 0) {
		    // should never get here
		    usleep(50000);
		    continue;
		}

		is_our = 0;
    	        is_special = 0;

		if(event.type == EV_MSC && event.code == MSC_SCAN) {
		    if(event.value == SCAN_SCREEN ||
		       event.value == SCAN_ANSWER ||
		       event.value == SCAN_HANGUP)
		     {
		         is_special = 1;
		     }
		}

		if((event.type == EV_KEY) || is_special) {
        	    prev_event_time = cur_event_time;
        	    cur_event_time = event.time;
		}

		if(event.type == EV_KEY && event.value == 1) {
		    // press
		    if(drop_meta && event.code == KEY_LEFTMETA)
		    {
		        //drop_meta = 0; clear on release
		        continue;
		    }
		    if(event.code == KEY_LEFTCTRL || event.code == KEY_LEFTALT) {
		        is_our = 1;
		    }
		}

		delta = (cur_event_time.tv_sec - prev_event_time.tv_sec)*1000000L + (cur_event_time.tv_usec - prev_event_time.tv_usec);

		if(g_verbose) {
		    printf("<%ld.%06ld> type: 0x%x code: %d value: %x (delta %d, our %d, alt/ctrl_state %x/%x (%x/%x), drop_alt/ctrl %d/%d, sz %d)\n",
		       event.time.tv_sec, event.time.tv_usec,
		       event.type, event.code, event.value,
		       (unsigned int)delta, is_our, alt_state, ctrl_state, alt_pressed, ctrl_pressed, drop_alt, drop_ctrl, sz
		       );
		}

		if(event.type == EV_KEY && delta >= TIME_HUMAN_THRESHOLD)
		{
		    // definitly human input, send queued Ctrl/Alt and continue
		    if(ctrl_state || alt_state) {
		        // send queued, older (with greater value) first.
		        // We try to keep order Ctrl Alt vs Alt Ctrl

		        // update, press state, but but not clear here.
		        ctrl_pressed |= ctrl_state;
		        alt_pressed |= alt_state;

                        send_key_event(virtkbd_dev,
                       	       (ctrl_state >= alt_state) ? KEY_LEFTCTRL : KEY_LEFTALT,
                       	       1);
		        usleep(TIME_HUMAN_THRESHOLD*2); // 2 ms
		        if(ctrl_state >= alt_state) {
		            ctrl_state = 0;
		        } else {
		            alt_state = 0;
		        }
		    }
		    if(ctrl_state || alt_state) {
                        send_key_event(virtkbd_dev,
                       	       ctrl_state ? KEY_LEFTCTRL : KEY_LEFTALT,
                       	       1);
		        usleep(TIME_HUMAN_THRESHOLD*2); // 2 ms
		        
		        ctrl_state = 0;
		        alt_state = 0;
		        // and now send original keyboard event
		    }

		} 

		if(is_our)
		{
		    // queue Ctrl/Alt to make sure that it is human input with significant delay between events
		    if(event.code == KEY_LEFTCTRL) {
		        ctrl_state = 1;
		        alt_state <<= 1;
		    }
		    if(event.code == KEY_LEFTALT) {
		        alt_state = 1;
		        ctrl_state <<= 1 ;
		    }
    	            continue;
		}

		if(is_special)
                {
                    // We get one of HP-specific 'phone' keys

                    // drop flags and send proper key (e.g. Insert) if necessary or drop sequence completly
                    alt_state = 0;
                    ctrl_state = 0;

                    switch(event.value) {
                        case SCAN_SCREEN:
                            //replace with ScrollLock
                            // just drop ctrl/alt
                            hp_num_pressed = !hp_num_pressed;
                            send_key_event(virtkbd_dev,
                     		       KEY_SCROLLLOCK,
                     		       hp_num_pressed);
                     	    drop_ctrl = ctrl_pressed;
                     	    drop_alt = alt_pressed;
                     	    break;

                        case SCAN_ANSWER:
                            //replace with Break
                            // just drop ctrl/alt and remember to drop Meta_L
                            drop_meta = 1;
                     	    //drop_ctrl = hp_break_pressed;
                            hp_break_pressed = !hp_break_pressed;
                            send_key_event(virtkbd_dev,
                     		       KEY_PAUSE,
                     		       hp_break_pressed);
                     	    drop_ctrl = ctrl_pressed;
                     	    drop_alt = alt_pressed;
                     	    break;
                            
                        case SCAN_HANGUP:
                            // replace with Insert
                            // handle Press/Release
                            hp_insert_pressed = !hp_insert_pressed;
                            send_key_event(virtkbd_dev,
                     		       KEY_INSERT,
                     		       hp_insert_pressed);
                     	    drop_ctrl = ctrl_pressed;
                     	    drop_alt = alt_pressed;
                            break;
                    } // end switch()
                    continue;
                }

		if (event.type != EV_KEY || event.value > 1) // drop raw scan codes and repeated keys (event.value=2)
		{
                    if(event.type == EV_MSC && event.code == MSC_SCAN  && !(hp_insert_pressed || hp_num_pressed || hp_break_pressed)) {
                        // special case: if we had Ctrl/Alt pressed *before* our special key, we shall NOT get 2nd release with EV_KEY. But will get EV_MSC
        		if(event.value == KEY_LEFTCTRL) {  // 0x1d (29)
                            if(ctrl_pressed && drop_ctrl)
                            {
                                ctrl_pressed = 0;
        		        drop_ctrl = 0;
                                send_key_event(virtkbd_dev,
                         		       KEY_LEFTCTRL,
                         		       0);
                     	    }
                     	    continue;
        		}
        		if(event.value == KEY_LEFTALT) {  
                            if(alt_pressed && drop_alt)
                            {
                                alt_pressed = 0;
        		        drop_alt = 0;
                                send_key_event(virtkbd_dev,
                         		       KEY_LEFTALT,
                         		       0);
                     	    }
                     	    continue;
        		}
        	    }	
		    continue;
		}

		if(event.type == EV_KEY && event.value == 0) {
		    // release
		    if(drop_meta && event.code == KEY_LEFTMETA)
		    {
		        drop_meta = 0;
		        continue;
		    }
		    // send release only if was pressed by human
		    if(event.code == KEY_LEFTCTRL)
		    {
		        if(drop_ctrl) {  // drop keyboard-generated release coming with special buttons
		            continue;
		        }
		        if(!ctrl_pressed)
		            continue;
		        ctrl_pressed = 0;
		    }
		    if(event.code == KEY_LEFTALT)
		    {
		        if(drop_alt) {  // drop keyboard-generated release coming with special buttons
		            continue;
		        }
		        if(!alt_pressed)
		            continue;
		        alt_pressed = 0;
		    }
		}

		send_key_event(virtkbd_dev, event.code, event.value);
	}
	return 0;
}

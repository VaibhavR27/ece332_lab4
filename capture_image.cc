#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdint.h>
#include "bmp_utility.h"
#include <unistd.h>   // For close(), read(), write()
using ::close;
using ::open;


#define HW_REGS_BASE (0xff200000)
#define HW_REGS_SPAN (0x00200000)
#define HW_REGS_MASK (HW_REGS_SPAN - 1)
#define LED_BASE 0x1000
#define PUSH_BASE 0x3010
#define VIDEO_BASE 0x0000

#define IMAGE_WIDTH 320
#define IMAGE_HEIGHT 240

#define FPGA_ONCHIP_BASE     (0xC8000000)
#define IMAGE_SPAN (IMAGE_WIDTH * IMAGE_HEIGHT * 4)
#define IMAGE_MASK (IMAGE_SPAN - 1)

#define KEY_BASE 0xFF200050
#define VIDEO_IN_BASE 0xFF203060
#define SW_BASE 0xFF200040
#define STRIDE 512


int main(void) {
    volatile unsigned int *key_ptr = NULL;
    volatile unsigned int *video_in_dma = NULL;
    volatile unsigned char *video_mem = NULL;
    volatile unsigned int *sw_ptr = NULL;
    void *virtual_base;
    int fd;
    int image_count = 0;

    // Open /dev/mem
    if ((fd = open("/dev/mem", (O_RDWR | O_SYNC))) == -1) {
        printf("ERROR: could not open \"/dev/mem\"...\n");
        return 1;
    }

    // Map physical memory into virtual address space
    virtual_base = mmap(NULL, HW_REGS_SPAN, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, HW_REGS_BASE);
    if (virtual_base == MAP_FAILED) {
        printf("ERROR: mmap() failed...\n");
        close(fd);
        return 1;
    }

    // Calculate the virtual address where our device is mapped
    video_in_dma = (unsigned int *)((uint8_t *)virtual_base + ((VIDEO_BASE) & (HW_REGS_MASK)));
    key_ptr = (unsigned int *)((uint8_t *)virtual_base + ((PUSH_BASE) & (HW_REGS_MASK)));
    video_mem = (unsigned char *)((uint8_t *)virtual_base + ((FPGA_ONCHIP_BASE) & (HW_REGS_MASK)));
    sw_ptr = (unsigned int *)((uint8_t *)virtual_base + ((SW_BASE) & (HW_REGS_MASK)));


    int value = *(video_in_dma+3);

    printf("Video In DMA register updated at:0%p\n",(video_in_dma));

    // Modify the PIO register
    *(video_in_dma+3) = 0x4;
    //*h2p_lw_led_addr = *h2p_lw_led_addr + 1;

    value = *(video_in_dma+3);

    printf("enabled video:0x%x\n",value);

	printf("KEY value: 0x%x\n", *key_ptr);
	fflush(stdout);
   	unsigned short image_buffer[IMAGE_HEIGHT][IMAGE_WIDTH];


	while(1){
        // key other than key0 is changed
        if ((*key_ptr) != 7) {
		printf("paused video\n");
                *(video_in_dma+3) = 0x0;
                while ( (*key_ptr) != 7 );
                image_count++;
		// Copy frame into local buffer
        for (int y = 0; y < IMAGE_HEIGHT; y++) {
            for (int x = 0; x < IMAGE_WIDTH; x++) {
                int offset = (y << 9) + (x);
                image_buffer[y][x] = *(volatile unsigned short *)(video_mem + offset);
            }
        }

                printf("captured image\n");
		char filename_color[64];  
                sprintf(filename_color, "final_image_color_%d.bmp", image_count);
                char filename_bw[64];  
                sprintf(filename_bw, "final_image_bw_%d.bmp", image_count);

                saveImageShort(filename_color, &image_buffer[0][0],IMAGE_WIDTH,IMAGE_HEIGHT);
                saveImageGrayscale(filename_bw,(const unsigned char *)&image_buffer[0][0],IMAGE_WIDTH,IMAGE_HEIGHT);

                if ((*key_ptr) == 7) {
                        *(video_in_dma + 3) = 0x0; 
                        *(video_in_dma + 3) = 0x4; 
                	printf("resumed video\n");
		}
        }
    }
    // Clean up
    if (munmap(virtual_base, IMAGE_SPAN) != 0) {
        printf("ERROR: munmap() failed...\n");
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}

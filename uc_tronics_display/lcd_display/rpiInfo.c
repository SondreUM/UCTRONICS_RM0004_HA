#include "rpiInfo.h"

#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <math.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <unistd.h>

#include "st7735.h"

/**
 * @brief Get the IP address of wlan0 or eth0
 *
 * @param ip_addr buffer to store the IP address, must be at least 16 bytes long
 */
void get_ip_address(char *ip_addr) {
    int fd, symbol = 0;
    struct ifreq ifr;
    char *buffer = "xxx.xxx.xxx.xxx";

    // set default value
    strncpy(ip_addr, buffer, strlen(buffer));

    fd = socket(AF_INET, SOCK_DGRAM, 0);

    // if failed to open socket, return default value
    if (fd == -1) {
        perror("Socket creation error");
        return;
    }
    ifr.ifr_addr.sa_family = AF_INET;

    if (IPADDRESS_TYPE == ETH0_ADDRESS) {  // get the IP address of eth0
        strncpy(ifr.ifr_name, "eth0", IFNAMSIZ - 1);
    } else if (IPADDRESS_TYPE == WLAN0_ADDRESS) {
        // get the IP address of wlan0
        strncpy(ifr.ifr_name, "wlan0", IFNAMSIZ - 1);
    }
    symbol = ioctl(fd, SIOCGIFADDR, &ifr);
    close(fd);
    if (symbol == 0) {
        strncpy(ip_addr, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr),
                strlen(buffer));
    }
}

/*
 * get ram memory
 */
void get_cpu_memory(float *Totalram, float *freeram) {
    struct sysinfo s_info;

    unsigned int value = 0;
    unsigned char buffer[100] = {0};
    unsigned char famer[100] = {0};
    if (sysinfo(&s_info) == 0)  // Get memory information
    {
        FILE *fp = fopen("/proc/meminfo", "r");
        if (fp == NULL) {
            return;
        }
        while (fgets(buffer, sizeof(buffer), fp)) {
            if (sscanf(buffer, "%s%u", famer, &value) != 2) {
                continue;
            }
            if (strcmp(famer, "MemTotal:") == 0) {
                *Totalram = value / 1000.0 / 1000.0;
            } else if (strcmp(famer, "MemFree:") == 0) {
                *freeram = value / 1000.0 / 1000.0;
            }
        }
        fclose(fp);
    }
}

/*
 * get sd memory
 */
void get_sd_memory(uint32_t *MemSize, uint32_t *freesize) {
    struct statfs diskInfo;
    statfs("/", &diskInfo);
    unsigned long long blocksize = diskInfo.f_bsize;               // The number of bytes per block
    unsigned long long totalsize = blocksize * diskInfo.f_blocks;  // Total number of bytes
    *MemSize = (unsigned int)(totalsize >> 30);

    unsigned long long size =
        blocksize * diskInfo.f_bfree;  // Now let's figure out how much space we have left
    *freesize = size >> 30;
    *freesize = *MemSize - *freesize;
}

/*
 * get hard disk memory
 */
uint8_t get_hard_disk_memory(uint16_t *diskMemSize, uint16_t *useMemSize) {
    *diskMemSize = 0;
    *useMemSize = 0;
    uint8_t diskMembuff[10] = {0};
    uint8_t useMembuff[10] = {0};
    FILE *fd = NULL;
    fd = popen("df -a | grep /dev/sda | awk '{printf \"%s\", $(2)}'", "r");
    fgets(diskMembuff, sizeof(diskMembuff), fd);
    fclose(fd);

    fd = popen("df -a | grep /dev/sda | awk '{printf \"%s\", $(3)}'", "r");
    fgets(useMembuff, sizeof(useMembuff), fd);
    fclose(fd);

    *diskMemSize = atoi(diskMembuff) / 1024 / 1024;
    *useMemSize = atoi(useMembuff) / 1024 / 1024;
}

/*
 * get temperature
 */

uint8_t get_temperature(void) {
    FILE *fd;
    unsigned int temp;
    char buff[10] = {0};
    fd = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    fgets(buff, sizeof(buff), fd);
    sscanf(buff, "%d", &temp);
    fclose(fd);
    return TEMPERATURE_TYPE == FAHRENHEIT ? temp / 1000 * 1.8 + 32 : temp / 1000;
}

/*
 * Get cpu usage
 */
uint8_t get_cpu_usage(void) {
    FILE *fp;
    char cpu_usr_buf[5] = {0};
    char cpy_sys_buf[5] = {0};
    int cpu_usr = 0;
    int cpu_sys = 0;

    /*
     * top in Hassos has names on values than normal ubuntu distro
     * Example:
     *  Mem: 1489420K used, 2392776K free, 1436K shrd, 89600K buff, 676408K cached
     *  CPU:   2% usr   2% sys   0% nic  95% idle   0% io   0% irq   0% sirq
     *  Load average: 0.92 0.92 0.90 2/824 365
     *
     * CPU row in ubuntu is named %Cpu(s)
     */

    fp = popen("top -bn1 | grep -m 1 -e CPU | awk '{printf \"%d\", $(2)}'",
               "r");                              // Gets the load on the CPU
    fgets(cpu_usr_buf, sizeof(cpu_usr_buf), fp);  // Read the user CPU load
    pclose(fp);

    fp = popen("top -bn1 | grep -m 1 -e CPU | awk '{printf \"%d\", $(4)}'",
               "r");                              // Gets the load on the CPU
    fgets(cpy_sys_buf, sizeof(cpy_sys_buf), fp);  // Read the system CPU load
    pclose(fp);

    cpu_usr = atoi(cpu_usr_buf);
    cpu_sys = atoi(cpy_sys_buf);
    return cpu_usr + cpu_sys;
}
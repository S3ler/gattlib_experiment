

/*
 *
 *  GattLib - GATT Library
 *
 *  Copyright (C) 2017  S3ler
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <glib.h>
#include <sys/queue.h>
#include <stdint.h>
#include <unistd.h>

#include <bluetooth.h>
#include <hci.h>
#include <lib/hci_lib.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>

int scan_duration = 10;
const char *peripheral_mac = "00:1A:7D:DA:71:11";
static volatile int signal_received = 0;
LIST_HEAD(listhead, ble_mac_t) g_ble_macs;


#define FLAGS_AD_TYPE 0x01
#define FLAGS_LIMITED_MODE_BIT 0x01
#define FLAGS_GENERAL_MODE_BIT 0x02
#define EIR_FLAGS                   0x01  /* flags */
#define EIR_UUID16_SOME             0x02  /* 16-bit UUID, more available */
#define EIR_UUID16_ALL              0x03  /* 16-bit UUID, all listed */
#define EIR_UUID32_SOME             0x04  /* 32-bit UUID, more available */
#define EIR_UUID32_ALL              0x05  /* 32-bit UUID, all listed */
#define EIR_UUID128_SOME            0x06  /* 128-bit UUID, more available */
#define EIR_UUID128_ALL             0x07  /* 128-bit UUID, all listed */
#define EIR_NAME_SHORT              0x08  /* shortened local name */
#define EIR_NAME_COMPLETE           0x09  /* complete local name */
#define EIR_TX_POWER                0x0A  /* transmit power level */
#define EIR_DEVICE_ID               0x10  /* device ID */


struct ble_mac_t {
    char *addr;
    LIST_ENTRY(ble_mac_t) entries;
};

static void sigint_handler(int sig) {
    signal_received = sig;
}

static int read_flags(uint8_t *flags, const uint8_t *data, size_t size) {
    size_t offset;

    if (!flags || !data)
        return -EINVAL;

    offset = 0;
    while (offset < size) {
        uint8_t len = data[offset];
        uint8_t type;

        /* Check if it is the end of the significant part */
        if (len == 0)
            break;

        if (len + offset > size)
            break;

        type = data[offset + 1];

        if (type == FLAGS_AD_TYPE) {
            *flags = data[offset + 2];
            return 0;
        }

        offset += 1 + len;
    }

    return -ENOENT;
}

static int check_report_filter(uint8_t procedure, le_advertising_info *info) {
    uint8_t flags;

    /* If no discovery procedure is set, all reports are treat as valid */
    if (procedure == 0)
        return 1;

    /* Read flags AD type value from the advertising report if it exists */
    if (read_flags(&flags, info->data, info->length))
        return 0;

    switch (procedure) {
        case 'l': /* Limited Discovery Procedure */
            if (flags & FLAGS_LIMITED_MODE_BIT)
                return 1;
            break;
        case 'g': /* General Discovery Procedure */
            if (flags & (FLAGS_LIMITED_MODE_BIT | FLAGS_GENERAL_MODE_BIT))
                return 1;
            break;
        default:
            fprintf(stderr, "Unknown discovery procedure\n");
    }

    return 0;
}

static void eir_parse_name(uint8_t *eir, size_t eir_len,
                           char *buf, size_t buf_len) {
    size_t offset;

    offset = 0;
    while (offset < eir_len) {
        uint8_t field_len = eir[0];
        size_t name_len;

        /* Check for the end of EIR */
        if (field_len == 0)
            break;

        if (offset + field_len > eir_len)
            goto failed;

        switch (eir[1]) {
            case EIR_NAME_SHORT:
            case EIR_NAME_COMPLETE:
                name_len = field_len - 1;
                if (name_len > buf_len)
                    goto failed;

                memcpy(buf, &eir[2], name_len);
                return;
        }

        offset += field_len + 1;
        eir += field_len + 1;
    }

    failed:
    snprintf(buf, buf_len, "(unknown)");
}


static int print_advertising_devices(int device_descriptor, uint8_t filter_type, int duration) {
    unsigned char buf[HCI_MAX_EVENT_SIZE], *ptr;
    struct hci_filter nf, of;
    struct sigaction sa;
    socklen_t olen;
    int len;

    olen = sizeof(of);
    if (getsockopt(device_descriptor, SOL_HCI, HCI_FILTER, &of, &olen) < 0) {
        printf("Could not get socket options\n");
        return -1;
    }

    hci_filter_clear(&nf);
    hci_filter_set_ptype(HCI_EVENT_PKT, &nf);
    hci_filter_set_event(EVT_LE_META_EVENT, &nf);

    if (setsockopt(device_descriptor, SOL_HCI, HCI_FILTER, &nf, sizeof(nf)) < 0) {
        printf("Could not set socket options\n");
        return -1;
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_flags = SA_NOCLDSTOP;
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);

    time_t start = time(NULL);
    while (1) {
        evt_le_meta_event *meta;
        le_advertising_info *info;
        char addr[18];

        while ((len = read(device_descriptor, buf, sizeof(buf))) < 0) {
            if (errno == EINTR && signal_received == SIGINT) {
                len = 0;
                goto done;
            }

            if (errno == EAGAIN || errno == EINTR)
                continue;
            goto done;
        }

        ptr = buf + (1 + HCI_EVENT_HDR_SIZE);
        len -= (1 + HCI_EVENT_HDR_SIZE);

        meta = (void *) ptr;

        if (meta->subevent != 0x02)
            goto done;

        /* Ignoring multiple reports */
        info = (le_advertising_info *) (meta->data + 1);
        if (check_report_filter(filter_type, info)) {
            char name[30];

            memset(name, 0, sizeof(name));

            ba2str(&info->bdaddr, addr);
            eir_parse_name(info->data, info->length,
                           name, sizeof(name) - 1);

            // HERE
            printf("Discovered: %s %s\n", addr, name);
            // check if connection already in list
            bool not_in_list = true;
            struct ble_mac_t *np;
            for (np = g_ble_macs.lh_first; np != NULL; np = np->entries.le_next) {
                if (strcmp(np->addr, addr) == 0) {
                    not_in_list = false;
                    break;
                }
            }

            if (not_in_list) {
                struct ble_mac_t *connection;

                connection = malloc(sizeof(struct ble_mac_t));
                if (connection == NULL) {
                    fprintf(stderr, "Failt to allocate connection.\n");
                    exit(1);
                }

                connection->addr = strdup(addr);

                LIST_INSERT_HEAD(&g_ble_macs, connection, entries);
            }
        }
        if (time(NULL) > start + duration) {
            // finished scan
            break;
        }
    }

    done:
    setsockopt(device_descriptor, SOL_HCI, HCI_FILTER, &of, sizeof(of));

    if (len < 0) {
        return -1;
    }
    return 0;
}

static void *lescan(void *scan_duration) {

    int duration = *((int *) scan_duration);
    uint8_t own_type = LE_PUBLIC_ADDRESS;
    uint8_t scan_type = 0x01;
    uint8_t filter_type = 0;
    uint8_t filter_policy = 0x00;
    uint16_t interval = htobs(0x0010);
    uint16_t window = htobs(0x0010);
    uint8_t filter_dup = 0x01;

    int dev_id = hci_get_route(NULL);
    int device_descriptor = hci_open_dev(dev_id);

    if (device_descriptor < 0) {
        perror("Could not open device");
        exit(1);
    }

    int err = hci_le_set_scan_parameters(device_descriptor, scan_type, interval, window,
                                         own_type, filter_policy, 10000);
    if (err < 0) {
        perror("Set scan parameters failed");
        exit(1);
    }

    err = hci_le_set_scan_enable(device_descriptor, 0x01, filter_dup, 10000);
    if (err < 0) {
        perror("Enable scan failed");
        exit(1);
    }

    printf("LE Scan ...\n");

    err = print_advertising_devices(device_descriptor, filter_type, duration);
    if (err < 0) {
        perror("Could not receive advertising events");
        exit(1);
    }

    err = hci_le_set_scan_enable(device_descriptor, 0x00, filter_dup, 10000);
    if (err < 0) {
        perror("Disable scan failed");
        exit(1);
    }

    hci_close_dev(device_descriptor);

    return NULL;
}

static struct listhead cmd_lescan(int scan_duration) {
    LIST_INIT(&g_ble_macs);
    pthread_t lescan_thread;
    pthread_create(&lescan_thread, NULL, (void *(*)(void *)) lescan, &scan_duration);
    pthread_join(lescan_thread, NULL);
    return g_ble_macs;
}

static bool ble_mac_found(const char *ble_mac, struct listhead g_ble_macs) {
    for (struct ble_mac_t *np = g_ble_macs.lh_first; np != NULL; np = np->entries.le_next) {
        if (strcmp(np->addr, peripheral_mac) == 0) {
            return true;
        }
    }
    return false;
}

int main(int argc, char *argv[]) {
    // GError *gerr = NULL;
    // GIOChannel *chan;

    struct listhead scan_result = cmd_lescan(scan_duration);
    if (ble_mac_found(peripheral_mac, scan_result)) {
        g_print("Remote Bluetooth address found during scanning\n");
    }
    //interactive(opt_src, opt_dst, opt_dst_type, opt_psm);
    /*
    g_printerr("%s\n", gerr->message);
    g_clear_error(&gerr);
    gboolean got_error = TRUE;

    if (got_error) {
        exit(EXIT_FAILURE);
    } else {
        exit(EXIT_SUCCESS);
    }
    */
    exit(EXIT_SUCCESS);

}
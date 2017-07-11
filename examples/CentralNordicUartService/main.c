/*
 *
 *  GattLib - GATT Library
 *  Copyright (C) 2016  Olivier Martin <olivier@labapart.org>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include <glib.h>
#include <gattlib.h>
#include <sys/queue.h>

#include <readline/readline.h>
#include <sys/signalfd.h>

#include "lib/hci_lib.h"
#include "lib/uuid.h"

#include "att.h"
#include "btio/btio.h"
#include "gattrib.h"
#include "gatt.h"
#include "src/shared/util.h"

int scan_duration = 2;
const char *peripheral_mac = "00:1A:7D:DA:71:11";


#define TX_CHRC_UUID "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
#define RX_CHRC_UUID "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define RX_CCCD_UUID "00002902-0000-1000-8000-00805f9b34fb"

uint16_t nus_rx_notify_handle = 0;
uint16_t nus_rx_handle = 0;
uint16_t nus_tx_handle = 0;
char tx_buffer[20] = {0};

static volatile int signal_received = 0;
LIST_HEAD(listhead, ble_mac_t) g_ble_macs;

static GIOChannel *iochannel = NULL;
static GAttrib *attrib = NULL;
static GMainLoop *event_loop;

static guint prompt_input;
static guint prompt_signal;
static char *prompt = "";


static char *opt_dst = NULL;
static char *opt_src = NULL;
static char *opt_dst_type = NULL;
static char *opt_sec_level = NULL;
static int opt_psm = 0;
static int opt_mtu = 0;
static int start;
static int end;

static enum state {
    STATE_DISCONNECTED,

    STATE_CONNECTING,
    STATE_CONNECTED,

    STATE_HANDLE_CHECKING,
    STATE_HANDLE_READY,

    STATE_TX_VALUE_SAVED,
    STATE_RX_VALUE_CLEARING,

    STATE_RX_NOTIFY_ENABLED,

    STATE_NUS_READY,

    STATE_ERROR
} conn_state;


// lescan defines

#define FLAGS_AD_TYPE               0x01
#define FLAGS_LIMITED_MODE_BIT      0x01
#define FLAGS_GENERAL_MODE_BIT      0x02
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

static bool ble_mac_found(const char *ble_mac, struct listhead *g_ble_macs) {
    for (struct ble_mac_t *np = LIST_FIRST(g_ble_macs); np != NULL; np = LIST_NEXT(np, entries)) {
        if (strcmp(np->addr, ble_mac) == 0) {
            return true;
        }
    }
    return false;
}

static void free_ble_mac_list(struct listhead *g_ble_macs) {
    for (struct ble_mac_t *np = LIST_FIRST(g_ble_macs); np != NULL; np = LIST_NEXT(np, entries)) {
        LIST_REMOVE(np, entries);
        free(np);
    }
}

/* End of Bluetooth Low Energy Scan */
GIOChannel *gatt_connect(const char *src, const char *dst,
                         const char *dst_type, const char *sec_level,
                         int psm, int mtu, BtIOConnect connect_cb,
                         GError **gerr) // ++ gpointer usercontext classe
{
    GIOChannel *chan;
    bdaddr_t sba, dba;
    uint8_t dest_type;
    GError *tmp_err = NULL;
    BtIOSecLevel sec;

    str2ba(dst, &dba);

    /* Local adapter */
    if (src != NULL) {
        if (!strncmp(src, "hci", 3))
            hci_devba(atoi(src + 3), &sba);
        else
            str2ba(src, &sba);
    } else
        bacpy(&sba, BDADDR_ANY);

    /* Not used for BR/EDR */
    if (strcmp(dst_type, "random") == 0)
        dest_type = BDADDR_LE_RANDOM;
    else
        dest_type = BDADDR_LE_PUBLIC;

    if (strcmp(sec_level, "medium") == 0)
        sec = BT_IO_SEC_MEDIUM;
    else if (strcmp(sec_level, "high") == 0)
        sec = BT_IO_SEC_HIGH;
    else
        sec = BT_IO_SEC_LOW;

    if (psm == 0)
        chan = bt_io_connect(connect_cb, NULL, NULL,
                             &tmp_err, // ++ gpointer usercontext classe hier ersten NULL erstezen
                             BT_IO_OPT_SOURCE_BDADDR, &sba,
                             BT_IO_OPT_SOURCE_TYPE, BDADDR_LE_PUBLIC,
                             BT_IO_OPT_DEST_BDADDR, &dba,
                             BT_IO_OPT_DEST_TYPE, dest_type,
                             BT_IO_OPT_CID, ATT_CID,
                             BT_IO_OPT_SEC_LEVEL, sec,
                             BT_IO_OPT_INVALID);
    else
        chan = bt_io_connect(connect_cb, NULL, NULL, &tmp_err,
                             BT_IO_OPT_SOURCE_BDADDR, &sba,
                             BT_IO_OPT_DEST_BDADDR, &dba,
                             BT_IO_OPT_PSM, psm,
                             BT_IO_OPT_IMTU, mtu,
                             BT_IO_OPT_SEC_LEVEL, sec,
                             BT_IO_OPT_INVALID);

    if (tmp_err) {
        g_propagate_error(gerr, tmp_err);
        return NULL;
    }

    return chan;
}


size_t gatt_attr_data_from_string(const char *str, uint8_t **data) {
    // TODO remove me!
    char tmp[3];
    size_t size, i;

    size = strlen(str) / 2;
    *data = g_try_malloc0(size);
    if (*data == NULL)
        return 0;

    tmp[2] = '\0';
    for (i = 0; i < size; i++) {
        memcpy(tmp, str + (i * 2), 2);
        (*data)[i] = (uint8_t) strtol(tmp, NULL, 16);
    }

    return size;
}


static void events_handler(const uint8_t *pdu, uint16_t len, gpointer user_data) {
    uint8_t *opdu;
    uint16_t handle, i, olen;
    size_t plen;
    GString *s;

    handle = get_le16(&pdu[1]);

    switch (pdu[0]) {
        case ATT_OP_HANDLE_NOTIFY:
            s = g_string_new(NULL);
            g_string_printf(s, "Notification handle = 0x%04x value: ",
                            handle);
            if (handle == nus_rx_notify_handle) {
                printf("nus_rx_notify_handle");
            } else if (handle == nus_rx_handle) {
                printf("nus_rx_handle");
            }
            // HERE: notify handle
            break;
        case ATT_OP_HANDLE_IND:
            s = g_string_new(NULL);
            g_string_printf(s, "Indication   handle = 0x%04x value: ",
                            handle);
            break;
        default:
            printf("Command Failed: Invalid opcode\n");
            return;
    }

    for (i = 3; i < len; i++)
        g_string_append_printf(s, "%02x ", pdu[i]);

    printf("%s\n", s->str);
    g_string_free(s, TRUE);

    if (pdu[0] == ATT_OP_HANDLE_NOTIFY)
        return;

    opdu = g_attrib_get_buffer(attrib, &plen);
    olen = enc_confirmation(opdu, plen);

    if (olen > 0)
        g_attrib_send(attrib, 0, opdu, olen, NULL, NULL, NULL);
}

static void disconnect_io() {
    if (conn_state == STATE_DISCONNECTED)
        return;

    g_attrib_unref(attrib);
    attrib = NULL;
    opt_mtu = 0;

    g_io_channel_shutdown(iochannel, FALSE, NULL);
    g_io_channel_unref(iochannel);
    iochannel = NULL;

    conn_state = STATE_DISCONNECTED;
}

static void cmd_init_mqttsn() {
    conn_state = STATE_NUS_READY;
    printf("\n\n Finally -- NUS Ready \n");
}

static void cmd_write_tx_notify_hnd() {

    if (conn_state != STATE_TX_VALUE_SAVED) {
        printf("Command Failed: Not STATE_TX_VALUE_SAVED\n");
        return;
    }

    uint16_t handle = nus_rx_notify_handle;
    if (handle <= 0) {
        printf("Command Failed: A valid handle is required\n");
        return;
    }

    uint8_t value[] = {1, 0};
    int plen = 2;

    gatt_write_cmd(attrib, handle, value, plen, NULL, NULL);

    conn_state = STATE_RX_NOTIFY_ENABLED;
    cmd_init_mqttsn();
}


static void cmd_read_tx_buffer_cb(guint8 status, const guint8 *pdu, guint16 plen,
                                  gpointer user_data) {
    uint8_t value[plen];
    ssize_t vlen;
    int i;
    GString *s;

    /*
    if (conn_state != STATE_RX_VALUE_CLEARING) {
        printf("Command Failed: Not STATE_RX_VALUE_CLEARING\n");
        return;
    }
    */

    if (status != 0) {
        printf("Command Failed: Characteristic value/descriptor read failed: %s\n",
               att_ecode2str(status));
        return;
    }

    vlen = dec_read_resp(pdu, plen, value, sizeof(value));
    if (vlen < 0) {
        printf("Command Failed: Protocol error\n");
        return;
    }

    memset(tx_buffer, 0, sizeof(tx_buffer));
    s = g_string_new("Characteristic value/descriptor: ");
    for (i = 0; i < vlen; i++) {
        g_string_append_printf(s, "%02x ", value[i]);
        tx_buffer[i] = value[i];
    }

    printf("%s\n", s->str);

    g_string_free(s, TRUE);

    conn_state = STATE_TX_VALUE_SAVED;
    cmd_write_tx_notify_hnd();
}

static void cmd_read_tx_buffer() {
    if (conn_state != STATE_HANDLE_READY) {
        printf("Command Failed: Not STATE_HANDLE_READY\n");
        return;
    }

    conn_state = STATE_RX_VALUE_CLEARING;
    gatt_read_char(attrib, nus_rx_handle, cmd_read_tx_buffer_cb, attrib);
}

static void check_characteristic_descriptors(uint8_t status, GSList *descriptors, void *user_data) {
    GSList *l;

    if (status) {
        printf("Command Failed: Discover descriptors failed: %s\n",
               att_ecode2str(status));
        return;
    }

    if (conn_state != STATE_HANDLE_CHECKING) {
        printf("Command Failed: Invalid State - must be STATE_HANDLE_CHECKING\n");
        return;
    }

    bool done = false;
    bool hnd_tx = FALSE;
    bool hnd_rx = FALSE;
    bool hnd_rx_cccd = FALSE;

    for (l = descriptors; l; l = l->next) {
        struct gatt_desc *desc = l->data;
        printf("handle: 0x%04x, uuid: %s\n", desc->handle,
               desc->uuid);
        if (!done) {
            if (strcmp(desc->uuid, TX_CHRC_UUID) == 0) {
                if (!hnd_tx) {
                    hnd_tx = TRUE;
                    nus_tx_handle = desc->handle;
                } else {
                    printf("Command Failed: Discovered duplicate hnd_tx");
                    return;
                }
            }
            if (strcmp(desc->uuid, RX_CHRC_UUID) == 0) {
                if (!hnd_rx) {
                    hnd_rx = TRUE;
                    nus_rx_handle = desc->handle;
                } else {
                    printf("Command Failed: Discovered duplicate hnd_rx");
                    return;
                }
            }
            if (!hnd_rx_cccd && hnd_rx && strcmp(desc->uuid, RX_CCCD_UUID) == 0) {
                if (!hnd_rx_cccd) {
                    hnd_rx_cccd = TRUE;
                    nus_rx_notify_handle = desc->handle;
                }
            }
            if (hnd_tx & hnd_rx & hnd_rx_cccd) {
                done = true;
            }
        }
    }
    if (done) {
        printf("Found all characteristics and CCCDs");
        conn_state = STATE_HANDLE_READY;
        cmd_read_tx_buffer();
        // call next
    } else {
        printf("Command Failed: Did not discover necessary characteristics and CCCDs");
    }
}

static void cmd_check_characteristic_descriptors() {
    if (conn_state != STATE_CONNECTED) {
        printf("Command Failed: Disconnected\n");
        return;
    }

    start = 0x0001;
    end = 0xffff;
    conn_state = STATE_HANDLE_CHECKING;
    gatt_discover_desc(attrib, start, end, NULL, check_characteristic_descriptors, NULL);
}

static void connect_cb(GIOChannel *io, GError *err, gpointer user_data) {
    uint16_t mtu;
    uint16_t cid;

    if (err) {
        conn_state = STATE_DISCONNECTED;
        printf("Command Failed: %s\n", err->message);
        return;
    }

    bt_io_get(io, &err, BT_IO_OPT_IMTU, &mtu,
              BT_IO_OPT_CID, &cid, BT_IO_OPT_INVALID);

    if (err) {
        g_printerr("Can't detect MTU, using default: %s", err->message);
        g_error_free(err);
        mtu = ATT_DEFAULT_LE_MTU;
    }

    if (cid == ATT_CID)
        mtu = ATT_DEFAULT_LE_MTU;

    attrib = g_attrib_new(iochannel, mtu, false);
    g_attrib_register(attrib, ATT_OP_HANDLE_NOTIFY, GATTRIB_ALL_HANDLES,
                      events_handler, attrib, NULL);
    g_attrib_register(attrib, ATT_OP_HANDLE_IND, GATTRIB_ALL_HANDLES,
                      events_handler, attrib, NULL);
    conn_state = STATE_CONNECTED;
    printf("Connection successful\n");

    cmd_check_characteristic_descriptors();
}

static gboolean channel_watcher(GIOChannel *chan, GIOCondition cond,
                                gpointer user_data) {
    disconnect_io();
    return FALSE;
}

static void cmd_connect(const char *ble_peripheral_mac) {
    GError *gerr = NULL;

    if (conn_state != STATE_DISCONNECTED)
        return;

    /*
    if (argcp > 1) {
        g_free(opt_dst);
        opt_dst = g_strdup(argvp[1]);

        g_free(opt_dst_type);
        if (argcp > 2)
            opt_dst_type = g_strdup(argvp[2]);
        else
            opt_dst_type = g_strdup("public");
    }
    */
    opt_dst_type = g_strdup("public");

    if (opt_dst == NULL) {
        printf("Command Failed: Remote Bluetooth address required\n");
        return;
    }

    printf("Attempting to connect to %s\n", opt_dst);
    conn_state = STATE_CONNECTING;
    iochannel = gatt_connect(opt_src, opt_dst, opt_dst_type, opt_sec_level,
                             opt_psm, opt_mtu, connect_cb, &gerr);
    if (iochannel == NULL) {
        conn_state = STATE_DISCONNECTED;
        printf("Command Failed: %s\n", gerr->message);
        g_error_free(gerr);
    } else
        g_io_add_watch(iochannel, G_IO_HUP, channel_watcher, NULL);
}


static void char_write_req_raw_cb(guint8 status, const guint8 *pdu, guint16 plen,
                                  gpointer user_data) {
    if (status != 0) {
        printf("Command Failed: Characteristic Write Request failed: "
                       "%s\n", att_ecode2str(status));
        return;
    }

    if (!dec_write_resp(pdu, plen) && !dec_exec_write_resp(pdu, plen)) {
        printf("Command Failed: Protocol error\n");
        return;
    }

    printf("Characteristic value was written successfully\n");
    conn_state = STATE_NUS_READY;
}

static void cmd_char_write_raw(uint16_t length, uint8_t *data) {

    if (length > 20) {
        printf("Command Failed: Too much data %i\n", length);
        return;
    }

    if (conn_state != STATE_NUS_READY) {
        printf("Command Failed: Not STATE_HANDLE_READY\n");
        return;
    }

    gatt_write_char(attrib, nus_tx_handle, data, (size_t) length,
                    char_write_req_raw_cb, NULL);

}

static void parse_line(char *line_read) {
    char exit_command[] = "exit";
    if (strcmp(line_read, exit_command) == 0) {
        exit(0);
    }

    if (conn_state != STATE_NUS_READY) {
        printf("Not Connnected yet.\n");
        goto done;
    }

    if (line_read == NULL) {
        printf("\n");
        // exit
        rl_callback_handler_remove();
        g_main_loop_quit(event_loop);
        return;
    }

    if (strlen(line_read) > 20) {
        printf("Message is longer then 20 bytes.\n");
        goto done;
    }

    cmd_char_write_raw((uint16_t) strlen(line_read), (uint8_t *) line_read);
    return;

    done:
    free(line_read);
}

static gboolean prompt_read(GIOChannel *chan, GIOCondition cond,
                            gpointer user_data) {
    if (cond & (G_IO_HUP | G_IO_ERR | G_IO_NVAL)) {
        g_io_channel_unref(chan);
        return FALSE;
    }

    rl_callback_read_char();

    return TRUE;
}

static guint setup_standard_input(void) {
    GIOChannel *channel;
    guint source;

    channel = g_io_channel_unix_new(fileno(stdin));

    source = g_io_add_watch(channel,
                            G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
                            prompt_read, NULL);

    g_io_channel_unref(channel);

    return source;
}


static gboolean signal_handler(GIOChannel *channel, GIOCondition condition,
                               gpointer user_data) {
    static unsigned int __terminated = 0;
    struct signalfd_siginfo si;
    ssize_t result;
    int fd;

    if (condition & (G_IO_NVAL | G_IO_ERR | G_IO_HUP)) {
        g_main_loop_quit(event_loop);
        return FALSE;
    }

    fd = g_io_channel_unix_get_fd(channel);

    result = read(fd, &si, sizeof(si));
    if (result != sizeof(si))
        return FALSE;

    switch (si.ssi_signo) {
        case SIGINT:
            rl_replace_line("", 0);
            rl_crlf();
            rl_on_new_line();
            rl_redisplay();
            break;
        case SIGTERM:
            if (__terminated == 0) {
                rl_replace_line("", 0);
                rl_crlf();
                g_main_loop_quit(event_loop);
            }

            __terminated = 1;
            break;
    }

    return TRUE;
}

static guint setup_signalfd(void) {
    GIOChannel *channel;
    guint source;
    sigset_t mask;
    int fd;

    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);

    if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0) {
        perror("Failed to set signal mask");
        return 0;
    }

    fd = signalfd(-1, &mask, 0);
    if (fd < 0) {
        perror("Failed to create signal descriptor");
        return 0;
    }

    channel = g_io_channel_unix_new(fd);

    g_io_channel_set_close_on_unref(channel, TRUE);
    g_io_channel_set_encoding(channel, NULL, NULL);
    g_io_channel_set_buffered(channel, FALSE);

    source = g_io_add_watch(channel,
                            G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
                            signal_handler, NULL);

    g_io_channel_unref(channel);

    return source;
}


static char *get_prompt(void) {
    return prompt;
}

static void init_readline() {
    prompt_input = setup_standard_input(); // input
    prompt_signal = setup_signalfd(); // signal
    rl_erase_empty_line = 1;
    rl_callback_handler_install(get_prompt(), parse_line);
}


int main(int argc, char *argv[]) {

    struct listhead scan_result = cmd_lescan(scan_duration);
    if (!ble_mac_found(peripheral_mac, &scan_result)) {
        g_print("Remote Bluetooth address not found\n");
        goto done;
    }
    g_print("Remote Bluetooth address found during scanning\n");

    opt_sec_level = g_strdup("low");
    opt_src = NULL;
    opt_dst = g_strdup(peripheral_mac);
    opt_dst_type = g_strdup("public");
    opt_psm = 0;

    event_loop = g_main_loop_new(NULL, FALSE);
    cmd_connect(peripheral_mac);
    init_readline();

    g_main_loop_run(event_loop);

    done:
    rl_callback_handler_remove();
    disconnect_io();
    g_source_remove(prompt_input);
    g_source_remove(prompt_signal);
    g_main_loop_unref(event_loop);

    g_free(opt_src);
    g_free(opt_dst);
    g_free(opt_sec_level);

    free_ble_mac_list(&scan_result);

    return 0;
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

}
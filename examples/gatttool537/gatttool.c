/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2010  Nokia Corporation
 *  Copyright (C) 2010  Marcel Holtmann <marcel@holtmann.org>
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

#include <getopt.h>


#include "lib/bluetooth.h"
#include "lib/hci.h"
#include "lib/hci_lib.h"
#include "lib/sdp.h"
#include "lib/uuid.h"

#include "src/shared/util.h"
#include "att.h"
#include "btio/btio.h"
#include "gattrib.h"
#include "gatt.h"
#include "gatttool.h"


static char *opt_src = NULL;
static char *opt_dst = NULL;
static char *opt_dst_type = NULL;
static char *opt_value = NULL;
static char *opt_sec_level = NULL;
static bt_uuid_t *opt_uuid = NULL;
static int opt_start = 0x0001;
static int opt_end = 0xffff;
static int opt_handle = -1;
static int opt_mtu = 0;
static int opt_psm = 0;
static gboolean opt_primary = FALSE;
static gboolean opt_characteristics = FALSE;
static gboolean opt_char_read = FALSE;
static gboolean opt_listen = FALSE;
static gboolean opt_char_desc = FALSE;
static gboolean opt_char_write = FALSE;
static gboolean opt_char_write_req = FALSE;
static gboolean opt_interactive = FALSE;
static GMainLoop *event_loop;
static gboolean got_error = FALSE;
static GSourceFunc operation;

struct characteristic_data {
    GAttrib *attrib;
    uint16_t start;
    uint16_t end;
};

static void events_handler(const uint8_t *pdu, uint16_t len, gpointer user_data) {
    GAttrib *attrib = user_data;
    uint8_t *opdu;
    uint16_t handle, i, olen = 0;
    size_t plen;

    handle = get_le16(&pdu[1]);

    switch (pdu[0]) {
        case ATT_OP_HANDLE_NOTIFY:
            g_print("Notification handle = 0x%04x value: ", handle);
            break;
        case ATT_OP_HANDLE_IND:
            g_print("Indication   handle = 0x%04x value: ", handle);
            break;
        default:
            g_print("Invalid opcode\n");
            return;
    }

    for (i = 3; i < len; i++)
        g_print("%02x ", pdu[i]);

    g_print("\n");

    if (pdu[0] == ATT_OP_HANDLE_NOTIFY)
        return;

    opdu = g_attrib_get_buffer(attrib, &plen);
    olen = enc_confirmation(opdu, plen);

    if (olen > 0)
        g_attrib_send(attrib, 0, opdu, olen, NULL, NULL, NULL);
}

static gboolean listen_start(gpointer user_data) {
    GAttrib *attrib = user_data;

    g_attrib_register(attrib, ATT_OP_HANDLE_NOTIFY, GATTRIB_ALL_HANDLES,
                      events_handler, attrib, NULL);
    g_attrib_register(attrib, ATT_OP_HANDLE_IND, GATTRIB_ALL_HANDLES,
                      events_handler, attrib, NULL);

    return FALSE;
}

static void connect_cb(GIOChannel *io, GError *err, gpointer user_data) {
    GAttrib *attrib;
    uint16_t mtu;
    uint16_t cid;
    GError *gerr = NULL;

    if (err) {
        g_printerr("%s\n", err->message);
        got_error = TRUE;
        g_main_loop_quit(event_loop);
    }

    bt_io_get(io, &gerr, BT_IO_OPT_IMTU, &mtu,
              BT_IO_OPT_CID, &cid, BT_IO_OPT_INVALID);

    if (gerr) {
        g_printerr("Can't detect MTU, using default: %s",
                   gerr->message);
        g_error_free(gerr);
        mtu = ATT_DEFAULT_LE_MTU;
    }

    if (cid == ATT_CID)
        mtu = ATT_DEFAULT_LE_MTU;

    attrib = g_attrib_new(io, mtu, false);

    if (opt_listen)
        g_idle_add(listen_start, attrib);

    operation(attrib); // calls primary
}

static gboolean characteristics(gpointer user_data) ;

static void primary_all_cb(uint8_t status, GSList *services, void *user_data) {
    GSList *l;

    if (status) {
        g_printerr("Discover all primary services failed: %s\n",
                   att_ecode2str(status));
        g_main_loop_quit(event_loop);
        return;
    }

    for (l = services; l; l = l->next) {
        struct gatt_primary *prim = l->data;
        g_print("attr handle = 0x%04x, end grp handle = 0x%04x "
                        "uuid: %s\n", prim->range.start, prim->range.end, prim->uuid);
    }
    operation = characteristics;
//    operation(user_data);
}

static void primary_by_uuid_cb(uint8_t status, GSList *ranges, void *user_data) {
    GSList *l;

    if (status != 0) {
        g_printerr("Discover primary services by UUID failed: %s\n",
                   att_ecode2str(status));
        goto done;
    }

    for (l = ranges; l; l = l->next) {
        struct att_range *range = l->data;
        g_print("Starting handle: %04x Ending handle: %04x\n",
                range->start, range->end);
    }

    done:
    g_main_loop_quit(event_loop);
}

static gboolean primary(gpointer user_data) {
    GAttrib *attrib = user_data;

    if (opt_uuid)
        gatt_discover_primary(attrib, opt_uuid, primary_by_uuid_cb,
                              NULL);
    else
        gatt_discover_primary(attrib, NULL, primary_all_cb, NULL);

    return FALSE;
}

static void char_discovered_cb(uint8_t status, GSList *characteristics,
                               void *user_data) {
    GSList *l;

    if (status) {
        g_printerr("Discover all characteristics failed: %s\n",
                   att_ecode2str(status));
        goto done;
    }

    for (l = characteristics; l; l = l->next) {
        struct gatt_char *chars = l->data;

        g_print("handle = 0x%04x, char properties = 0x%02x, char value "
                        "handle = 0x%04x, uuid = %s\n", chars->handle,
                chars->properties, chars->value_handle, chars->uuid);
    }

    done:
    g_main_loop_quit(event_loop);
}

static gboolean characteristics(gpointer user_data) {
    GAttrib *attrib = user_data;

    gatt_discover_char(attrib, opt_start, opt_end, opt_uuid,
                       char_discovered_cb, NULL);

    return FALSE;
}

static void char_read_cb(guint8 status, const guint8 *pdu, guint16 plen,
                         gpointer user_data) {
    uint8_t value[plen];
    ssize_t vlen;
    int i;

    if (status != 0) {
        g_printerr("Characteristic value/descriptor read failed: %s\n",
                   att_ecode2str(status));
        goto done;
    }

    vlen = dec_read_resp(pdu, plen, value, sizeof(value));
    if (vlen < 0) {
        g_printerr("Protocol error\n");
        goto done;
    }
    g_print("Characteristic value/descriptor: ");
    for (i = 0; i < vlen; i++)
        g_print("%02x ", value[i]);
    g_print("\n");

    done:
    if (!opt_listen)
        g_main_loop_quit(event_loop);
}

static void char_read_by_uuid_cb(guint8 status, const guint8 *pdu,
                                 guint16 plen, gpointer user_data) {
    struct att_data_list *list;
    int i;

    if (status != 0) {
        g_printerr("Read characteristics by UUID failed: %s\n",
                   att_ecode2str(status));
        goto done;
    }

    list = dec_read_by_type_resp(pdu, plen);
    if (list == NULL)
        goto done;

    for (i = 0; i < list->num; i++) {
        uint8_t *value = list->data[i];
        int j;

        g_print("handle: 0x%04x \t value: ", get_le16(value));
        value += 2;
        for (j = 0; j < list->len - 2; j++, value++)
            g_print("%02x ", *value);
        g_print("\n");
    }

    att_data_list_free(list);

    done:
    g_main_loop_quit(event_loop);
}

static gboolean characteristics_read(gpointer user_data) {
    GAttrib *attrib = user_data;

    if (opt_uuid != NULL) {

        gatt_read_char_by_uuid(attrib, opt_start, opt_end, opt_uuid,
                               char_read_by_uuid_cb, NULL);

        return FALSE;
    }

    if (opt_handle <= 0) {
        g_printerr("A valid handle is required\n");
        g_main_loop_quit(event_loop);
        return FALSE;
    }

    gatt_read_char(attrib, opt_handle, char_read_cb, attrib);

    return FALSE;
}

static void mainloop_quit(gpointer user_data) {
    uint8_t *value = user_data;

    g_free(value);
    g_main_loop_quit(event_loop);
}

static gboolean characteristics_write(gpointer user_data) {
    GAttrib *attrib = user_data;
    uint8_t *value;
    size_t len;

    if (opt_handle <= 0) {
        g_printerr("A valid handle is required\n");
        goto error;
    }

    if (opt_value == NULL || opt_value[0] == '\0') {
        g_printerr("A value is required\n");
        goto error;
    }

    len = gatt_attr_data_from_string(opt_value, &value);
    if (len == 0) {
        g_printerr("Invalid value\n");
        goto error;
    }

    gatt_write_cmd(attrib, opt_handle, value, len, mainloop_quit, value);

    g_free(value);
    return FALSE;

    error:
    g_main_loop_quit(event_loop);
    return FALSE;
}

static void char_write_req_cb(guint8 status, const guint8 *pdu, guint16 plen,
                              gpointer user_data) {
    if (status != 0) {
        g_printerr("Characteristic Write Request failed: "
                           "%s\n", att_ecode2str(status));
        goto done;
    }

    if (!dec_write_resp(pdu, plen) && !dec_exec_write_resp(pdu, plen)) {
        g_printerr("Protocol error\n");
        goto done;
    }

    g_print("Characteristic value was written successfully\n");

    done:
    if (!opt_listen)
        g_main_loop_quit(event_loop);
}

static gboolean characteristics_write_req(gpointer user_data) {
    GAttrib *attrib = user_data;
    uint8_t *value;
    size_t len;

    if (opt_handle <= 0) {
        g_printerr("A valid handle is required\n");
        goto error;
    }

    if (opt_value == NULL || opt_value[0] == '\0') {
        g_printerr("A value is required\n");
        goto error;
    }

    len = gatt_attr_data_from_string(opt_value, &value);
    if (len == 0) {
        g_printerr("Invalid value\n");
        goto error;
    }

    gatt_write_char(attrib, opt_handle, value, len, char_write_req_cb,
                    NULL);

    g_free(value);
    return FALSE;

    error:
    g_main_loop_quit(event_loop);
    return FALSE;
}

static void char_desc_cb(uint8_t status, GSList *descriptors, void *user_data) {
    GSList *l;

    if (status) {
        g_printerr("Discover descriptors failed: %s\n",
                   att_ecode2str(status));
        return;
    }

    for (l = descriptors; l; l = l->next) {
        struct gatt_desc *desc = l->data;

        g_print("handle = 0x%04x, uuid = %s\n", desc->handle,
                desc->uuid);
    }

    if (!opt_listen)
        g_main_loop_quit(event_loop);
}

static gboolean characteristics_desc(gpointer user_data) {
    GAttrib *attrib = user_data;

    gatt_discover_desc(attrib, opt_start, opt_end, NULL, char_desc_cb,
                       NULL);

    return FALSE;
}

static gboolean parse_uuid(const char *key, const char *value,
                           gpointer user_data, GError **error) {
    if (!value)
        return FALSE;

    opt_uuid = g_try_malloc(sizeof(bt_uuid_t));
    if (opt_uuid == NULL)
        return FALSE;

    if (bt_string_to_uuid(opt_uuid, value) < 0)
        return FALSE;

    return TRUE;
}

static GOptionEntry primary_char_options[] = {
        {"start", 's', 0,                          G_OPTION_ARG_INT, &opt_start,
                                                                                 "Starting handle(optional)",   "0x0001"},
        {"end",   'e', 0,                          G_OPTION_ARG_INT, &opt_end,
                                                                                 "Ending handle(optional)",     "0xffff"},
        {"uuid",  'u', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK,
                                                                     parse_uuid, "UUID16 or UUID128(optional)", "0x1801"},
        {NULL},
};

static GOptionEntry char_rw_options[] = {
        {"handle", 'a', 0, G_OPTION_ARG_INT,    &opt_handle,
                "Read/Write characteristic by handle(required)", "0x0001"},
        {"value",  'n', 0, G_OPTION_ARG_STRING, &opt_value,
                "Write characteristic value (required for write operation)",
                                                                 "0x0001"},
        {NULL},
};

static GOptionEntry gatt_options[] = {
        {"primary",         0,   0,                     G_OPTION_ARG_NONE, &opt_primary,
                                                                                             "Primary Service Discovery",                   NULL},
        {"characteristics", 0,   0,                     G_OPTION_ARG_NONE, &opt_characteristics,
                                                                                             "Characteristics Discovery",                   NULL},
        {"char-read",       0,   0,                     G_OPTION_ARG_NONE, &opt_char_read,
                                                                                             "Characteristics Value/Descriptor Read",       NULL},
        {"char-write",      0,   0,                     G_OPTION_ARG_NONE, &opt_char_write,
                                                                                             "Characteristics Value Write Without Response (Write Command)",
                                                                                                                                            NULL},
        {"char-write-req",  0,   0,                     G_OPTION_ARG_NONE, &opt_char_write_req,
                                                                                             "Characteristics Value Write (Write Request)", NULL},
        {"char-desc",       0,   0,                     G_OPTION_ARG_NONE, &opt_char_desc,
                                                                                             "Characteristics Descriptor Discovery",        NULL},
        {"listen",          0,   0,                     G_OPTION_ARG_NONE, &opt_listen,
                                                                                             "Listen for notifications and indications",    NULL},
        {"interactive",     'I', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
                                                                           &opt_interactive, "Use interactive mode",                        NULL},
        {NULL},
};

static GOptionEntry options[] = {
        {"adapter",   'i', 0, G_OPTION_ARG_STRING, &opt_src,
                "Specify local adapter interface",          "hciX"},
        {"device",    'b', 0, G_OPTION_ARG_STRING, &opt_dst,
                "Specify remote Bluetooth address",         "MAC"},
        {"addr-type", 't', 0, G_OPTION_ARG_STRING, &opt_dst_type,
                "Set LE address type. Default: public",     "[public | random]"},
        {"mtu",       'm', 0, G_OPTION_ARG_INT,    &opt_mtu,
                "Specify the MTU size",                     "MTU"},
        {"psm",       'p', 0, G_OPTION_ARG_INT,    &opt_psm,
                "Specify the PSM for GATT/ATT over BR/EDR", "PSM"},
        {"sec-level", 'l', 0, G_OPTION_ARG_STRING, &opt_sec_level,
                "Set security level. Default: low",         "[low | medium | high]"},
        {NULL},
};


#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

/* Unofficial value, might still change */
#define LE_LINK		0x80

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


LIST_HEAD(listhead, ble_mac_t) g_ble_macs;

struct ble_mac_t {
    char *addr;
    LIST_ENTRY(ble_mac_t) entries;
};




#define for_each_opt(opt, long, short) while ((opt=getopt_long(argc, argv, short ? short:"+", long, NULL)) != -1)
static volatile int signal_received = 0;



static struct option lescan_options[] = {
        { "help",	0, 0, 'h' },
        { "static",	0, 0, 's' },
        { "privacy",	0, 0, 'p' },
        { "passive",	0, 0, 'P' },
        { "whitelist",	0, 0, 'w' },
        { "discovery",	1, 0, 'd' },
        { "duplicates",	0, 0, 'D' },
        { 0, 0, 0, 0 }
};

static void sigint_handler(int sig)
{
    signal_received = sig;
}

static void eir_parse_name(uint8_t *eir, size_t eir_len,
                           char *buf, size_t buf_len)
{
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


static int read_flags(uint8_t *flags, const uint8_t *data, size_t size)
{
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

static int check_report_filter(uint8_t procedure, le_advertising_info *info)
{
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

static int print_advertising_devices(int dd, uint8_t filter_type)
{
    unsigned char buf[HCI_MAX_EVENT_SIZE], *ptr;
    struct hci_filter nf, of;
    struct sigaction sa;
    socklen_t olen;
    int len;

    olen = sizeof(of);
    if (getsockopt(dd, SOL_HCI, HCI_FILTER, &of, &olen) < 0) {
        printf("Could not get socket options\n");
        return -1;
    }

    hci_filter_clear(&nf);
    hci_filter_set_ptype(HCI_EVENT_PKT, &nf);
    hci_filter_set_event(EVT_LE_META_EVENT, &nf);

    if (setsockopt(dd, SOL_HCI, HCI_FILTER, &nf, sizeof(nf)) < 0) {
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

        while ((len = read(dd, buf, sizeof(buf))) < 0) {
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
                    //return;
                }

                connection->addr = strdup(addr);

                LIST_INSERT_HEAD(&g_ble_macs, connection, entries);
            }
        }
        if (time(NULL) > start + 1) {
            // finished scan
            break;
        }
    }

    done:
    setsockopt(dd, SOL_HCI, HCI_FILTER, &of, sizeof(of));

    if (len < 0)
        return -1;

    return 0;
}

static const char *lescan_help =
        "Usage:\n"
                "\tlescan [--privacy] enable privacy\n"
                "\tlescan [--passive] set scan type passive (default active)\n"
                "\tlescan [--whitelist] scan for address in the whitelist only\n"
                "\tlescan [--discovery=g|l] enable general or limited discovery"
                "procedure\n"
                "\tlescan [--duplicates] don't filter duplicates\n";

static void helper_arg(int min_num_arg, int max_num_arg, int *argc,
                       char ***argv, const char *usage)
{
    *argc -= optind;
    /* too many arguments, but when "max_num_arg < min_num_arg" then no
         limiting (prefer "max_num_arg=-1" to gen infinity)
    */
    if ( (*argc > max_num_arg) && (max_num_arg >= min_num_arg ) ) {
        fprintf(stderr, "%s: too many arguments (maximal: %i)\n",
                *argv[0], max_num_arg);
        printf("%s", usage);
        exit(1);
    }

    /* print usage */
    if (*argc < min_num_arg) {
        fprintf(stderr, "%s: too few arguments (minimal: %i)\n",
                *argv[0], min_num_arg);
        printf("%s", usage);
        exit(0);
    }

    *argv += optind;
}

static void cmd_lescan(int dev_id, int argc, char **argv)
{
    int err, opt, dd;
    uint8_t own_type = LE_PUBLIC_ADDRESS;
    uint8_t scan_type = 0x01;
    uint8_t filter_type = 0;
    uint8_t filter_policy = 0x00;
    uint16_t interval = htobs(0x0010);
    uint16_t window = htobs(0x0010);
    uint8_t filter_dup = 0x01;

    for_each_opt(opt, lescan_options, NULL) {
        switch (opt) {
            case 's':
                own_type = LE_RANDOM_ADDRESS;
                break;
            case 'p':
                own_type = LE_RANDOM_ADDRESS;
                break;
            case 'P':
                scan_type = 0x00;
                break;
            case 'w':
                filter_policy = 0x01;
                break;
            case 'd':
                filter_type = optarg[0];
                if (filter_type != 'g' && filter_type != 'l') {
                    fprintf(stderr, "Unknown discovery procedure\n");
                    exit(1);
                }

                interval = htobs(0x0012);
                window = htobs(0x0012);
                break;
            case 'D':
                filter_dup = 0x00;
                break;
            default:
                printf("%s", lescan_help);
                return;
        }
    }
    helper_arg(0, 1, &argc, &argv, lescan_help);

    if (dev_id < 0)
        dev_id = hci_get_route(NULL);

    dd = hci_open_dev(dev_id);
    if (dd < 0) {
        perror("Could not open device");
        exit(1);
    }

    err = hci_le_set_scan_parameters(dd, scan_type, interval, window,
                                     own_type, filter_policy, 10000);
    if (err < 0) {
        perror("Set scan parameters failed");
        exit(1);
    }

    err = hci_le_set_scan_enable(dd, 0x01, filter_dup, 10000);
    if (err < 0) {
        perror("Enable scan failed");
        exit(1);
    }

    printf("LE Scan ...\n");

    err = print_advertising_devices(dd, filter_type);
    if (err < 0) {
        perror("Could not receive advertising events");
        exit(1);
    }

    err = hci_le_set_scan_enable(dd, 0x00, filter_dup, 10000);
    if (err < 0) {
        perror("Disable scan failed");
        exit(1);
    }

    hci_close_dev(dd);
}

struct lescan_arg_struct {
    int dev_id;
    int argc;
    char **argv;
} lescan_arg_struct_t;

static void* call_cmd_lescan(void *arg_struct) {
    LIST_INIT(&g_ble_macs);

    struct lescan_arg_struct *arg = (struct lescan_arg_struct *)arg_struct;
    cmd_lescan(arg->dev_id, arg->argc, arg->argv);
    puts("Scan completed");
    return NULL;
}


int main(int argc, char *argv[]) {
    /*
    GOptionContext *context;
    GOptionGroup *gatt_group, *params_group, *char_rw_group;
    */
    GError *gerr = NULL;
    GIOChannel *chan;

    /* scan for 10 seconds */
    pthread_t le_scan_thread;
    struct lescan_arg_struct lescan_arg;
    lescan_arg.dev_id = -1;
    lescan_arg.argc = 1;
    char* fake_argv = "./gatttool537";
    lescan_arg.argv = &fake_argv;
    pthread_create(&le_scan_thread, NULL, call_cmd_lescan, &lescan_arg);
    pthread_join(le_scan_thread, NULL);

    const char *default_mac = "00:1A:7D:DA:71:11";
    bool not_in_list = true;
    struct ble_mac_t *np;
    /*
    for (np = g_ble_macs.lh_first; np != NULL; np = np->entries.le_next) {
        if (strcmp(np->addr, default_mac) == 0) {
            not_in_list = false;
            break;
        }
    }
    */
    for (np = g_ble_macs.lh_first; np != NULL; np = np->entries.le_next) {
        if (strlen(np->addr) == 17) {
            opt_dst = g_strdup(np->addr);
            not_in_list = false;
            break;
        }
    }
    if (not_in_list) {
        g_print("Remote Bluetooth address not found during scanning\n");
        got_error = TRUE;
        goto done;
    }
    g_print("Remote Bluetooth address found during scanning\n");

    //opt_dst = g_strdup(default_mac);
    opt_dst_type = g_strdup("public");
    opt_sec_level = g_strdup("low");

    /*
    context = g_option_context_new(NULL);
    /* g_option_context_add_main_entries(context, options, NULL); */

    /* GATT commands
    gatt_group = g_option_group_new("gatt", "GATT commands",
                                    "Show all GATT commands", NULL, NULL);
    g_option_context_add_group(context, gatt_group);
    g_option_group_add_entries(gatt_group, gatt_options);
    */
    /* Primary Services and Characteristics arguments
    params_group = g_option_group_new("params",
                                      "Primary Services/Characteristics arguments",
                                      "Show all Primary Services/Characteristics arguments",
                                      NULL, NULL);
    g_option_context_add_group(context, params_group);
    g_option_group_add_entries(params_group, primary_char_options);
    */
    /* Characteristics value/descriptor read/write arguments
    char_rw_group = g_option_group_new("char-read-write",
                                       "Characteristics Value/Descriptor Read/Write arguments",
                                       "Show all Characteristics Value/Descriptor Read/Write "
                                               "arguments",
                                       NULL, NULL);
    g_option_context_add_group(context, char_rw_group);
    g_option_group_add_entries(char_rw_group, char_rw_options);
    */

    /*
    if (!g_option_context_parse(context, &argc, &argv, &gerr)) {
        g_printerr("%s\n", gerr->message);
        g_clear_error(&gerr);
    }
    */
    opt_interactive = TRUE;
    if (opt_interactive) {
        interactive(opt_src, opt_dst, opt_dst_type, opt_psm);
        goto done;
    }
    /**/
     /*
    if (opt_primary)
        operation = primary;
    else if (opt_characteristics)
        operation = characteristics;
    else if (opt_char_read)
        operation = characteristics_read;
    else if (opt_char_write)
        operation = characteristics_write;
    else if (opt_char_write_req)
        operation = characteristics_write_req;
    else if (opt_char_desc)
        operation = characteristics_desc;*/
    /*else {
        char *help = g_option_context_get_help(context, TRUE, NULL);
        g_print("%s\n", help);
        g_free(help);
        got_error = TRUE;
        goto done;
    }*/
    /*
    if (opt_dst == NULL) {
        g_print("Remote Bluetooth address required\n");
        got_error = TRUE;
        goto done;
    }
    */
    operation = characteristics;
    chan = gatt_connect(opt_src, opt_dst, opt_dst_type, opt_sec_level,
                        opt_psm, opt_mtu, connect_cb, &gerr);
    if (chan == NULL) {
        g_printerr("%s\n", gerr->message);
        g_clear_error(&gerr);
        got_error = TRUE;
        goto done;
    }

    event_loop = g_main_loop_new(NULL, FALSE);

    g_main_loop_run(event_loop);

    g_main_loop_unref(event_loop);

    done:
    //g_option_context_free(context);
    g_free(opt_src);
    g_free(opt_dst);
    g_free(opt_uuid);
    g_free(opt_sec_level);

    if (got_error)
        exit(EXIT_FAILURE);
    else
        exit(EXIT_SUCCESS);
}

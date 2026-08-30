#ifndef BTSTACK_CONFIG_H
#define BTSTACK_CONFIG_H

/* ===== BLE ONLY ===== */
#define ENABLE_LE_ONLY 1
#define ENABLE_BLE 1
#define ENABLE_CLASSIC 0

/* Explicitly disable Classic BT key DB */
#define HAVE_BTSTACK_LINK_KEY_DB 0

/* Role */
#define ENABLE_LE_PERIPHERAL 1

/* Debug */
#define ENABLE_PRINTF_HEXDUMP 1

/* Memory / buffers */
#define HAVE_MALLOC 1
#define HCI_ACL_PAYLOAD_SIZE 251

/* CYW43 HCI transport requirements */
#define HCI_OUTGOING_PRE_BUFFER_SIZE 4
#define HCI_ACL_CHUNK_SIZE_ALIGNMENT 4

/* Security / bonding (LE only) */
#define MAX_NR_HCI_CONNECTIONS 1
#define MAX_NR_SM_LOOKUP_ENTRIES 1
#define MAX_NR_LE_DEVICE_DB_ENTRIES 1
#define NVM_NUM_DEVICE_DB_ENTRIES 1

/* GATT limits */
#define MAX_NR_ATT_SERVICES 1
#define MAX_NR_ATT_CHARACTERISTICS 4
#define NVM_NUM_LINK_KEYS 0

#endif

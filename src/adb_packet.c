#include "adb_packet.h"

#include "adb_conn_priv.h"

adb_error_t adb__packet_send(
        adb_conn_t *conn,
        adb__packet_t *pkt)
{
    const uint8_t *payload_data = NULL;
    uint32_t sum = 0;
    adb_error_t res = ADB_ERR_OK;
    if(!conn || !pkt)
        return ADB_ERR_PARAM;

    payload_data = pkt->payload;
    for(uint32_t i = 0; i < pkt->payload_size; i++)
        sum += payload_data[i];

    pkt->data_check = sum;
    pkt->magic = pkt->command ^ 0xffffffff;

    res = adb__conn_write(conn, pkt, 
            sizeof(*pkt) - sizeof(void*));
    if(res != ADB_ERR_OK)
        return res;

    if(pkt->payload_size == 0)
        return ADB_ERR_OK;

    res = adb__conn_write(conn, pkt->payload, 
            pkt->payload_size);
    if(res != ADB_ERR_OK)
        return res;

    return ADB_ERR_OK;
}


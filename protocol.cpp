#include "protocol.h"

PDU *mkPDU(uint uiMsglen)
{
    uint uiPDUlen = sizeof(PDU)+uiMsglen;
    PDU *pdu = (PDU*)malloc(uiPDUlen);
    if(NULL == pdu)
    {
        exit(EXIT_FAILURE);
    }
    memset(pdu,0,uiPDUlen);
    pdu->uiPDUlen = uiPDUlen;
    pdu->uiMsglen = uiMsglen;
    return pdu;
}

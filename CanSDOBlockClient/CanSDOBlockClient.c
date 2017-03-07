#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "CanSDOBlockClient.h"

FILE  *NewFirmwareFile = NULL;
const char  *NewFirmwareFileName = NULL;
uint8_t u8NodeId = 0;
int iClientTimeOutTimer = 0;
CommStatus_t eCommStatus = comclst_ClientInit;
uint16_t u16LastIndexProcessed = 0;
uint8_t  u8LastSubindexProcessed = 0;
int iBlockSizeFromServer = 0;
bool boCRCSupportedByServer = false;

int fnInit(void)
{
    u8NodeId = NODE_ID;
    eCommStatus = comclst_ClientInit;
    NewFirmwareFile = NULL;
    if(NewFirmwareFileName != NULL)
    {
        fprintf( stderr, "File name: %s \n", NewFirmwareFileName);
        NewFirmwareFile =  fopen(NewFirmwareFileName, "rb");
    }

    if(NewFirmwareFile != NULL)
        return 0;
    else
        return -1;
    return 0;
}


int fnClientTimer(int iInterval)
{
    static int iTimeElapsed = 0;
    iTimeElapsed += iInterval;
    //fprintf( stderr, "Client elapsed: %u \n",   iTimeElapsed);
    if (iClientTimeOutTimer > iInterval) iClientTimeOutTimer -= iInterval; else iClientTimeOutTimer = 0;
    fnRunStateMachine(NULL, 0);
    return (0);
}


#define RCV_DATA(byte)   (ReceivedCanMsg->can_data[byte])
#define SDO_COMMAND_RCV  (*((SDO_Command_t *)(&(RCV_DATA(SDO_COMMAND_POS)))))
#define SDO_INDEX_RCV    (*((uint16_t *)(&(ReceivedCanMsg->can_data[SDO_INDEX_POS]))))
#define SDO_SUBINDEX_RCV (*((uint8_t *) (&(ReceivedCanMsg->can_data[SDO_SUBINDEX_POS]))))
#define SDO_BLKSIZE_RCV  (*((uint8_t *) (&(ReceivedCanMsg->can_data[SDO_BLKSIZE_POS]))))

#define SDO_COMMAND_SND  (*((SDO_Command_t *)(&(SendCanMsg.can_data[SDO_COMMAND_POS]))))
#define SDO_EXP_DATA_SND (*((uint32_t *)(&(SendCanMsg.can_data[SDO_EXP_DATA_POS]))))
#define SDO_INDEX_SND    (*((uint16_t *)(&(SendCanMsg.can_data[SDO_INDEX_POS]))))
#define SDO_SUBINDEX_SND (*((uint8_t *) (&(SendCanMsg.can_data[SDO_SUBINDEX_POS]))))
#define SDO_BLOCK_DATA_SND_P ((char *) (&(SendCanMsg.can_data[SDO_BLOCK_DATA_POS])))
#define CLEAR_MSG_SND    (memset(&SendCanMsg, 0, sizeof(XMC_LMOCan_t)))


int fnProcessCANMessage(XMC_LMOCan_t *ReceivedCanMsg)
{
    fprintf( stderr, "ID: 0x%.X \n", ReceivedCanMsg->can_identifier);
    fprintf( stderr, "len: %u \n",    ReceivedCanMsg->can_data_length);
    for (int i = 0; i < ReceivedCanMsg->can_data_length; ++i)
    {
      fprintf( stderr, "%.2X " , ReceivedCanMsg->can_data[i]);
    }
    fprintf( stderr, "\n\n");

    //Check ID
    if(ReceivedCanMsg->can_identifier != TSDO_Id + u8NodeId)
    {
        return -1;
    }


    fnRunStateMachine(ReceivedCanMsg, 0);

    return(0);
}




int fnRunStateMachine(XMC_LMOCan_t *ReceivedCanMsg, int iCommand)
{
    //check if FileIsOpened
    if(NewFirmwareFile == NULL)
    {
       //fprintf( stderr, "Firmare File Not Opened\n");
       eCommStatus = comclst_ClientInit;
       return -1;
    }

    XMC_LMOCan_t SendCanMsg;
    iCommand = iCommand;
    //communication state machine
    switch (eCommStatus)
    {
        case comclst_ClientInit:

        break;

        case comclst_InitiateBlockDownload:
            if(ReceivedCanMsg == NULL)
            {
                //determine file size
                uint32_t u32BytesToSend;
                rewind(NewFirmwareFile);
                fseek(NewFirmwareFile, 0L, SEEK_END);
                u32BytesToSend = ftell(NewFirmwareFile);
                rewind(NewFirmwareFile);


                //send request to download.
                CLEAR_MSG_SND;
                SDO_COMMAND_SND.InitBlockDownloadReq.ccs = SDO_CCS_INIT_BLOCK_DOWN_REQ;
                SDO_COMMAND_SND.InitBlockDownloadReq.x   = SDO_RESERVED;
                SDO_COMMAND_SND.InitBlockDownloadReq.cc  = SDO_CRC_SUPPORT;
                SDO_COMMAND_SND.InitBlockDownloadReq.s   = SDO_DATA_SIZE_INDICATED;
                SDO_COMMAND_SND.InitBlockDownloadReq.cs  = SDO_CLIENT_SUBCOMMAND_INIT;
                SDO_INDEX_SND     = u16LastIndexProcessed   = OD_DOWNLOAD_PROGRAM_DATA;
                SDO_SUBINDEX_SND  = u8LastSubindexProcessed =OD_PROGRAM_DATA_MAIN;
                SDO_EXP_DATA_SND  = u32BytesToSend;
                SendCanMsg.can_identifier = RSDO_Id + u8NodeId;
                SendCanMsg.can_data_length = 8;
                fnSendCanMessage(&SendCanMsg);

                //Wait for response
                iClientTimeOutTimer = KN_CLIENT_TIMEOUT_MS;
                eCommStatus = comclst_WaitServerInitiateResponse;
             }

        break;

        case comclst_WaitServerInitiateResponse:
             //Check Timeour
            if (!iClientTimeOutTimer)
            {
              //send abort if timeout
                CLEAR_MSG_SND;
                SDO_COMMAND_SND.Abort.cs   = SDO_CS_ABORT;
                SDO_COMMAND_SND.Abort.x    = SDO_RESERVED;
                SDO_INDEX_SND              = u16LastIndexProcessed;
                SDO_SUBINDEX_SND           = u8LastSubindexProcessed;
                SDO_EXP_DATA_SND           = SDO_ABORT_PROTOCOL_TIMED_OUT;
                SendCanMsg.can_identifier  = RSDO_Id + u8NodeId;
                SendCanMsg.can_data_length = 8;
                fnSendCanMessage(&SendCanMsg);

                eCommStatus = comclst_ClientInit;
                break;
            }

            if(ReceivedCanMsg != NULL)
            {
                if(
                    (SDO_COMMAND_RCV.InitBlockDownloadResp.scs == SDO_SCS_INIT_BLOCK_DOWN_RESP)&&
                    (SDO_COMMAND_RCV.InitBlockDownloadResp.ss  == SDO_SERVER_SUBCOMMAND_INIT)
                  )
                {
                    //check correct OD Entry
                    if (
                         (SDO_INDEX_RCV     == u16LastIndexProcessed) &&
                         (SDO_SUBINDEX_RCV  == u8LastSubindexProcessed)
                       )
                    {
                       //uint32_t u32TestData = 0;
                       //int i = 0;
                       boCRCSupportedByServer = SDO_COMMAND_RCV.InitBlockDownloadResp.sc;
                       iBlockSizeFromServer   = SDO_BLKSIZE_RCV;
                       //go to the beginning of the firmware file
                       rewind(NewFirmwareFile);

                       //send data
                       for (int i = 0; i < iBlockSizeFromServer; ++i)
                       {

                         CLEAR_MSG_SND;
                         SDO_COMMAND_SND.DownloadBlock.c = SDO_NOT_LAST_BLOCK;
                         SDO_COMMAND_SND.DownloadBlock.seqno = i+1;

                         //u32TestData += 5;
                         int iBytesReadFromFile = fread(SDO_BLOCK_DATA_SND_P, 1, SDO_BLOCK_DATA_SIZE, NewFirmwareFile);
                         if(iBytesReadFromFile < SDO_BLOCK_DATA_SIZE)
                         {
                             //TODO this is the end of transfer - should be determined before to correctly set LAST_BLOCK_ATTRIBUTE
                         }
                         SendCanMsg.can_identifier  = RSDO_Id + u8NodeId;
                         SendCanMsg.can_data_length = 8;
                         fnSendCanMessage(&SendCanMsg);

                         iClientTimeOutTimer = KN_CLIENT_TIMEOUT_MS;

                       }

                    }
                    else
                    {

                    }

                }
                else
                {

                }
            }
            else
            {

            }



        break;

        default:

        break;
    }


    return(0);
}


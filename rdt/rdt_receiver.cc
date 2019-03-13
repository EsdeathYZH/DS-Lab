/*
 * FILE: rdt_receiver.cc
 * DESCRIPTION: Reliable data transfer receiver.
 * NOTE: This implementation assumes there is no packet loss, corruption, or 
 *       reordering.  You will need to enhance it to deal with all these 
 *       situations.  In this implementation, the packet format is laid out as 
 *       the following:
 *       
 *       |<-  1 byte  ->|<-             the rest            ->|
 *       | payload size |<-             payload             ->|
 *
 *       The first byte of each packet indicates the size of the payload
 *       (excluding this single-byte header)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <list>
#include "rdt_struct.h"
#include "rdt_receiver.h"
#include "utils.h"

static struct message * recv_buffer[SEQ_SIZE];
static bool             flag_buffer[SEQ_SIZE];
static std::list<struct message*> message_factory;

static seq_nr_t next_frame_expected = 0;

static void acknowledge(seq_nr_t seq_num){
    packet pkt;
    pkt.data[2] = 0;
    pkt.data[3] = seq_num;
    /* calculate checksum */
    *((u_int16_t*)pkt.data) = chksum(pkt.data + 2, 2);
    Receiver_ToLowerLayer(&pkt);
}

static void Receiver_SubmitMsg(struct message* message, bool end_flag){
    ASSERT(message!=NULL);
    if(end_flag){
        int size = 0;
        /* calculate total size */
        for(auto& item:message_factory){
            size += item->size;
        }
        size += message->size;

        /* Construct a message */
        struct message *msg = (struct message*) malloc(sizeof(struct message));
        ASSERT(msg!=NULL);
        msg->size = size;
        msg->data = (char*)malloc(size);
        ASSERT(msg->data!=NULL);
        
        /* Copy all data in order */
        int cursor = 0;
        for(auto& item:message_factory){
            memcpy(msg->data + cursor, item->data, item->size);
            cursor += item->size; 
            free(item->data);
            free(item);
        }
        memcpy(msg->data + cursor, message->data, message->size);
        cursor += message->size;
        ASSERT(cursor == size);

        Receiver_ToUpperLayer(msg);
        free(msg->data);
        free(msg);
        message_factory.clear();
    }
    else{
        message_factory.push_back(message);
    }
}

/* receiver initialization, called once at the very beginning */
void Receiver_Init()
{
    fprintf(stdout, "At %.2fs: receiver initializing ...\n", GetSimulationTime());
    for(int i = 0; i < SEQ_SIZE; i++){
        recv_buffer[i] = NULL;
        flag_buffer[i] = false;
    }
}

/* receiver finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to use this opportunity to release some 
   memory you allocated in Receiver_init(). */
void Receiver_Final()
{
    fprintf(stdout, "At %.2fs: receiver finalizing ...\n", GetSimulationTime());
}

/* event handler, called when a packet is passed from the lower layer at the 
   receiver */
void Receiver_FromLowerLayer(struct packet *pkt)
{
    /* 1-byte header indicating the size of the payload */
    /* 1-byte header indicating the sequence number */
    /* 2-byte header indicating the checksum */
    int header_size = 4;
    ASSERT(pkt);
    seq_nr_t seq_num = pkt->data[3] & 127;
    ASSERT(seq_num < 128);
    bool end_flag = ((pkt->data[3] & 128) != 0);

    fprintf(stdout, "At %.2fs: a packet received(%d),expected(%d), flag:(%d)!\n", GetSimulationTime(), seq_num, next_frame_expected, end_flag);
    /* sanity check in case the packet is corrupted */
    if(*((u_int16_t*)pkt->data) != chksum(pkt->data + 2, pkt->data[2] + 2)){
        fprintf(stdout, "At %.2fs: a packet corrupted!\n", GetSimulationTime());
        return;
    }

    /* if a previous ack is lost or corrupted*/
    // if(between((next_frame_expected + SEQ_SIZE - WINDOW_SIZE) % SEQ_SIZE, seq_num, next_frame_expected)){
    //     //acknowledge(pkt->data[3]);
    //     acknowledge(next_frame_expected - 1);
    //     return;
    // }
    if(!between(next_frame_expected, seq_num, (next_frame_expected+WINDOW_SIZE)%SEQ_SIZE)){
        //acknowledge(pkt->data[3]);
        acknowledge((next_frame_expected + SEQ_SIZE - 1) % SEQ_SIZE);
        return;
    }

    /* construct a message and deliver to the upper layer */
    struct message *msg = (struct message*) malloc(sizeof(struct message));
    ASSERT(msg!=NULL);
    msg->size = pkt->data[2];
    ASSERT(msg->size > 0);
    msg->data = (char*) malloc(msg->size);
    ASSERT(msg->data!=NULL);
    memcpy(msg->data, pkt->data+header_size, msg->size);

    if(seq_num == next_frame_expected){
        //Receiver_ToUpperLayer(msg);
        Receiver_SubmitMsg(msg, end_flag);
        incNum(next_frame_expected, SEQ_SIZE);
        int counter = 0;
        while(recv_buffer[next_frame_expected] != NULL && counter < 9){
            //Receiver_ToUpperLayer(recv_buffer[next_frame_expected]);
            Receiver_SubmitMsg(recv_buffer[next_frame_expected], flag_buffer[next_frame_expected]);
            recv_buffer[next_frame_expected] = NULL;
            flag_buffer[next_frame_expected] = 0;
            incNum(next_frame_expected, SEQ_SIZE);
            counter++;
        }
        acknowledge((next_frame_expected + SEQ_SIZE - 1) % SEQ_SIZE);
    }
    else{
        if(recv_buffer[seq_num] == NULL){
            recv_buffer[seq_num] = msg;
            flag_buffer[seq_num] = end_flag;
        }else{
            /* don't forget to free the space */
            free(msg->data);
            free(msg);
        }
    }
}

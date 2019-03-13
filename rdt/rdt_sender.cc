/*
 * FILE: rdt_sender.cc
 * DESCRIPTION: Reliable data transfer sender.
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
#include "rdt_sender.h"
#include "utils.h"

struct SenderTimer{
    packet* pkt;
    double expire_time;
    bool acked;
};

packet sliding_window[WINDOW_SIZE];
std::list<packet>wait_buffer;
std::list<SenderTimer>timer_chain;

//seq_nr_t next_frame_to_send = 0;
seq_nr_t next_ack_expected = 0;
seq_nr_t next_seq_num = 0;
seq_nr_t nbuffered = 0;

static seq_nr_t GetSeqNum(packet* pkt){ ASSERT(pkt); return (seq_nr_t)(pkt->data[3] & 127); }

static void Sender_AddTimer(packet* pkt, double expire_time){
    printf("At %.2fs: Add timer(%d)\n", GetSimulationTime(), GetSeqNum(pkt));
    SenderTimer timer;
    timer.acked = false;
    timer.expire_time = expire_time;
    timer.pkt = pkt;
    timer_chain.push_back(timer);
    if(timer_chain.size() == 1 && !Sender_isTimerSet()){
        Sender_StartTimer(TIME_OUT);
    }
}

static void Sender_RemoveTimer(seq_nr_t seq_num){
    printf("At %.2fs: Remove timer(%d)\n", GetSimulationTime(), seq_num);
    ASSERT(timer_chain.size()!=0);
    ASSERT(Sender_isTimerSet());
    if(GetSeqNum(timer_chain.front().pkt) == seq_num){
        Sender_StopTimer();
        timer_chain.pop_front();
        while(timer_chain.size()!=0){
            if(timer_chain.front().acked){
                timer_chain.pop_front();
            }else{
                printf("At %.2fs: Start timer(%d),rest time(%.2fs)\n", GetSimulationTime(),
                 GetSeqNum(timer_chain.front().pkt), timer_chain.front().expire_time - GetSimulationTime());
                Sender_StartTimer(timer_chain.front().expire_time - GetSimulationTime());
                break;
            }
        }
    }else{
        for(auto& iter:timer_chain){
            if(GetSeqNum(iter.pkt) == seq_num) iter.acked = true;
        }
    }
}

/* sender initialization, called once at the very beginning */
void Sender_Init()
{
    fprintf(stdout, "At %.2fs: sender initializing ...\n", GetSimulationTime());
}

/* sender finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to take this opportunity to release some 
   memory you allocated in Sender_init(). */
void Sender_Final()
{
    fprintf(stdout, "At %.2fs: sender finalizing ...\n", GetSimulationTime());
}

/* event handler, called when a message is passed from the upper layer at the 
   sender */
void Sender_FromUpperLayer(struct message *msg)
{
    /* 1-byte header indicating the size of the payload */
    /* 1-byte header indicating the sequence number */
    /* 2-byte header indicating the checksum */
    int header_size = 4;

    /* maximum payload size */
    int maxpayload_size = RDT_PKTSIZE - header_size;

    /* split the message if it is too big */

    /* reuse the same packet data structure */
    packet pkt;

    /* the cursor always points to the first unsent byte in the message */
    int cursor = 0;
    ASSERT(msg);
    while (cursor < msg->size) {
        /* fill in the packet */
        int payload_size = (maxpayload_size < (msg->size - cursor)) ? maxpayload_size : (msg->size - cursor);

        pkt.data[2] = payload_size;
        pkt.data[3] = next_seq_num;

        /* If it reaches the end of a message, set the first bit*/
        if(payload_size == (msg->size - cursor)){
            pkt.data[3] |= (1 << 7);
        }

        incNum(next_seq_num, SEQ_SIZE);
        memcpy(pkt.data+header_size, msg->data+cursor, payload_size);
        /* calculate checksum */
        *((u_int16_t*)pkt.data) = chksum(pkt.data + 2, payload_size + 2);

        /* If there are blank slots, send the packet */
        if(nbuffered < WINDOW_SIZE && wait_buffer.size() == 0){
            /* send it out through the lower layer */
            int next_send = (next_ack_expected + nbuffered) % WINDOW_SIZE;
            sliding_window[next_send] = pkt;
            Sender_ToLowerLayer(&sliding_window[next_send]);
            fprintf(stdout, "At %.2fs: send packet(%d)!\n", GetSimulationTime(), GetSeqNum(&pkt));
            Sender_AddTimer(&sliding_window[next_send], GetSimulationTime() + TIME_OUT);
            nbuffered++;
        }else{
            /* store the packet in wait buffer */
            printf("At %.2fs: Enter into wait buffer(%d)\n", GetSimulationTime(), GetSeqNum(&pkt));
            wait_buffer.emplace_back(pkt);
        }
        /* move the cursor */
        cursor += payload_size;
    }
}

/* event handler, called when a packet is passed from the lower layer at the 
   sender */
void Sender_FromLowerLayer(struct packet *pkt)
{
    ASSERT(pkt);
    fprintf(stdout, "At %.2fs: a ack(%d) received,expected(%d),nbuffer(%d), waitbuffer(%d)!\n", GetSimulationTime(), 
            GetSeqNum(pkt), GetSeqNum(&sliding_window[next_ack_expected]), nbuffered, (int)wait_buffer.size());
    /* sanity check in case the packet is corrupted */
    if(*((u_int16_t*)pkt->data) != chksum(pkt->data + 2, pkt->data[2] + 2)){
        fprintf(stdout, "At %.2fs: a packet corrupted!\n", GetSimulationTime());
        return;
    }

    seq_nr_t ack = GetSeqNum(pkt);
    while(nbuffered > 0 && between(GetSeqNum(&sliding_window[next_ack_expected]), ack,
             (GetSeqNum(&sliding_window[(next_ack_expected+nbuffered+WINDOW_SIZE-1)%WINDOW_SIZE])+1)%SEQ_SIZE)){
        nbuffered--;
        Sender_RemoveTimer(GetSeqNum(&sliding_window[next_ack_expected]));
        incNum(next_ack_expected, WINDOW_SIZE);
    }
    
    while(nbuffered < WINDOW_SIZE && wait_buffer.size() != 0){
        int next_send = (next_ack_expected + nbuffered) % WINDOW_SIZE;
        sliding_window[next_send] = wait_buffer.front();
        wait_buffer.pop_front();
        printf("Drain a packet from wait buffer(%d)\n", GetSeqNum(&sliding_window[next_send]));
        Sender_ToLowerLayer(&sliding_window[next_send]);
        fprintf(stdout, "At %.2fs: send packet(%d)!\n", GetSimulationTime(), GetSeqNum(&sliding_window[next_send]));
        Sender_AddTimer(&sliding_window[next_send], GetSimulationTime() + TIME_OUT);
        nbuffered++;
    }
}

/* event handler, called when the timer expires */
void Sender_Timeout()
{
    fprintf(stdout, "At %.2fs: a timeout occurs!\n", GetSimulationTime());
    packet* pkt = timer_chain.front().pkt;
    timer_chain.pop_front();
    fprintf(stdout, "At %.2fs: resend packet(%d)!\n", GetSimulationTime(), GetSeqNum(pkt));
    Sender_ToLowerLayer(pkt);
    Sender_AddTimer(pkt, GetSimulationTime() + TIME_OUT);
    while(timer_chain.size()!=0){
        if(timer_chain.front().acked){
            timer_chain.pop_front();
        }else{
            printf("At %.2fs: Start timer(%d),rest time(%.2fs)\n", GetSimulationTime(),
                GetSeqNum(timer_chain.front().pkt), timer_chain.front().expire_time - GetSimulationTime());
            double new_timeout = timer_chain.front().expire_time - GetSimulationTime();
            if(new_timeout < 0){
                pkt = timer_chain.front().pkt;
                timer_chain.pop_front();
                fprintf(stdout, "At %.2fs: resend packet(%d)!\n", GetSimulationTime(), GetSeqNum(pkt));
                Sender_ToLowerLayer(pkt);
                Sender_AddTimer(pkt, GetSimulationTime() + TIME_OUT);
            }else{
                Sender_StartTimer(new_timeout);
                break;
            }
        }
    }
}

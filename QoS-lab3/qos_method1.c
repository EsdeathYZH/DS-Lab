#include "rte_common.h"
#include "rte_mbuf.h"
#include "rte_meter.h"
#include "rte_red.h"

#include "qos.h"

struct rte_meter_srtcm app_flows[APP_FLOWS_MAX];
struct rte_red         app_queue[APP_FLOWS_MAX];

struct rte_meter_srtcm_params app_srtcm_params[] = {
	{.cir = 1000000000000 * 0.16,  .cbs = 80000, .ebs = 80000},
	{.cir = 1000000000000 * 0.08,  .cbs = 40000, .ebs = 40000},
	{.cir = 1000000000000 * 0.04,  .cbs = 20000, .ebs = 20000},
	{.cir = 1000000000000 * 0.02,  .cbs = 10000, .ebs = 10000},
};

uint64_t queue_size[APP_FLOWS_MAX] = {0};

struct rte_red_config  red_params[APP_FLOWS_MAX] = {
	/* Colors Green / Yellow / Red */
	[0] = {.min_th = 1022 << 19, .max_th = 1023 << 19, .maxp_inv = 10, .wq_log2 = 9},
	[1] = {.min_th = 1022 << 19, .max_th = 1023 << 19, .maxp_inv = 10, .wq_log2 = 9},
	[2] = {.min_th = 0, .max_th = 1, .maxp_inv = 10, .wq_log2 = 9},
};

/**
 * srTCM
 */
int
qos_meter_init(void)
{
	int ret;
    for (int i = 0, j = 0; i < APP_FLOWS_MAX; i++, j = (j + 1) % RTE_DIM(app_srtcm_params)) {
		ret = rte_meter_srtcm_config(&app_flows[i], &app_srtcm_params[j]);
		if (ret) return ret;
	}

    return 0;
}

enum qos_color
qos_meter_run(uint32_t flow_id, uint32_t pkt_len, uint64_t time)
{
    return rte_meter_srtcm_color_blind_check(&app_flows[flow_id], time, pkt_len);
}


/**
 * WRED
 */

int
qos_dropper_init(void)
{
	int ret;
    for(int i = 0; i < APP_FLOWS_MAX; i++){
		ret = rte_red_rt_data_init(&app_queue[i]);
		if (ret) return ret;
	}
	return 0;
}

int
qos_dropper_run(uint32_t flow_id, enum qos_color color, uint64_t time)
{
	if(time != app_queue[flow_id].q_time){
		//printf("queue %d avg size:%d\n", flow_id, app_queue[flow_id].avg);
		rte_red_mark_queue_empty(&app_queue[flow_id], time);
		queue_size[flow_id] = 0;
	}
    int result = rte_red_enqueue(&red_params[color], &app_queue[flow_id], queue_size[flow_id], time);

	if(result){
		return 1;
	}else{
		queue_size[flow_id]++;
		return 0;
	}
}

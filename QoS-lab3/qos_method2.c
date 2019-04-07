#include "rte_common.h"
#include "rte_mbuf.h"
#include "rte_meter.h"
#include "rte_red.h"

#include "qos.h"

struct rte_meter_srtcm app_flows[APP_FLOWS_MAX];
struct rte_red         app_queue[APP_FLOWS_MAX];

struct rte_meter_srtcm_params app_srtcm_params[] = {
	{.cir = 1000000000000 * 0.16,  .cbs = 60000, .ebs = 50000},
};

uint64_t queue_size[APP_FLOWS_MAX] = {0, 0, 0, 0};

struct rte_red_config  red_params[APP_FLOWS_MAX][3] = {
	/* Traffic Class 0 Colors Green / Yellow / Red */
	[0][0] = {.min_th = 480*2000, .max_th = 640*2000, .maxp_inv = 10, .wq_log2 = 1},
	[0][1] = {.min_th = 400*2000, .max_th = 640*2000, .maxp_inv = 10, .wq_log2 = 1},
	[0][2] = {.min_th = 320*2000, .max_th = 640*2000, .maxp_inv = 10, .wq_log2 = 1},

	/* Traffic Class 1 - Colors Green / Yellow / Red */
	[1][0] = {.min_th = 480*1200, .max_th = 640*1200, .maxp_inv = 10, .wq_log2 = 2},
	[1][1] = {.min_th = 400*1200, .max_th = 640*1200, .maxp_inv = 10, .wq_log2 = 2},
	[1][2] = {.min_th = 320*1200, .max_th = 640*1200, .maxp_inv = 10, .wq_log2 = 2},

	/* Traffic Class 2 - Colors Green / Yellow / Red */
	[2][0] = {.min_th = 480*900, .max_th = 640*900, .maxp_inv = 10, .wq_log2 = 3},
	[2][1] = {.min_th = 400*900, .max_th = 640*900, .maxp_inv = 10, .wq_log2 = 3},
	[2][2] = {.min_th = 320*900, .max_th = 640*900, .maxp_inv = 10, .wq_log2 = 3},

	/* Traffic Class 3 - Colors Green / Yellow / Red */
	[3][0] = {.min_th = 480*600, .max_th = 640*600, .maxp_inv = 10, .wq_log2 = 4},
	[3][1] = {.min_th = 400*600, .max_th = 640*600, .maxp_inv = 10, .wq_log2 = 4},
	[3][2] = {.min_th = 320*600, .max_th = 640*600, .maxp_inv = 10, .wq_log2 = 4}
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
    int result = rte_red_enqueue(&red_params[flow_id][color], &app_queue[flow_id], queue_size[flow_id], time);

	if(result){
		return 1;
	}else{
		queue_size[flow_id]++;
		return 0;
	}
}

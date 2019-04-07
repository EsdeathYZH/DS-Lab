## Lab3 : QoS Implementation with DPDK

### 1. Parameter Deduction

#### Method 1: Use SRTCM to control service, different flows share same RED parameters

+ Use maximum rate(1.28 Gbps) to calculate CIR(1.28 Gbps = 0.16 GB/s), divide CIR into two same size buckets.
+ Divide bandwidth in proportion of 8:4:2:1 to calculate all flows' SRTCM parameters

```c
struct rte_meter_srtcm_params app_srtcm_params[] = {
	{.cir = 1000000000000 * 0.16,  .cbs = 80000, .ebs = 80000},
	{.cir = 1000000000000 * 0.08,  .cbs = 40000, .ebs = 40000},
	{.cir = 1000000000000 * 0.04,  .cbs = 20000, .ebs = 20000},
	{.cir = 1000000000000 * 0.02,  .cbs = 10000, .ebs = 10000},
};
```
+ Use the data of maxp_inv and wq_log2 in DPDK examples
+ We drop all red packets and enqueue all green/yellow packets

```c
struct rte_red_config  red_params[APP_FLOWS_MAX] = {
	/* Colors Green / Yellow / Red */
	[0] = {.min_th = 1022 << 19, .max_th = 1023 << 19, .maxp_inv = 10, .wq_log2 = 9},
	[1] = {.min_th = 1022 << 19, .max_th = 1023 << 19, .maxp_inv = 10, .wq_log2 = 9},
	[2] = {.min_th = 0, .max_th = 1, .maxp_inv = 10, .wq_log2 = 9},
};
```



#### Method 2: Use RED to control service, different flow share same SRTCM parameters

+ Different flows share the same SRTCM parameters, CIR equals to the maximum rate 1.28 Gbps

```c
struct rte_meter_srtcm_params app_srtcm_params[] = {
	{.cir = 1000000000000 * 0.16,  .cbs = 60000, .ebs = 50000},
};
```

+ Assign wq_log2 with 1,2,3,4 to mark the weights of queues
+ Use min/max threshold to control queue as the proportion of 8:4:2:1
+ maxp_inv is still 10

```c
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
```



### 2. DPDK APIs I use

- `rte_meter_srtcm_config()`: Initialize SRTCM data with parameters.
- `rte_red_config_init()`: Initialize RED config with parameters.
- `rte_red_rt_data_init()`: Initialize RED data.
- `rte_meter_srtcm_color_blind_check()`: Mark packets with three kind of colors using blind check algorithm.
- `rte_red_mark_queue_empty()`: Mark the queue clear.
- `rte_red_enqueue()`: Enqueue a packet, the result indicates whether this packet is enqueued/dropped.
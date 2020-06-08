import numpy as np


pl_to_ppl_ratio = 2  # this is there because we set the smoothening time equal to planning for the moment
# since we don't have knobs for smoothening (so we haven't included it as a stage)

# latency of the stages we didn't include in the optimizer formulation
run_diagnostics = .15
filtering = .400
pc_latency = run_diagnostics + filtering

pc_to_om_oh = .6
runtime_latency = .2 # this does include point cloud intro (i.e, generating point cloud and running diagnostics)
pc_outro = .1 # filtering and such
misc_latency = pc_outro + runtime_latency + pc_to_om_oh  # time of the stages not included in the controller

# constraints
#pc_res_min = .3
r_min_static = .3
om_to_pl_res_min = r_min_static
r_steps = 4
r_max_static = (2 ** r_steps) * r_min_static
v_min = 3000
#v_max = np.inf
v_max = 2000000
#rt_max = 30



import numpy as np
import sys
sys.path.append('..')
sys.path.append('../../../../../data_processing/common_utils')
#from calc_sampling_time import *
from utils import *
from model_generation.model_gen import *
from model import Model
from data_parser import DataParser
import matplotlib.pyplot as plt

import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D

   

def calculate_static_values():
    filtering_time = .4
    run_diagnostics = .15
    pc_to_om_overhead = .6
    run_time_budget = .1 
    # collect data 
    om_res, om_vol, om_response_time_measured, om_pl_res, om_pl_vol, om_pl_response_time_measured, pp_pl_res, pp_pl_vol, pp_pl_response_time_measured = collect_data()

    # colecting models model 
    om_popt, om_pl_popt, pp_pl_popt, typical_model = roborun_model_gen(om_res, om_vol, om_response_time_measured, om_pl_res, om_pl_vol, om_pl_response_time_measured, pp_pl_res, pp_pl_vol,
            pp_pl_response_time_measured)	 # for error calculation


    om_res_desired = om_pl_res_desired=pp_pl_res_desired = .3
    om_vol_desired = 30000
    om_pl_vol_desired = 2*30000
    pp_pl_vol_desired = 2*30000
    visibility_avg = 12.5 

    om_latency = calculate_fitted_value(om_popt, om_res_desired, om_vol_desired, typical_model)

    om_pl_latency = calculate_fitted_value(om_pl_popt, om_pl_res_desired, om_pl_vol_desired, typical_model)
    pp_pl_latency = calculate_fitted_value(pp_pl_popt, pp_pl_res_desired, pp_pl_vol_desired, typical_model)


    max_time_budget = om_latency + om_pl_latency + 2*pp_pl_latency + pc_to_om_overhead + filtering_time + run_diagnostics + run_time_budget

    velocity_to_budget_on = calc_v_max(max_time_budget, visibility_avg, .1439, .8016)
    print("----om_latency:" + str(om_latency))
    print("--------om_pl_latency:" + str(om_pl_latency))
    print("-------------pp_pl_latency" + str(2*pp_pl_latency))
    print("-----------------controlled_latency" + str(om_latency + om_pl_latency + 2*pp_pl_latency))
    print("----------------------max_budget" + str(max_time_budget))
    print("----------------------v_max" + str(velocity_to_budget_on))

    json_output_file = open("static_knobs.json", "w")  

    json_output_file.write("{\n")
    json_output_file.write("\t\t\"max_time_budget\": "+ str(max_time_budget)+",\n");
    json_output_file.write("\t\t\"om_latency_expected\": " + str(om_latency)+",\n");
    json_output_file.write("\t\t\"om_to_pl_latency_expected\": " + str(om_pl_latency)+",\n");
    json_output_file.write("\t\t\"ppl_latency_expected\": " + str(pp_pl_latency)+",\n");
    json_output_file.write("\t\t\"velocity_to_budget_on\": " + str(velocity_to_budget_on)+",\n");
    json_output_file.write("\t\t\"v_max\": " + str(velocity_to_budget_on)+",\n");
    json_output_file.write("\t\t\"pc_res_max\": " + str(om_res_desired)+",\n");
    json_output_file.write("\t\t\"pc_vol_ideal_max\": " + str(om_vol_desired)+",\n");
    json_output_file.write("\t\t\"om_to_pl_res_max\": " + str(om_pl_res_desired)+",\n");
    json_output_file.write("\t\t\"om_to_pl_vol_ideal_max\": " + str(om_pl_vol_desired)+",\n");
    json_output_file.write("\t\t\"ppl_vol_ideal_max\": " + str(pp_pl_vol_desired)+"\n");
    json_output_file.write("}")
    json_output_file.close()




## May want to convert this to using absolute paths to make life easier ##
if __name__ == '__main__':
    calculate_static_values() 

#include <boost/smart_ptr/shared_ptr.hpp>
#include <common.h>
#include <coord.h>
#include <datacontainer.h>
#include <datat.h>
#include <Drone.h>
#include <geometry_msgs/Point.h>
#include <geometry_msgs/Twist.h>
#include <geometry_msgs/Vector3.h>
#include <mavbench_msgs/control.h>
#include <mavbench_msgs/control_input.h>
#include <mavbench_msgs/control_internal_states.h>
#include <mavbench_msgs/multiDOFpoint.h>
#include <mavbench_msgs/multiDOFtrajectory.h>
#include <mavbench_msgs/planner_info.h>
#include <mavbench_msgs/runtime_debug.h>
#include <package_delivery/point.h>
#include <profile_manager/profiling_data_srv.h>
#include <profile_manager/profiling_data_verbose_srv.h>
#include <profile_manager.h>
#include <ros/duration.h>
#include <ros/init.h>
#include <ros/node_handle.h>
#include <ros/param.h>
#include <ros/publisher.h>
#include <ros/rate.h>
#include <ros/service_server.h>
#include <ros/this_node.h>
#include <ros/time.h>
#include <rosconsole/macros_generated.h>
#include <signal.h>
#include <std_msgs/Header.h>
#include <TimeBudgetter.h>
#include <visualization_msgs/Marker.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>
bool motivation_section, motivation_section_for_velocity;
ros::Time last_time_sent_budgets;
Drone *drone;
string design_mode;
using namespace std;
std::string ip_addr__global;
double g_sensor_max_range, g_sampling_interval, g_v_max, g_planner_drone_radius, g_planner_drone_radius_when_hovering, vmax_max;
bool DEBUG_VIS;
std::deque<double> MacroBudgets;
bool reactive_runtime;
string budgetting_mode;
double max_time_budget, max_time_budget_performance_modeling, perf_iterative_cntr;
bool knob_performance_modeling = false;
bool knob_performance_modeling_for_pc_om = false;
bool knob_performance_modeling_for_om_to_pl = false;
bool knob_performance_modeling_for_piecewise_planner = false;
double ppl_vol_min_coeff;
double planner_drone_radius;
bool DEBUG_RQT;
int g_capture_size = 600; //set this to 1, if you want to see every data collected separately
bool time_budgetting_failed = false;
double pc_res_max, num_of_res_to_try, pc_vol_ideal_max, pc_vol_ideal_min, om_to_pl_vol_ideal_max, ppl_vol_ideal_max ;
double planner_consecutive_failure_ctr = 0;




coord drone_position;
coord last_drone_position;
geometry_msgs::Twist drone_vel;
ProfileManager *profile_manager_client;
mavbench_msgs::control control;

bool got_new_input = false;
ros::Time last_modification_time;
bool last_modification(double time_passed){
	bool result = false;
	if ((ros::Time::now() - last_modification_time).toSec() > time_passed){
		last_modification_time = ros::Time::now(); // update
		result = true;
	}
	return result;
}

typedef struct node_budget_t {
	string node_type;
	double budget;
} NodeBudget;


typedef struct param_val_t {
	string param_name;
	double param_val;
} ParamVal;


typedef struct node_param_t {
	string node_type;
	vector<ParamVal>  param_val_vec;
} NodeParams;

vector<string> node_types;
mavbench_msgs::runtime_debug debug_data;
DataContainer *profiling_container;

vector<NodeBudget> calc_micro_budget(double macro_time_budget){
	vector<NodeBudget> node_budget_vec;
	double node_coeff;
	for (auto &node_type: node_types){

		if (node_type == "point_cloud"){
			node_coeff = .1;
		} else if (node_type == "octomap"){
			node_coeff = .55;
		} else if (node_type == "planning"){
			node_coeff = .35;
		}else{
			ROS_ERROR_STREAM("node:" << node_type << " is not defined for runtime");
			exit(0);
		}
		node_budget_vec.push_back(NodeBudget{node_type, node_coeff*macro_time_budget});
	}

	return node_budget_vec;
}


vector<ParamVal> perf_model(NodeBudget node_budget) {
	vector<ParamVal> results;
	if (node_budget.node_type == "point_cloud"){
		if (node_budget.budget > .2) {
			ROS_ERROR_STREAM("widening point c loud");
			results.push_back(ParamVal{"point_cloud_height", 25});
			results.push_back(ParamVal{"point_cloud_width", 25});
			results.push_back(ParamVal{"pc_res", .3});
		}else{
			//ROS_ERROR_STREAM("shrinking point c loud");
			results.push_back(ParamVal{"point_cloud_height", 5});
			results.push_back(ParamVal{"point_cloud_width", 5});
			results.push_back(ParamVal{"pc_res", .8});
		}
	} else if (node_budget.node_type == "octomap"){
		if (node_budget.budget > .5) {
			results.push_back(ParamVal{"om_to_pl_res", .8});
		}else{
			results.push_back(ParamVal{"om_to_pl_res", .8});
		}
	}else if (node_budget.node_type == "planning"){
		results.push_back(ParamVal{"ppl_time_budget", node_budget.budget/2});
		results.push_back(ParamVal{"smoothening_budget", node_budget.budget/2});
	}else{
		ROS_ERROR_STREAM("node:" << node_budget.node_type << " is not defined for runtime");
		exit(0);
	}
	return results;
}


vector<ParamVal> calc_params(NodeBudget node_budget) {
	return perf_model(node_budget);
}


void set_param(ParamVal param) {
	ros::param::set(param.param_name, param.param_val);
}


/*
void convertMavBenchMultiDOFtoMultiDOF(mavbench_msgs::multiDOFpoint copy_from, multiDOFpoint &copy_to){
    	copy_to.vx = copy_from.vx;
    	copy_to.vy = copy_from.vy;
    	copy_to.vz = copy_from.vz;
    	copy_to.ax = copy_from.ax;
    	copy_to.ay = copy_from.ay;
    	copy_to.az = copy_from.az;
    	copy_to.x = copy_from.x;
    	copy_to.y = copy_from.y;
    	copy_to.z = copy_from.z;
}
*/

void control_callback(const mavbench_msgs::control::ConstPtr& msg){
	// -- note that we need to set the values one by one
	// -- since we don't know the order
	// -- with which the callbacks are called,
	// -- and if we set the control_inputs to msg, we can overwrite
	// -- the nex_steps_callback sensor_to_acuation_time_budget
	control.inputs.sensor_volume_to_digest = msg->inputs.sensor_volume_to_digest;
	control.inputs.gap_statistics_avg = msg->inputs.gap_statistics_avg;
	control.inputs.gap_statistics_min = msg->inputs.gap_statistics_min;
	control.inputs.gap_statistics_max = msg->inputs.gap_statistics_max;
	control.inputs.cur_tree_total_volume = msg->inputs.cur_tree_total_volume;
	control.inputs.obs_dist_statistics_avg = msg->inputs.obs_dist_statistics_avg;
	control.inputs.obs_dist_statistics_min = msg->inputs.obs_dist_statistics_min;
	control.inputs.planner_consecutive_failure_ctr = msg->inputs.planner_consecutive_failure_ctr;
	planner_consecutive_failure_ctr = msg->inputs.planner_consecutive_failure_ctr;
	//ROS_INFO_STREAM("got a new input in control_callback crun");
	control.inputs.max_time_budget = msg->inputs.max_time_budget;
	max_time_budget =  control.inputs.max_time_budget;
	//ros::param::get("/max_time_budget", max_time_budget);
	got_new_input = true;
}


std::deque<multiDOFpoint> traj;
vector<double> accelerationCoeffs = {.1439,.8016};
double maxSensorRange;
double TimeIncr;
double maxVelocity;
geometry_msgs::Point g_goal_pos;
multiDOFpoint closest_unknown_point;
multiDOFpoint closest_unknown_point_upper_bound; // used if closest_uknown is inf, that's there is not unknown
bool goal_known = false;
double cur_vel_mag; // current drone's velocity


double calc_sensor_to_actuation_time_budget_to_enforce_based_on_current_velocity(double velocityMag, double sensor_range){
	if (planner_consecutive_failure_ctr > 1){
		return max_time_budget;
	}

	TimeBudgetter time_budgetter(maxSensorRange, maxVelocity, accelerationCoeffs, TimeIncr, max_time_budget, g_planner_drone_radius, design_mode);

	time_budgetter.set_params(vmax_max,g_planner_drone_radius, g_planner_drone_radius_when_hovering);
	double time_budget = time_budgetter.calcSamplingTimeFixV(velocityMag, 0.0, design_mode, sensor_range);
//	ROS_INFO_STREAM("---- calc budget directly"<< time_budget);
	return time_budget;
}

void closest_unknown_callback(const mavbench_msgs::planner_info::ConstPtr& msg){
	closest_unknown_point.x = msg->x;
	closest_unknown_point.y = msg->y;
	closest_unknown_point.z = msg->z;
	if (isnan(closest_unknown_point.x) ||
			isnan(closest_unknown_point.y) ||
			isnan(closest_unknown_point.z) ){
//		ROS_INFO_STREAM("================================== closest_uknown_is not defined");
		closest_unknown_point = closest_unknown_point_upper_bound;
	}
	//ROS_INFO_STREAM("unknown coordinats"<<msg->x << " "<<msg->y<< " " << msg->z);
}


TimeBudgetter *time_budgetter;

void next_steps_callback(const mavbench_msgs::multiDOFtrajectory::ConstPtr& msg){
	/*
	traj.clear();


	for (auto point_: msg->points){
    	multiDOFpoint point__;
    	convertMavBenchMultiDOFtoMultiDOF(point_, point__);
    	traj.push_back(point__);
    }


	double latency = 1.55; //TODO: get this from follow trajectory
	TimeBudgetter MacrotimeBudgetter(maxSensorRange, maxVelocity, accelerationCoeffs, TimeIncr, max_time_budget, g_planner_drone_radius);
	//double sensor_range_calc;
	auto macro_time_budgets = MacrotimeBudgetter.calcSamplingTime(traj, latency, closest_unknown_point, drone_position); // not really working well with cur_vel_mag
	double time_budget;
	if (msg->points.size() < 2 || macro_time_budgets.size() < 2){
		time_budgetting_failed = true;
		ROS_INFO_STREAM("failed to time budgget");
		return;
	}
	else if (macro_time_budgets.size() >= 1){
		time_budgetting_failed = false;
		time_budget = min (max_time_budget, macro_time_budgets[1]);
		time_budget -= time_budget*.2;
	}
	*/

    drone_position = drone->position();
	double velocity_magnitude = time_budgetter->get_velocity_projection_mag(traj[0], closest_unknown_point);
	time_budgetter->set_params(vmax_max,g_planner_drone_radius, g_planner_drone_radius_when_hovering);
	double time_budget = time_budgetter->calc_budget(*msg, &traj, closest_unknown_point, drone_position);
	if (time_budget == -10){
			time_budgetting_failed = true;
	}
	else{
			time_budgetting_failed = false;
	}


	//profiling_container->capture("time_budget", "single", time_budget, 1);


    //ROS_INFO_STREAM("time budget"<<time_budget<< "obstacle distance"<<control.inputs.obs_dist_statistics_min);

//	else{
//		time_budget = min(max_time_budget, macro_time_budgets[0]);
//	}
//	ROS_INFO_STREAM("---- next step"<< control_inputs.sensor_to_actuation_time_budget_to_enforce);

	control.internal_states.sensor_to_actuation_time_budget_to_enforce = time_budget;
	control.internal_states.drone_point_while_budgetting.x = drone_position.x;
	control.internal_states.drone_point_while_budgetting.y = drone_position.y;
	control.internal_states.drone_point_while_budgetting.z = drone_position.z;
	control.internal_states.drone_point_while_budgetting.vel_mag = velocity_magnitude;


	//	ros::param::set("sensor_to_actuation_time_budget_to_enforce", macro_time_budgets[1]);

	/*
	auto node_budget_vec = calc_micro_budget(macro_time_budgets[0]);

	// calculate each node's params
	vector<vector<ParamVal>> node_params_vec;
	for (auto &node_budget: node_budget_vec) {
		node_params_vec.push_back(calc_params(node_budget));
	}

	//set each node's params
	for (auto &node_params: node_params_vec) {
		for (auto param : node_params)
			;
			//set_param(param);
	}

	MacroBudgets.pop_front();
	*/

	return;
}


void initialize_global_params() {
	if(!ros::param::get("/sampling_interval", g_sampling_interval)){
		ROS_FATAL_STREAM("Could not start run_time_node sampling_interval not provided");
        exit(-1);
	}
	if(!ros::param::get("/design_mode", design_mode)){
		ROS_FATAL_STREAM("Could not start run_time_node sampling_interval not provided");
        exit(-1);
	}

	if(!ros::param::get("/DEBUG_VIS", DEBUG_VIS)){
		ROS_FATAL_STREAM("Could not start run_time_node DEBUG_VIS not provided");
        exit(-1);
	}


	if(!ros::param::get("/v_max", g_v_max)) {
		ROS_FATAL_STREAM("Could not start run_time_node v_max not provided");
        exit(-1);
	}

	if(!ros::param::get("/motivation_section", motivation_section)) {
		ROS_FATAL_STREAM("Could not start run_time_node motivation_section not provided");
        exit(-1);
	}

	if(!ros::param::get("/motivation_section_for_velocity", motivation_section_for_velocity)) {
		ROS_FATAL_STREAM("Could not start run_time_node motivation_section_for_velocity not provided");
        exit(-1);
	}

	if(!ros::param::get("/v_max", vmax_max)) {
		ROS_FATAL_STREAM("Could not start run_time_node v_max not provided");
		exit(-1);
	}


	if(!ros::param::get("/planner_drone_radius", g_planner_drone_radius)) {
		ROS_FATAL_STREAM("Could not start run_time_node planner_drone_radius not provided");
        exit(-1);
	}

	if(!ros::param::get("/planner_drone_radius_when_hovering", g_planner_drone_radius_when_hovering)) {
		ROS_FATAL_STREAM("Could not start planner_drone_radius_when_hovering not provided");
        exit(-1);
	}





	if(!ros::param::get("/sensor_max_range", g_sensor_max_range)) {

		ROS_FATAL_STREAM("Could not start run_time_node sensor_max_range not provided");
        exit(-1);
	}

	if(!ros::param::get("/ip_addr",ip_addr__global)){
		ROS_FATAL_STREAM("Could not start run_time_node ip_addr not provided");
        exit(-1);
	}


	maxSensorRange = g_sensor_max_range;
	TimeIncr = g_sampling_interval;
	maxVelocity = g_v_max;



}


int get_point_count(double resolution, vector<std::pair<double, int>>& pc_res_point_count){
	for (auto it= pc_res_point_count.begin(); it!=pc_res_point_count.end(); it++){
		if (it->first >= resolution){
			return it->second;
		}
	}

	ROS_ERROR_STREAM("should have found a resolution greater or equal to the requested one");
	exit(0);
}

/*
void static_budgetting(double cur_vel_mag, vector<std::pair<double, int>>& pc_res_point_count_vec){
	// -- knobs to set (and some temp values)
	double point_cloud_num_points;
	float MapToTransferSideLength;
	double pc_res;
	double om_to_pl_res;
	int pc_res_power_index; // -- temp

	// -- knob's boundaries; Note that for certain knobs, their max depends on the other knobs (e.g., point_cloud_num_points depends on resolution)
	double pc_res_max = .15;
	int num_of_steps_on_y = 4;
	double pc_res_min = pow(2, num_of_steps_on_y)*pc_res_max;  //this value must be a power of two
	static double static_pc_res = pc_res_max;
	//static double static_map_to_transfer_side_length = map_to_transfer_side_length_max;
	double pc_vol_ideal_max = 8000;
	double pc_vol_ideal_min = 100;
	double pc_vol_ideal_step_cnt = 30;
	static double  static_pc_vol_ideal = pc_vol_ideal_max;

	//double map_to_transfer_side_length_step_size = (map_to_transfer_side_length_max -  map_to_transfer_side_length_min)/map_to_transfer_side_length_step_cnt;
	double map_to_transfer_side_length_max = 500;
	double map_to_transfer_side_length_min = 40;
	double map_to_transfer_side_length_step_cnt = 20;
	double point_cloud_num_points_step_cnt = 2;
	double point_cloud_num_points_max;  // -- depends on resolution
	double point_cloud_num_points_min = 10;
	double om_to_pl_res_max = pc_res_max;
	double om_to_pl_res_min = pow(2, num_of_steps_on_y)*om_to_pl_res_max;  //this value must be a power of two
	static double static_om_to_pl_res = om_to_pl_res_max;
	double om_to_pl_vol_ideal_max = 200000; // -- todo: change to 20000; This value really depends on what we think the biggest map we
											   // -- wanna cover be, and match it to this value.
	double om_to_pl_vol_ideal_min = 3500;
	double om_to_pl_vol_ideal_step_cnt = 20;
	static double  static_om_to_pl_vol_ideal = om_to_pl_vol_ideal_max;



	double ppl_vol_ideal_max = 400000; // -- todo: change to 20000; This value really depends on what we think the biggest map we
											   // -- wanna cover be, and match it to this value.
	double ppl_vol_ideal_min = 100000;
	double ppl_vol_ideal_step_cnt = 20;
	static double  static_ppl_vol_ideal = ppl_vol_ideal_max;

    // not used any more
	static double static_point_cloud_num_points = (double) get_point_count(static_pc_res, pc_res_point_count_vec);
	double map_to_transfer_side_length_step_size = (map_to_transfer_side_length_max -  map_to_transfer_side_length_min)/map_to_transfer_side_length_step_cnt;


	// --initialize some knobs
	//pc_res = static_pc_res;
	static double static_point_cloud_num_points_max = (double) get_point_count(static_pc_res, pc_res_point_count_vec); // -- get the resolution and look into the vector to find the maximum number of points for a certain resolution
	static double static_point_cloud_num_points_step_size = (int) (static_point_cloud_num_points_max - point_cloud_num_points_min)/point_cloud_num_points_step_cnt;


	// -- feed forward (simple)
	static_pc_res =  pc_res_max;
	static_pc_vol_ideal  = pc_vol_ideal_max;
	static_om_to_pl_res = static_pc_res;
	static_om_to_pl_vol_ideal = om_to_pl_vol_ideal_max;
	static_ppl_vol_ideal = ppl_vol_ideal_max;// - ppl_vol_ideal_min)/(0 - g_v_max)*cur_vel_mag + ppl_vol_ideal_max;


	// -- set the parameters
	ros::param::set("pc_res", static_pc_res);
    profiling_container->capture("pc_res", "single", static_pc_res, g_capture_size);
    profiling_container->capture("om_to_pl_res", "single", static_om_to_pl_res, g_capture_size);
	ros::param::set("pc_vol_ideal", static_pc_vol_ideal);
	profiling_container->capture("pc_vol_ideal", "single", static_pc_vol_ideal, g_capture_size);
	profiling_container->capture("point_cloud_num_points", "single", static_point_cloud_num_points, g_capture_size);
// -- determine how much of the space to keep
//	ros::param::set("MapToTransferSideLength", MapToTransferSideLength);
	ros::param::set("om_to_pl_vol_ideal", static_om_to_pl_vol_ideal);
	ros::param::set("ppl_vol_ideal", static_ppl_vol_ideal);
	ros::param::set("om_to_pl_res", static_om_to_pl_res);

    if (DEBUG_RQT) {
    	debug_data.header.stamp = ros::Time::now();
    	debug_data.pc_res = profiling_container->findDataByName("pc_res")->values.back();
    	debug_data.om_to_pl_res = profiling_container->findDataByName("om_to_pl_res")->values.back();
    }
    //
}
*/



void performance_modeling(double cur_vel_mag, vector<std::pair<double, int>>& pc_res_point_count_vec){
	// -- knobs to set (and some temp values)
	double point_cloud_num_points;
	float MapToTransferSideLength;
	double pc_res;
	double om_to_pl_res;
	int pc_res_power_index; // -- temp

	// -- knob's boundaries; Note that for certain knobs, their max depends on the other knobs (e.g., point_cloud_num_points depends on resolution)
	//double pc_res_max = .15;
	int num_of_steps_on_y = num_of_res_to_try;// -1;
	double pc_res_min = pow(2, num_of_steps_on_y)*pc_res_max;  //this value must be a power of two
	static double static_pc_res = pc_res_max;
	//static double static_map_to_transfer_side_length = map_to_transfer_side_length_max;

	double pc_vol_ideal_step_cnt = 30;
	static double  static_pc_vol_ideal = pc_vol_ideal_max;

	//double map_to_transfer_side_length_step_size = (map_to_transfer_side_length_max -  map_to_transfer_side_length_min)/map_to_transfer_side_length_step_cnt;
	double map_to_transfer_side_length_max = 500;
	double map_to_transfer_side_length_min = 40;
	double map_to_transfer_side_length_step_cnt = 20;
	double point_cloud_num_points_step_cnt = 2;
	double point_cloud_num_points_max;  // -- depends on resolution
	double point_cloud_num_points_min = 10;
	double om_to_pl_res_max = pc_res_max;
	double om_to_pl_res_min = pow(2, num_of_steps_on_y)*om_to_pl_res_max;  //this value must be a power of two
	static double static_om_to_pl_res = om_to_pl_res_max;
											   // -- wanna cover be, and match it to this value.
	double om_to_pl_vol_ideal_min = pc_vol_ideal_min;
	double om_to_pl_vol_ideal_step_cnt = 20;
	static double  static_om_to_pl_vol_ideal = om_to_pl_vol_ideal_max;


										   // -- wanna cover be, and match it to this value.
	double ppl_vol_ideal_min = pc_vol_ideal_min;
	double ppl_vol_ideal_step_cnt = 20;
	static double  static_ppl_vol_ideal = ppl_vol_ideal_max;

    // not used any more
	static double static_point_cloud_num_points = (double) get_point_count(static_pc_res, pc_res_point_count_vec);
	double map_to_transfer_side_length_step_size = (map_to_transfer_side_length_max -  map_to_transfer_side_length_min)/map_to_transfer_side_length_step_cnt;


	// --initialize some knobs
	//pc_res = static_pc_res;
	static double static_point_cloud_num_points_max = (double) get_point_count(static_pc_res, pc_res_point_count_vec); // -- get the resolution and look into the vector to find the maximum number of points for a certain resolution
	static double static_point_cloud_num_points_step_size = (int) (static_point_cloud_num_points_max - point_cloud_num_points_min)/point_cloud_num_points_step_cnt;


	// -- knob performance modeling logic
	//ros::Duration(1).sleep();  // -- sleep enough so that the change can get sampled // TODO: this needs to change according to the knobs, or set to the worst case scenario, but for now we keep it simple for fast data collection
	// -- point cloud knobs (pointcloud/octomap since these knobs impact octomap)
	if (knob_performance_modeling_for_pc_om && last_modification(4)){
		//ros::Duration(4).sleep();  // -- sleep enough so that the change can get sampled // TODO: this needs to change according to the knobs, or set to the worst case scenario, but for now we keep it simple for fast data collection
		ROS_INFO_STREAM("------ changing the values");
		if (static_pc_vol_ideal < pc_vol_ideal_min){
			if (static_pc_res == pc_res_min){
				static_pc_res = pc_res_max;
			}else{
				static_pc_res = min(2*static_pc_res, pc_res_min);
			}
			//static_point_cloud_num_points_max = (double) get_point_count(static_pc_res, pc_res_point_count_vec); // -- get the resolution and look into the vector to find the maximum number of points for a certain resolution
			//static_point_cloud_num_points = static_point_cloud_num_points_max;
			//static_point_cloud_num_points_step_size = (int) (static_point_cloud_num_points_max - point_cloud_num_points_min)/point_cloud_num_points_step_cnt;
			static_pc_vol_ideal  = pc_vol_ideal_max;
		}else{
			static_pc_vol_ideal -= (pc_vol_ideal_max - pc_vol_ideal_min)/pc_vol_ideal_step_cnt;
		}
		static_om_to_pl_res = static_pc_res;  // set it equal (because we have an assertion that requires pc_res <= om_to_pl_res
	}

	// -- octomap to planning communication knobs
	if (knob_performance_modeling_for_om_to_pl && last_modification(12)){
		//ros::Duration(6).sleep();  // -- sleep enough so that the change can get sampled // TODO: this needs to change according to the knobs, or set to the worst case scenario, but for now we keep it simple for fast data collection
		if (static_om_to_pl_vol_ideal < om_to_pl_vol_ideal_min){
			static_om_to_pl_vol_ideal = om_to_pl_vol_ideal_max; // -- reset the map size
			if (static_om_to_pl_res == om_to_pl_res_min){
				static_om_to_pl_res = om_to_pl_res_max;
			}else{
				static_om_to_pl_res = min(2*static_om_to_pl_res, om_to_pl_res_min);
			}

		}else{
			static_om_to_pl_vol_ideal -= (om_to_pl_vol_ideal_max - om_to_pl_vol_ideal_min)/om_to_pl_vol_ideal_step_cnt;
		}
		static_pc_res = static_om_to_pl_res;  // set it equal (because we have an assertion that requires pc_res <= om_to_pl_res
	}

	// -- piecewise planner
	if (knob_performance_modeling_for_piecewise_planner && last_modification(perf_iterative_cntr*max_time_budget_performance_modeling) ){
		//ros::Duration(20).sleep();  // -- sleep enough so that the change can get sampled // TODO: this needs to change according to the knobs, or set to the worst case scenario, but for now we keep it simple for fast data collection
		if (static_ppl_vol_ideal < ppl_vol_ideal_min){
			static_ppl_vol_ideal = ppl_vol_ideal_max; // -- reset the map size
			if (om_to_pl_res == om_to_pl_res_min){
				static_om_to_pl_res = om_to_pl_res_max;
			}else{
				static_om_to_pl_res = min(2*static_om_to_pl_res, om_to_pl_res_min);
			}

		}else{
			static_ppl_vol_ideal -= (ppl_vol_ideal_max - ppl_vol_ideal_min)/ppl_vol_ideal_step_cnt;
		}
	}

	// -- sanity check
	//assert(knob_performance_modeling_for_pc_om ^ knob_performance_modeling_for_om_to_pl);///, "could not have both of the knobs to be true"); // this is a hack, but we actually want to have the capability to simaltenouysly modify both of the kernels
	pc_res_power_index = 0;
	//static_point_cloud_num_points -= static_point_cloud_num_points_step_size;


	// -- set the parameters
	ros::param::set("pc_res", static_pc_res);
	profiling_container->capture("pc_res", "single", pc_res, g_capture_size);
    profiling_container->capture("om_to_pl_res", "single", om_to_pl_res, g_capture_size);
    ros::param::set("point_cloud_num_points", static_point_cloud_num_points);
	ros::param::set("pc_vol_ideal", (static_pc_res/pc_res_max)*static_pc_vol_ideal);
	profiling_container->capture("pc_vol_ideal", "single", static_pc_vol_ideal, g_capture_size);
	//profiling_container->capture("point_cloud_num_points", "single", static_point_cloud_num_points, g_capture_size);
// -- determine how much of the space to keep
//	ros::param::set("MapToTransferSideLength", MapToTransferSideLength);
	ros::param::set("om_to_pl_vol_ideal", (static_om_to_pl_res/om_to_pl_res_max)*static_om_to_pl_vol_ideal);
	ros::param::set("ppl_vol_ideal", (static_om_to_pl_res/om_to_pl_res_max)*static_ppl_vol_ideal);
	ros::param::set("om_to_pl_res", static_om_to_pl_res);



	// -- determine the planning budgets
//	double ppl_time_budget_min = .01;
	//double ppl_time_budget_max = .5;
//	double ppl_time_budget = (ppl_time_budget_max - ppl_time_budget_min)/(pc_res_max- min_pc_res)*pc_res +
//			ppl_time_budget_max;

	//vector<double> ppl_time_budget_vec{.8, .3, .1, .05, .01};
	vector<double> ppl_time_budget_vec{.8, .3, .1, .05, .01};
	double ppl_time_budget = ppl_time_budget_vec[pc_res_power_index];
	ros::param::set("ppl_time_budget", ppl_time_budget);
	double smoothening_budget = ppl_time_budget;
	ros::param::set("smoothening_budget", smoothening_budget);
    profiling_container->capture("ppl_time_budget", "single", ppl_time_budget, g_capture_size);
    profiling_container->capture("smoothening_budget", "single", smoothening_budget, g_capture_size);

	ros::param::set("new_control_data", true);
	ros::param::set("optimizer_succeeded", false);
	ros::param::set("log_control_data", true);


    if (DEBUG_RQT) {
    	debug_data.header.stamp = ros::Time::now();
    	debug_data.pc_res = profiling_container->findDataByName("pc_res")->values.back();
    	debug_data.om_to_pl_res = profiling_container->findDataByName("om_to_pl_res")->values.back();
    	//debug_data.point_cloud_num_points =  profiling_container->findDataByName("point_cloud_num_points")->values.back();
    	debug_data.ppl_time_budget =  profiling_container->findDataByName("ppl_time_budget")->values.back();
    	debug_data.smoothening_budget =  profiling_container->findDataByName("smoothening_budget")->values.back();
    }
    //
}





void reactive_budgetting(double cur_vel_mag, vector<std::pair<double, int>>& pc_res_point_count_vec){
	// -- knobs to set (and some temp values)
	double point_cloud_num_points;
	float MapToTransferSideLength;
	double pc_res;
	double om_to_pl_res;
	int pc_res_power_index; // -- temp

	// -- knob's boundaries; Note that for certain knobs, their max depends on the other knobs (e.g., point_cloud_num_points depends on resolution)
	int num_of_steps_on_y = num_of_res_to_try;//-1;
	double pc_res_min = pow(2, num_of_steps_on_y)*pc_res_max;  //this value must be a power of two
	static double static_pc_res = pc_res_max;
	//static double static_map_to_transfer_side_length = map_to_transfer_side_length_max;
//	double pc_vol_ideal_max = 8000;
//	double pc_vol_ideal_min = 100;
	double pc_vol_ideal_step_cnt = 30;
	static double  static_pc_vol_ideal = pc_vol_ideal_max;

	//double map_to_transfer_side_length_step_size = (map_to_transfer_side_length_max -  map_to_transfer_side_length_min)/map_to_transfer_side_length_step_cnt;
	double map_to_transfer_side_length_max = 500;
	double map_to_transfer_side_length_min = 40;
	double map_to_transfer_side_length_step_cnt = 20;
	double point_cloud_num_points_step_cnt = 2;
	double point_cloud_num_points_max;  // -- depends on resolution
	double point_cloud_num_points_min = 10;
	double om_to_pl_res_max = pc_res_max;
	double om_to_pl_res_min = pow(2, num_of_steps_on_y)*om_to_pl_res_max;  //this value must be a power of two
	static double static_om_to_pl_res = om_to_pl_res_max;
	double om_to_pl_vol_ideal_max = 200000; // -- todo: change to 20000; This value really depends on what we think the biggest map we
											   // -- wanna cover be, and match it to this value.
	double om_to_pl_vol_ideal_min = 3500;
	double om_to_pl_vol_ideal_step_cnt = 20;
	static double  static_om_to_pl_vol_ideal = om_to_pl_vol_ideal_max;



	double ppl_vol_ideal_max = 400000; // -- todo: change to 20000; This value really depends on what we think the biggest map we
											   // -- wanna cover be, and match it to this value.
	double ppl_vol_ideal_min = 100000;
	double ppl_vol_ideal_step_cnt = 20;
	static double  static_ppl_vol_ideal = ppl_vol_ideal_max;

    // not used any more
	static double static_point_cloud_num_points = (double) get_point_count(static_pc_res, pc_res_point_count_vec);
	double map_to_transfer_side_length_step_size = (map_to_transfer_side_length_max -  map_to_transfer_side_length_min)/map_to_transfer_side_length_step_cnt;


	// --initialize some knobs
	//pc_res = static_pc_res;
	static double static_point_cloud_num_points_max = (double) get_point_count(static_pc_res, pc_res_point_count_vec); // -- get the resolution and look into the vector to find the maximum number of points for a certain resolution
	static double static_point_cloud_num_points_step_size = (int) (static_point_cloud_num_points_max - point_cloud_num_points_min)/point_cloud_num_points_step_cnt;


	// -- feed forward (simple)
	float offset_v_max = g_v_max/num_of_steps_on_y; // this is used to offsset the g_v_max; this is necessary otherwise, the step function basically never reacehs the min_pc_res
	double pc_res_temp =  (pc_res_max - pc_res_min)/(0 - (g_v_max-offset_v_max)) * cur_vel_mag + pc_res_max;
	pc_res_power_index = int(log2(pc_res_temp/pc_res_max));

	static_pc_res =  pow(2, pc_res_power_index)*pc_res_max;
	static_pc_vol_ideal  = (pc_vol_ideal_max - pc_vol_ideal_min)/(0 - g_v_max)*cur_vel_mag + pc_vol_ideal_max;
	static_om_to_pl_res = static_pc_res;
	static_om_to_pl_vol_ideal = (om_to_pl_vol_ideal_max - om_to_pl_vol_ideal_min)/(0 - g_v_max)*cur_vel_mag + om_to_pl_vol_ideal_max;
	static_ppl_vol_ideal = (ppl_vol_ideal_max - ppl_vol_ideal_min)/(0 - g_v_max)*cur_vel_mag + ppl_vol_ideal_max;

	// -- determine the number of points within point cloud
	/*
	double max_point_cloud_point_count_max_resolution = (double) get_point_count(static_pc_res, pc_res_point_count_vec);
	double max_point_cloud_point_count_min_resolution = max_point_cloud_point_count_max_resolution/15;
	double min_point_cloud_point_count = max_point_cloud_point_count_min_resolution;
	//--  calculate the maximum number of points in an unfiltered point cloud as a function of resolution
	//    double modified_max_point_cloud_point_count = (max_point_cloud_point_count_max_resolution - max_point_cloud_point_count_min_resolution)/(pc_res_max - min_pc_res)*(pc_res - pc_res_max) + max_point_cloud_point_count_max_resolution;
	//    calucate num of points as function of velocity
	//static_point_cloud_num_points = (max_point_cloud_point_count_max_resolution - min_point_cloud_point_count)/(0 - g_v_max)*cur_vel_mag + max_point_cloud_point_count_max_resolution;
	//MapToTransferSideLength = 500 + (500 -40)/(0-g_v_max)*cur_vel_mag;
	 */



	// -- set the parameters
	ros::param::set("pc_res", static_pc_res);
    profiling_container->capture("pc_res", "single", pc_res, g_capture_size);
    profiling_container->capture("om_to_pl_res", "single", om_to_pl_res, g_capture_size);
	ros::param::set("point_cloud_num_points", static_point_cloud_num_points);
	ros::param::set("pc_vol_ideal", static_pc_vol_ideal);
	profiling_container->capture("pc_vol_ideal", "single", static_pc_vol_ideal, g_capture_size);
	profiling_container->capture("point_cloud_num_points", "single", static_point_cloud_num_points, g_capture_size);
// -- determine how much of the space to keep
//	ros::param::set("MapToTransferSideLength", MapToTransferSideLength);
	ros::param::set("om_to_pl_vol_ideal", static_om_to_pl_vol_ideal);
	ros::param::set("ppl_vol_ideal", static_ppl_vol_ideal);
	ros::param::set("om_to_pl_res", static_om_to_pl_res);



	// -- determine the planning budgets
//	double ppl_time_budget_min = .01;
	//double ppl_time_budget_max = .5;
//	double ppl_time_budget = (ppl_time_budget_max - ppl_time_budget_min)/(pc_res_max- min_pc_res)*pc_res +
//			ppl_time_budget_max;

	//vector<double> ppl_time_budget_vec{.8, .3, .1, .05, .01};
	//vector<double> ppl_time_budget_vec{.8, .3, .1, .05, .01};
	//double ppl_time_budget = ppl_time_budget_vec[pc_res_power_index];
	//ros::param::set("ppl_time_budget", ppl_time_budget);
	//double smoothening_budget = ppl_time_budget;
	//ros::param::set("smoothening_budget", smoothening_budget);
    //profiling_container->capture("ppl_time_budget", "single", ppl_time_budget, g_capture_size);
    //profiling_container->capture("smoothening_budget", "single", smoothening_budget, g_capture_size);

	ros::param::set("new_control_data", true);
	ros::param::set("optimizer_succeeded", false);
	ros::param::set("log_control_data", true);


    if (DEBUG_RQT) {
    	debug_data.header.stamp = ros::Time::now();
    	debug_data.pc_res = profiling_container->findDataByName("pc_res")->values.back();
    	debug_data.om_to_pl_res = profiling_container->findDataByName("om_to_pl_res")->values.back();
    	//debug_data.point_cloud_num_points =  profiling_container->findDataByName("point_cloud_num_points")->values.back();
    	//debug_data.ppl_time_budget =  profiling_container->findDataByName("ppl_time_budget")->values.back();
    	//debug_data.smoothening_budget =  profiling_container->findDataByName("smoothening_budget")->values.back();
    }
    //
}

bool goal_rcv_call_back(package_delivery::point::Request &req, package_delivery::point::Response &res){
	g_goal_pos = req.goal;
	goal_known = true;
}



double inline calc_dist(coord point1, multiDOFpoint point) {
    	double dx = point1.x - point.x;
    	double dy = point1.y - point.y;
    	double dz = point1.z - point.z;
    	return std::sqrt(dx*dx + dy*dy + dz*dz);
}


double inline calc_dist(coord point1, coord point) {
    	double dx = point1.x - point.x;
    	double dy = point1.y - point.y;
    	double dz = point1.z - point.z;
    	return std::sqrt(dx*dx + dy*dy + dz*dz);
}


void log_data_before_shutting_down()
{
    profiling_container->setStatsAndClear();
    profile_manager::profiling_data_srv profiling_data_srv_inst;
    profile_manager::profiling_data_verbose_srv profiling_data_verbose_srv_inst;

    profiling_data_verbose_srv_inst.request.key = ros::this_node::getName()+"_verbose_data";
    profiling_data_verbose_srv_inst.request.value = "\n" + profiling_container->getStatsInString();
    profile_manager_client->verboseClientCall(profiling_data_verbose_srv_inst);

}

void sigIntHandlerPrivate(int signo){
    if (signo == SIGINT) {
        log_data_before_shutting_down();
        ros::shutdown();
    }
    exit(0);
}

// *** F:DN main function
int main(int argc, char **argv)
{
    // ROS node initialization
    ros::init(argc, argv, "run_time_node", ros::init_options::NoSigintHandler);
    ros::NodeHandle n;//("~");
    //ros::NodeHandle n2("~");
   // ros::CallbackQueue callback_queue_1; // -- queues for next steps
    //ros::CallbackQueue callback_queue_2; // -- queues for control_inputs
    //n.setCallbackQueue(&callback_queue_1);
    //n2.setCallbackQueue(&callback_queue_2);

    signal(SIGINT, sigIntHandlerPrivate);
    profile_manager_client = new ProfileManager("client", "/record_profiling_data", "/record_profiling_data_verbose");

    //signal(SIGINT, sigIntHandler);
    last_modification_time = ros::Time::now();

    node_types.push_back("point_cloud");
    node_types.push_back("octomap");
    node_types.push_back("planning");

    std::string ns = ros::this_node::getName();
    ros::Subscriber next_steps_sub = n.subscribe<mavbench_msgs::multiDOFtrajectory>("/next_steps", 1, next_steps_callback);
    ros::Subscriber closest_unknown_sub = n.subscribe<mavbench_msgs::planner_info>("/closest_unknown_point", 1, closest_unknown_callback);

    initialize_global_params();
    ros::Subscriber control_sub = n.subscribe<mavbench_msgs::control>("/control_to_crun", 1, control_callback);
    ros::ServiceServer goal_rcv_service = n.advertiseService("goal_rcv_2", goal_rcv_call_back);

    ros::Publisher control_to_pyrun = n.advertise<mavbench_msgs::control>("control_to_pyrun", 1);//, connect_cb, connect_cb);
    ros::Publisher closest_unknown_marker_pub = n.advertise<visualization_msgs::Marker>("closest_unknown_marker", 1);//, connect_cb, connect_cb);

    profiling_container = new DataContainer();
    ROS_INFO_STREAM("ip to contact to now"<<ip_addr__global);
    bool use_pyrun = false;


    vector<std::pair<double,int>>pc_res_point_count;
	//.1, 14600
	//.2, 36700
	//.3 16500
	//.4 9500
	//.5 6000
	//.6 4200
	//.7 3000
	//.8 2400
	//.9 1900
	//1.0 1500
	//1.1 1270
	//1.2 1100
	//1.3 950
	//1.4 800
	//1.5 700
	//1.6 620
	//1.7 520
	//1.8 500
	//1.9 430
	//2.0 400
	//2.1 375
	//2.2 350
	//2.3 300
	//2.4 300
	//2.5 270
	//2.6 250
	//2.7 210
	//2.8 210
	//2.9 210
	//3 200
	//3.5 135
	//4 117
	//4.5 90

//    float pc_res_array[] = {.15, .2, .3, .4, .5, .6, .7, .8, .9, 1.0, 1.1, 1.2, 1.3, 1.4,
 //   							1.5, 1.6, 1.7, 1.8, 1.9, 2.0, 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 2.8, 2.9,
	//							3.0, 3.6, 4, 4.5};

//    float point_cloud_count_array[] = {146000, 36700, 16500, 9500, 6000, 4200, 3000, 2400, 1900, 1500, 1270,
 //   		1100, 950 , 800, 700, 620, 520, 500, 430, 400, 375, 350, 300, 300, 270, 250, 210, 210, 210,
	//							200, 135, 117, 90};


    float pc_res_array[] = {.15, .3, .6, 1.2, 2.4, 4.8};
    float point_cloud_count_array[] = {64280, 16416, 4165, 1075, 299, 77};

    for (int i=0; i< sizeof(pc_res_array)/sizeof(pc_res_array[0]); i++){
    	pc_res_point_count.push_back(
    			std::make_pair(pc_res_array[i], point_cloud_count_array[i]));
    }

    std::string ip_addr, localization_method;
    ros::param::get("/ip_addr", ip_addr);
    uint16_t port = 41451;

    if(!ros::param::get("/localization_method", localization_method)) {
        ROS_FATAL("Could not start occupancy map node. Localization parameter missing!");
        exit(-1);
    }

    /*
    if(!ros::param::get("/dynamic_budgetting", dynamic_budgetting)) {
        ROS_FATAL("Could not start occupancy map node. dynamic_budgetting parameter missing!");
        exit(-1);
    }
*/
    if(!ros::param::get("/budgetting_mode", budgetting_mode)) {
        ROS_FATAL("Could not start runtime node. budgetting_mode parameter missing!");
        exit(-1);
    }



    if(!ros::param::get("/reactive_runtime", reactive_runtime)) {
        ROS_FATAL("Could not start occupancy map node. reactive_runtime parameter missing!");
        exit(-1);
    }

   if(!ros::param::get("/DEBUG_RQT", DEBUG_RQT)) {
        ROS_FATAL("Could not start run_time_thread node. DEBG_RQT parameter missing!");
        exit(-1);
   }

   if(!ros::param::get("/capture_size", g_capture_size)){
      ROS_FATAL_STREAM("Could not start run_time_thread. capture_size not provided");
      exit(-1);
    }

    if(!ros::param::get("/knob_performance_modeling", knob_performance_modeling)){
    	ROS_FATAL_STREAM("Could not start runtime cloud knob_performance_modeling not provided");
    	exit(0);
    }
    if(!ros::param::get("/knob_performance_modeling_for_pc_om", knob_performance_modeling_for_pc_om)){
			ROS_FATAL_STREAM("Could not start runtime knob_performance_modeling_for_pc_om not provided");
			exit(0);
    }

   /*
    if(!ros::param::get("/point_cloud_num_points_step_size", point_cloud_num_points_step_size)){
    	ROS_FATAL_STREAM("Could not start runtime; point_cloud_num_points_step_size not provided");
    	exit(0);
    }
    */

    if(!ros::param::get("/knob_performance_modeling_for_om_to_pl", knob_performance_modeling_for_om_to_pl)){
			ROS_FATAL_STREAM("Could not start runtime; knob_performance_modeling_forr_om_to_pl not provided");
			exit(0);
    }

   if(!ros::param::get("/knob_performance_modeling_for_piecewise_planner", knob_performance_modeling_for_piecewise_planner)){
			ROS_FATAL_STREAM("Could not start runtime; knob_performance_modeling_for_piecewise_planner not provided");
			exit(0);
    }

    if(!ros::param::get("/pc_res_max", pc_res_max)){
			ROS_FATAL_STREAM("Could not start runtime; pc_res_max_for_piecewise_planner not provided");
			exit(0);
    }
    if(!ros::param::get("/num_of_res_to_try", num_of_res_to_try)){
			ROS_FATAL_STREAM("Could not start runtime; num_of_res_to_try_for_piecewise_planner not provided");
			exit(0);
    }

    if(!ros::param::get("/pc_vol_ideal_min", pc_vol_ideal_min)){
			ROS_FATAL_STREAM("Could not start runtime; pc_vol_ideal_min not provided");
			exit(0);
    }

   if(!ros::param::get("/pc_vol_ideal_max", pc_vol_ideal_max)){
			ROS_FATAL_STREAM("Could not start runtime; pc_vol_ideal_max not provided");
			exit(0);
    }


     if(!ros::param::get("/om_to_pl_vol_ideal_max", om_to_pl_vol_ideal_max)){
			ROS_FATAL_STREAM("Could not start runtime; om_topl_vol_ideal_max not provided");
			exit(0);
    }

  if(!ros::param::get("/ppl_vol_ideal_max", ppl_vol_ideal_max)){
			ROS_FATAL_STREAM("Could not start runtime; ppl_vol_ideal_max not provided");
			exit(0);
    }


   if(!ros::param::get("/ppl_vol_min_coeff", ppl_vol_min_coeff)){
			ROS_FATAL_STREAM("Could not start runtime; ppl_vol_min_coeff not provided");
			exit(0);
    }

   if(!ros::param::get("/planner_drone_radius", planner_drone_radius)){
			ROS_FATAL_STREAM("Could not start runtime; planner_drone_radius not provided");
			exit(0);
    }



   if(!ros::param::get("/use_pyrun", use_pyrun)){
			ROS_FATAL_STREAM("Could not start runtime; knob_performance_modeling_for_piecewise_planner not provided");
			exit(0);
    }



   /*
   ros::param::get("/sensor_to_actuation_time_budget_to_enforce", sensor_to_actuation_time_budget_to_enforce_static_value);
   ros::param::get("/om_latency_expected", om_latency_expected_static_value);
   ros::param::get("/om_to_pl_latency_expected", om_to_pl_latency_expected_static_value);
   ros::param::get("/ppl_latency_expected", ppl_latency_expected_static_value);
   ros::param::get("/ee_latency_expected", ee_latency_expected_static_value);
   ros::param::get("/x_coord_while_budgetting",x_coord_while_budgetting_static_value);
   ros::param::get("/y_coord_while_budgetting", y_coord_while_budgetting_static_value);
   ros::param::get("/vel_mag_while_budgetting", vel_mag_while_budgetting_static_value);
   // -- point cloud to octomap knobs
   ros::param::get("/pc_res", pc_res_static_value);
   ros::param::get("/pc_vol_ideal", pc_vol_ideal_static_value);
   // -- octomap to planner
   ros::param::get("/om_to_pl_res", om_to_pl_res_static_value);
   ros::param::get("/om_to_pl_vol_ideal", om_to_pl_vol_ideal_static_value);
   // -- piecewise planner
   ros::param::get("/ppl_vol_ideal", ppl_vol_ideal_static_value);
	*/

    /*
    if(!ros::param::get("/map_to_transfer_side_length_step_size", map_to_transfer_side_length_step_size)){
    	ROS_FATAL_STREAM("Could not start run time; map_to_transfer_side_length_step_size not provided");
    	exit(0);
    }
     */
    ros::Publisher runtime_debug_pub = n.advertise<mavbench_msgs::runtime_debug>("/runtime_debug", 1);
    ros::param::get("/max_time_budget", max_time_budget);
    ros::param::get("/max_time_budget", max_time_budget_performance_modeling); // this never changes
    ros::param::get("/perf_iterative_cntr", perf_iterative_cntr); // how mnay times to run the same configuration


    drone = new Drone(ip_addr.c_str(), port, localization_method);

    closest_unknown_point.x = drone->position().x  + g_sensor_max_range/3;
    closest_unknown_point.y = drone->position().y + g_sensor_max_range/3;
    closest_unknown_point.z = drone->position().z + g_sensor_max_range/3;

    //Drone drone(ip_addr__global.c_str(), port);
	ros::Rate pub_rate(50);
	//string state = "waiting_for_pc";
	last_drone_position = drone_position;
	bool new_control_data;
	while (ros::ok())
	{
		ros::spinOnce();

    	time_budgetter = new TimeBudgetter(maxSensorRange, maxVelocity, accelerationCoeffs, TimeIncr, max_time_budget, g_planner_drone_radius, design_mode);

    	//bool is_empty = callback_queue_1.empty();
    	//bool is_empty_2 = callback_queue_2.empty();
    	//callback_queue_1.callAvailable(ros::WallDuration());  // -- first, get the meta data (i.e., resolution, volume)
    	//callback_queue_2.callAvailable(ros::WallDuration());  // -- first, get the meta data (i.e., resolution, volume)
    	/*
    	if (state == "waiting_for_py_run"){
    		ros::param::get("/new_control_data", new_control_data);
    		if (new_control_data){
    			state = "waiting_for_pc";
    		}
    	}
		*/

    	if(!got_new_input && (budgetting_mode != "static") && (budgetting_mode != "performance_modeling")){
    		pub_rate.sleep();
    		continue;
    	}
    	/*
    	else if (state == "waiting_for_py_run"){
    		continue;
    	}
    	*/
    	got_new_input = false;
    	drone_vel = drone->velocity();
    	cur_vel_mag = (double) calc_vec_magnitude(drone_vel.linear.x, drone_vel.linear.y, drone_vel.linear.z);
    	drone_position = drone->position();
    	//ROS_INFO_STREAM("velocity in run time"<<cur_vel_mag);


    	if (!use_pyrun) { // if not using pyrun. This is mainly used for performance modeling and static scenarios
    		ros::param::set("velocity_to_budget_on", cur_vel_mag);
    		ros::param::get("/reactive_runtime", reactive_runtime);
    		ros::param::get("/knob_performance_modeling_for_om_to_pl", knob_performance_modeling_for_om_to_pl);
    		ros::param::get("/knob_performance_modeling_for_pc_om", knob_performance_modeling_for_pc_om);
    		ros::param::get("/knob_performance_modeling_for_piecewise_planner", knob_performance_modeling_for_piecewise_planner);
    		ros::param::get("/ppl_vol_min_coeff", ppl_vol_min_coeff);
    		assert (!(use_pyrun && reactive_runtime));
    		assert (!(use_pyrun && knob_performance_modeling_for_om_to_pl));
    		assert (!(use_pyrun && knob_performance_modeling_for_pc_om));
    		assert (!(use_pyrun && knob_performance_modeling_for_piecewise_planner));

    		if (budgetting_mode == "manual" || budgetting_mode == "static"){
    			ros::param::set("optimizer_succeeded", false);
    			ros::param::set("log_control_data", true);
    			ros::param::set("new_control_data", true);
    			ros::param::set("optimizer_failure_status", 0);
    		} else if (budgetting_mode == "dynamic"){
    			if (reactive_runtime){
    				reactive_budgetting(cur_vel_mag, pc_res_point_count);
    			}
    		} else if (budgetting_mode == "performance_modeling"){
    				performance_modeling(cur_vel_mag, pc_res_point_count);
    		} else{
    		  ROS_INFO_STREAM("budgetting mode" << budgetting_mode<< " not defined");
    		  exit(0);
    		}
    	    /*
    		else if (budgetting_mode == "static"){
    			static_budgetting(cur_vel_mag, pc_res_point_count);
    			ros::param::set("new_control_data", true);
    		}
    	*/
    		if (DEBUG_RQT) {runtime_debug_pub.publish(debug_data);}
    	}else{
    		ros::param::get("/v_max", g_v_max);
    		maxVelocity = g_v_max;
    		closest_unknown_point_upper_bound.x = drone->position().x  + (g_sensor_max_range/pow(3,.5))/4;
    		closest_unknown_point_upper_bound.y = drone->position().y + (g_sensor_max_range/pow(3, .5))/4;
    		closest_unknown_point_upper_bound.z = drone->position().z + (g_sensor_max_range/pow(3, .5))/4;

    		if (goal_known && !isnan(closest_unknown_point.x)){
    			double direct_length = calc_vec_magnitude(drone->position().x - g_goal_pos.x, drone->position().y - g_goal_pos.y, drone->position().z - g_goal_pos.z);
    			double volume_of_direct_distance_to_goal = 3.4*pow(planner_drone_radius, 2)*direct_length;
    			control.inputs.ppl_vol_min = float(ppl_vol_min_coeff*volume_of_direct_distance_to_goal);
    		}else{
    			closest_unknown_point.x = drone->position().x  + g_sensor_max_range/pow(3,.5);
    			closest_unknown_point.y = drone->position().y + g_sensor_max_range/pow(3, .5);
    			closest_unknown_point.z = drone->position().z + g_sensor_max_range/pow(3, .5);
    			control.inputs.ppl_vol_min = 0;
    		}

    		auto drone_position_when_took_sample =  drone->position();
    		if (traj.size() == 0 || time_budgetting_failed || planner_consecutive_failure_ctr>0 || motivation_section){
    			//ROS_INFO_STREAM("------------------------------------coming here now");
    			double closest_unknown_point_distance = calc_dist(drone_position, closest_unknown_point);
    			//ROS_INFO_STREAM("closest unknown distance"<<closest_unknown_point_distance);
    			double sensor_range = closest_unknown_point_distance;
    			//if (motivation_section_for_velocity){
    			//	sensor_range =  14;
    			//}

    			double time_budget = min(max_time_budget, calc_sensor_to_actuation_time_budget_to_enforce_based_on_current_velocity(cur_vel_mag, sensor_range));
    			//ROS_INFO_STREAM(" right here with time of budget of " <<time_budget);
    			//ROS_INFO_STREAM("failed to budgget, time_budgetg:"<< time_budget<< "  cur_vel_mag:"<<cur_vel_mag);
    			control.internal_states.sensor_to_actuation_time_budget_to_enforce = time_budget;
    		}
    		control.internal_states.closest_unknown_distance= calc_dist(drone_position, closest_unknown_point);
    		ROS_INFO_STREAM("sending msg from c_run to pyrun");
    		control_to_pyrun.publish(control);
    		//state = "waiting_for_py_run";
    		time_budgetting_failed  = false;

    		if (DEBUG_VIS){
    			auto marker = get_marker(closest_unknown_point);
    			closest_unknown_marker_pub.publish(marker);
    		}

    		auto time_between_budgettings = (ros::Time::now() - last_time_sent_budgets).toSec();
    		last_time_sent_budgets = ros::Time::now();
    		//ROS_INFO_STREAM("closest uknown distance:"<< calc_dist(drone_position, closest_unknown_point) << " budget "<<control.internal_states.sensor_to_actuation_time_budget_to_enforce << " velocity" << cur_vel_mag << " time between budgettins"<< time_between_budgettings);
    		ros::param::set("velocity_to_budget_on", cur_vel_mag);

    		if (DEBUG_RQT){
    			debug_data.header.stamp = ros::Time::now();
    			debug_data.closest_unknown_distance = calc_dist(drone_position, closest_unknown_point);
    			debug_data.cur_velocity = cur_vel_mag;
    			debug_data.distance_travelled = calc_dist(drone_position_when_took_sample, last_drone_position);
    			debug_data.sensor_to_actuation_time_budget_to_enforce = control.internal_states.sensor_to_actuation_time_budget_to_enforce;
    			runtime_debug_pub.publish(debug_data);
    		}
    		last_drone_position = drone_position;
    	}
	}




}



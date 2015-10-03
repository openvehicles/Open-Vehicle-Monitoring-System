#!/bin/bash

NAME=$1

if test "${NAME}" == "" ; then
	echo "Usage: $0 basename"
	echo "e.g. $0 130311-tour1"
	exit
fi

# common protocol record intro columns:
MPHEAD="mp_cmd,mp_cmdres,mp_row,mp_rowcnt,rec_type,rec_time"

echo "Fetch *-OVM-DebugCrash..."
(
	HEAD="${MPHEAD},0,version,crashcnt,crashreason,checkpoint"
	echo $HEAD
	./cmd.pl 32 "*-OVM-DebugCrash"
) > "${NAME}-debugcrash.csv"

echo "Fetch RT-BAT-P..."
(
	HEAD="${MPHEAD},packnr,volt_alertstatus,temp_alertstatus"
	HEAD+=",soc,soc_min,soc_max"
	HEAD+=",volt_act,volt_min,volt_max"
	HEAD+=",temp_act,temp_min,temp_max"
	HEAD+=",cell_volt_stddev_max,cmod_temp_stddev_max"
	HEAD+=",max_drive_pwr,max_recup_pwr"
	echo $HEAD
	./cmd.pl 32 "RT-BAT-P"
) > "${NAME}-battpack.csv"

echo "Fetch RT-BAT-C..."
(
	HEAD="${MPHEAD},cellnr,volt_alertstatus,temp_alertstatus"
	HEAD+=",volt_act,volt_min,volt_max,volt_maxdev"
	HEAD+=",temp_act,temp_min,temp_max,temp_maxdev"
	echo $HEAD
	./cmd.pl 32 "RT-BAT-C"
) > "${NAME}-battcell.csv"

echo "Fetch RT-PWR-Stats..."
(
	HEAD="${MPHEAD},0"
	HEAD+=",speed_const_dist,speed_const_use,speed_const_rec"
	HEAD+=",speed_const_cnt,speed_const_sum"
	HEAD+=",speed_accel_dist,speed_accel_use,speed_accel_rec"
	HEAD+=",speed_accel_cnt,speed_accel_sum"
	HEAD+=",speed_decel_dist,speed_decel_use,speed_decel_rec"
	HEAD+=",speed_decel_cnt,speed_decel_sum"
	HEAD+=",level_up_dist,level_up_hsum,level_up_use,level_up_rec"
	HEAD+=",level_down_dist,level_down_hsum,level_down_use,level_down_rec"
	HEAD+=",charge_used,charge_recd"
	echo $HEAD
	./cmd.pl 32 "RT-PWR-Stats"
) > "${NAME}-stats.csv"

echo "Fetch RT-GPS-Log..."
(
	HEAD="${MPHEAD},odometer_mi_10th,latitude,longitude,altitude,direction,speed"
	HEAD+=",gps_fix,gps_stale_cnt,gsm_signal"
	HEAD+=",current_power_w,power_used_wh,power_recd_wh,power_distance,min_power_w,max_power_w"
	HEAD+=",car_status"
	HEAD+=",max_drive_pwr_w,max_recup_pwr_w"
	HEAD+=",autodrive_level,autorecup_level"
	HEAD+=",min_current_a,max_current_a"
	echo $HEAD
	./cmd.pl 32 "RT-GPS-Log"
) > "${NAME}-gpslog.csv"

echo "Fetch RT-PWR-Log..."
(
	HEAD="${MPHEAD},0"
	HEAD+=",odometer_km,latitude,longitude,altitude"
	HEAD+=",chargestate,parktime_min"
	HEAD+=",soc,soc_min,soc_max"
	HEAD+=",power_used_wh,power_recd_wh,power_distance"
	HEAD+=",range_estim_km,range_ideal_km"
	HEAD+=",batt_volt,batt_volt_min,batt_volt_max"
	HEAD+=",batt_temp,batt_temp_min,batt_temp_max"
	HEAD+=",motor_temp,pem_temp"
	HEAD+=",trip_length_km,trip_soc_usage"
	HEAD+=",trip_avg_speed_kph,trip_avg_accel_kps,trip_avg_decel_kps"
	HEAD+=",charge_used_ah,charge_recd_ah,batt_capacity_prc"
	echo $HEAD
	./cmd.pl 32 "RT-PWR-Log"
) > "${NAME}-pwrlog.csv"

echo "Done."

ls -l ${NAME}-*

package com.openvehicles.OVMS;

import java.io.Serializable;
import java.util.Date;

public class CarData implements Serializable {
	/**
	 * 
	 */
	private static final long serialVersionUID = 9069218298370983462L;
	public String ServerNameOrIP = "www.openvehicles.com";
	public String VehicleID;
	public String CarPass; //server secret
	public String UserPass; //app secret
	
	public String VehicleImageDrawable;
	
	// DATA S
	public int Data_CarsConnected = 0;
	public int Data_SOC = 0;
	public String Data_DistanceUnit = "";
	public int Data_LineVoltage = 0;
	public int Data_ChargeCurrent = 0;
	public String Data_ChargeState = "";
	public String Data_ChargeMode = "";
	public int Data_IdealRange = 0;
	public int Data_EstimatedRange = 0;
	public Date Data_LastCarUpdate;
	public long Data_LastCarUpdate_raw = 0;
	
	// DATA L
	public double Data_Latitude = 0;
	public double Data_Longitude = 0;
	
	// DATA D, field 1
	public boolean Data_LeftDoorOpen = false;
	public boolean Data_RightDoorOpen = false;
	public boolean Data_ChargePortOpen = false;
	public boolean Data_PilotPresent = false;
	public boolean Data_Charging = false;
	public boolean Data_HandBrakeApplied = false;
	public boolean Data_CarPoweredON = false;

	// DATA D, field 2
	public boolean Data_BonnetOpen = false;
	public boolean Data_TrunkOpen = false;

	// DATA D, field 3-9
	public boolean Data_CarLocked = false;
	public double Data_TemperaturePEM = 0;
	public double Data_TemperatureMotor = 0;
	public double Data_TemperatureBattery = 0;
	public double Data_TripMeter = 0;
	public double Data_Odometer = 0;
	public double Data_Speed = 0;
	
	// DATA F
	public String Data_CarModuleFirmwareVersion = "";
	public String Data_VIN = "";
	public String Data_CarModuleGSMSignalLevel = "";
	
	// Data f
	public String Data_OVMSServerFirmwareVersion = "";

	// DATA W
	public double Data_FRWheelPressure = 0;
	public double Data_FRWheelTemperature = 0;
	public double Data_RRWheelPressure = 0;
	public double Data_RRWheelTemperature = 0;
	public double Data_FLWheelPressure = 0;
	public double Data_FLWheelTemperature = 0;
	public double Data_RLWheelPressure = 0;
	public double Data_RLWheelTemperature = 0;

	public boolean ParanoidMode = false;
}

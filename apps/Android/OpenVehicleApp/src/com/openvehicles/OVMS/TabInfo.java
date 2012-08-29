package com.openvehicles.OVMS;

import java.util.Date;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.TextView;

public class TabInfo extends Activity {

	/** Called when the activity is first created. */
	@Override
	public void onCreate(Bundle savedInstanceState) {
	    super.onCreate(savedInstanceState);
		setContentView(R.layout.tabinfo);
		
		//notifications = new OVMSNotifications(this);
	}
	
	//private OVMSNotifications notifications;
	private CarData data;
	private Handler lastUpdateTimerHandler = new Handler();
	private AlertDialog softwareInformation;

	private Runnable lastUpdateTimer = new Runnable() {
		public void run() {
			// update the last updated textview
			updateLastUpdatedView();
			
			// schedule next run
			lastUpdateTimerHandler.postDelayed(lastUpdateTimer, 5000);
		}
	};

	@Override
	protected void onResume() {
		super.onResume();
		
		// set update timer
		lastUpdateTimerHandler.postDelayed(lastUpdateTimer, 5000);
	}

	@Override
	protected void onPause() {
		super.onPause();

		if ((softwareInformation != null) && softwareInformation.isShowing())
			softwareInformation.dismiss();
		
		// remove update timer
		lastUpdateTimerHandler.removeCallbacks(lastUpdateTimer);
	}
	
	private void updateLastUpdatedView() {
		if ((data == null) || (data.Data_LastCarUpdate == null))
			return;
		
		TextView tv = (TextView)findViewById(R.id.textLastUpdated);
		Date now = new Date();
		long lastUpdateSecondsAgo = (now.getDate() - data.Data_LastCarUpdate.getDate()) / 1000;
		Log.d("OVMS", "Last updated: " + lastUpdateSecondsAgo + " secs ago");
		
		if (lastUpdateSecondsAgo < 60)
			tv.setText("live");
		else if (lastUpdateSecondsAgo < 3600)
		{
			int displayValue = (int)Math.ceil(lastUpdateSecondsAgo / 60);
			tv.setText(String.format("Updated: %d minute%s ago", displayValue, (displayValue > 1) ? "s" : ""));
		}
		else if (lastUpdateSecondsAgo < 86400)
		{
			int displayValue = (int)Math.ceil(lastUpdateSecondsAgo / 3600);
			tv.setText(String.format("Updated: %d hour%s ago", displayValue, (displayValue > 1) ? "s" : ""));
		}
		else if (lastUpdateSecondsAgo < 864000)
		{
			int displayValue = (int)Math.ceil(lastUpdateSecondsAgo / 86400);
			tv.setText(String.format("Updated: %d day%s ago", displayValue, (displayValue > 1) ? "s" : ""));
		}
		else
			tv.setText(String.format(getString(R.string.LastUpdated), data.Data_LastCarUpdate));
	
	}

	private Handler handler = new Handler() {
		public void handleMessage(Message msg) {
			updateLastUpdatedView();
			
			TextView tv = (TextView)findViewById(R.id.textVehicleID);
			tv.setText(data.VehicleID);
			
			tv = (TextView)findViewById(R.id.textSOC);
			tv.setText(String.format(getString(R.string.SOC), data.Data_SOC));
			
			tv = (TextView)findViewById(R.id.textChargeStatus);
			if (data.Data_ChargeState.equals("charging"))
				tv.setText(String.format("Charging - %s (%sV %sA)", data.Data_ChargeMode, data.Data_LineVoltage, data.Data_ChargeCurrent)); 
			else
				tv.setText("");
			
			tv = (TextView)findViewById(R.id.textRange);
			String distanceUnit = " km";
			String speedUnit = " kph";
			if (data.Data_DistanceUnit != null && !data.Data_DistanceUnit.equals("K"))
			{
				distanceUnit = " miles";
				speedUnit = " mph";
			}
			tv.setText(String.format(getString(R.string.Range), data.Data_IdealRange + distanceUnit, data.Data_EstimatedRange + distanceUnit));
			
			/*tv = (TextView)findViewById(R.id.textVehicleSpeed);
			if (data.Data_Speed > 0)
				tv.setText(String.format("%d%s", (int)data.Data_Speed, speedUnit));
			else
				tv.setText("");*/
			
			ImageView iv = (ImageView)findViewById(R.id.imgError);
			iv.setVisibility(View.INVISIBLE);

			iv = (ImageView)findViewById(R.id.imgLock);
			iv.setVisibility( (data.ParanoidMode) ? View.VISIBLE : View.INVISIBLE);

			iv = (ImageView)findViewById(R.id.imgBatteryOverlay);
			iv.getLayoutParams().width = 268 * data.Data_SOC / 100;
			//iv.getLayoutParams().height = 70;
			
			iv = (ImageView)findViewById(R.id.imgCar);
			int resId = getResources().getIdentifier(data.VehicleImageDrawable, "drawable", "com.openvehicles.OVMS");
			iv.setImageResource(resId);
			
			iv.setOnClickListener(new OnClickListener() {

				public void onClick(View v) {
					AlertDialog.Builder builder = new AlertDialog.Builder(
							TabInfo.this);
					builder.setMessage(String.format("Power: %s\nVIN: %s\nGSM Signal: %s\nHandbrake: %s\n\nCar Module: %s\nOVMS Server: %s", (data.Data_CarPoweredON ? "ON" : "OFF"), data.Data_VIN,data.Data_CarModuleGSMSignalLevel,(data.Data_HandBrakeApplied ? "ENGAGED" : "DISENGAGED"), data.Data_CarModuleFirmwareVersion, data.Data_OVMSServerFirmwareVersion))
							.setTitle("Software Information")
							.setCancelable(false)
							.setPositiveButton("Close",
									new DialogInterface.OnClickListener() {
										public void onClick(DialogInterface dialog,
												int id) {
											dialog.dismiss();
										}
									});
					softwareInformation = builder.create();
					softwareInformation.show();
				}
			});
			
			/*
			// check if there is any saved notifications for this car 
			ImageButton btn = (ImageButton)findViewById(R.id.btnNotifications);
			boolean notificationFound = false;
			for (NotificationData n : notifications.Notifications)
			{
				if (n.Title.equals(data.VehicleID))
				{
					notificationFound = true;
					break;
				}
			}
			btn.setVisibility(notificationFound ? View.VISIBLE : View.INVISIBLE);
			if (notificationFound)
				btn.setOnClickListener(new OnClickListener() {
					public void onClick(View arg0) {
						((OVMSActivity)getParent()).getTabHost().setCurrentTab(2);
					}
				});
				*/
		}
	};
	
	public void RefreshStatus(CarData carData) {
		data = carData;
		handler.sendEmptyMessage(0);

	}

}

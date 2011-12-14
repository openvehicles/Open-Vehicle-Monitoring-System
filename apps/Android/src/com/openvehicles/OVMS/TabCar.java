package com.openvehicles.OVMS;

import java.util.Date;

import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.TextView;

public class TabCar extends Activity {

	/** Called when the activity is first created. */
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.tabcar);

	}

	private CarData data;
	private Handler lastUpdateTimerHandler = new Handler();

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

		// remove update timer
		lastUpdateTimerHandler.removeCallbacks(lastUpdateTimer);
	}

	private void updateLastUpdatedView() {
		if ((data == null) || (data.Data_LastCarUpdate == null))
			return;

		TextView tv = (TextView) findViewById(R.id.tabCarTextLastUpdated);
		Date now = new Date();
		long lastUpdateSecondsAgo = (now.getDate() - data.Data_LastCarUpdate
				.getDate()) / 1000;

		if (lastUpdateSecondsAgo < 60)
			tv.setText("live");
		else if (lastUpdateSecondsAgo < 3600) {
			int displayValue = (int) Math.ceil(lastUpdateSecondsAgo / 60);
			tv.setText(String.format("Updated: %d minute%s ago", displayValue,
					(displayValue > 1) ? "s" : ""));
		} else if (lastUpdateSecondsAgo < 86400) {
			int displayValue = (int) Math.ceil(lastUpdateSecondsAgo / 3600);
			tv.setText(String.format("Updated: %d hour%s ago", displayValue,
					(displayValue > 1) ? "s" : ""));
		} else if (lastUpdateSecondsAgo < 864000) {
			int displayValue = (int) Math.ceil(lastUpdateSecondsAgo / 86400);
			tv.setText(String.format("Updated: %d day%s ago", displayValue,
					(displayValue > 1) ? "s" : ""));
		} else
			tv.setText(String.format(getString(R.string.LastUpdated),
					data.Data_LastCarUpdate));

	}

	private Handler handler = new Handler() {
		public void handleMessage(Message msg) {
			updateLastUpdatedView();

			TextView tv = (TextView) findViewById(R.id.tabCarTextVehicleID);
			tv.setText(data.VehicleID);

			tv = (TextView) findViewById(R.id.textLeftDoorOpen);
			tv.setVisibility(data.Data_LeftDoorOpen ? View.VISIBLE
					: View.INVISIBLE);
			tv = (TextView) findViewById(R.id.textRightDoorOpen);
			tv.setVisibility(data.Data_RightDoorOpen ? View.VISIBLE
					: View.INVISIBLE);
			tv = (TextView) findViewById(R.id.textChargePortOpen);
			tv.setVisibility(data.Data_ChargePortOpen ? View.VISIBLE
					: View.INVISIBLE);
			tv = (TextView) findViewById(R.id.textHoodOpen);
			tv.setVisibility(data.Data_BonnetOpen ? View.VISIBLE
					: View.INVISIBLE);
			tv = (TextView) findViewById(R.id.textTrunkOpen);
			tv.setVisibility(data.Data_TrunkOpen ? View.VISIBLE
					: View.INVISIBLE);

			tv = (TextView) findViewById(R.id.tabCarTextCarStats);
			tv.setText(String.format(
					"PEM: %d°„C\nMotor: %d°„C\nBatt: %d°„C\nSpeed: %dkph",
					(int) data.Data_TemperaturePEM,
					(int) data.Data_TemperatureMotor,
					(int) data.Data_TemperatureBattery, (int) data.Data_Speed)); // \nTrip:
																					// %d\nOdo:
																					// %d
																					// ,
																					// (int)data.Data_TripMeter,
																					// (int)data.Data_Odometer

			String tirePressureDisplayFormat = "%.1fpsi\n%.0f°„C";

			tv = (TextView) findViewById(R.id.textFLWheel);
			tv.setText(String.format(tirePressureDisplayFormat,
					data.Data_FLWheelPressure, data.Data_FLWheelTemperature));
			tv = (TextView) findViewById(R.id.textFRWheel);
			tv.setText(String.format(tirePressureDisplayFormat,
					data.Data_FRWheelPressure, data.Data_FRWheelTemperature));
			tv = (TextView) findViewById(R.id.textRLWheel);
			tv.setText(String.format(tirePressureDisplayFormat,
					data.Data_RLWheelPressure, data.Data_RLWheelTemperature));
			tv = (TextView) findViewById(R.id.textRRWheel);
			tv.setText(String.format(tirePressureDisplayFormat,
					data.Data_RRWheelPressure, data.Data_RRWheelTemperature));

			ImageView iv = (ImageView) findViewById(R.id.tabCarImageCarChargePortOpen);
			iv.setVisibility(data.Data_ChargePortOpen ? View.VISIBLE
					: View.INVISIBLE);
			iv = (ImageView) findViewById(R.id.tabCarImageCarHoodOpen);
			iv.setVisibility(data.Data_BonnetOpen ? View.VISIBLE
					: View.INVISIBLE);
			iv = (ImageView) findViewById(R.id.tabCarImageCarLeftDoorOpen);
			iv.setVisibility(data.Data_LeftDoorOpen ? View.VISIBLE
					: View.INVISIBLE);
			iv = (ImageView) findViewById(R.id.tabCarImageCarRightDoorOpen);
			iv.setVisibility(data.Data_RightDoorOpen ? View.VISIBLE
					: View.INVISIBLE);
			iv = (ImageView) findViewById(R.id.tabCarImageCarTrunkOpen);
			iv.setVisibility(data.Data_TrunkOpen ? View.VISIBLE
					: View.INVISIBLE);

			iv = (ImageView) findViewById(R.id.imageCarLocked);
			iv.setImageResource(data.Data_CarLocked ? R.drawable.carlock
					: R.drawable.carunlock);

			iv = (ImageView) findViewById(R.id.tabCarImageLock);
			iv.setVisibility((data.ParanoidMode) ? View.VISIBLE
					: View.INVISIBLE);

			iv = (ImageView) findViewById(R.id.tabCarImageError);
			iv.setVisibility(View.INVISIBLE);
		}
	};

	public void RefreshStatus(CarData carData) {
		Log.d("Tab", "TabCar Refresh");
		data = carData;
		handler.sendEmptyMessage(0);

	}

}

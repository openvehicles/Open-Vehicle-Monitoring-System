package com.openvehicles.OVMS;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.PendingIntent;
import android.app.ProgressDialog;
import android.app.TabActivity;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.os.Bundle;
import android.os.Handler;

import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.PrintWriter;
import java.math.BigInteger;
import java.net.Socket;
import java.net.SocketException;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Date;
import java.util.Random;
import java.util.UUID;

import javax.crypto.Cipher;
import javax.crypto.Mac;

import android.os.AsyncTask;
import android.telephony.TelephonyManager;
import android.text.ClipboardManager;
import android.util.Base64;
import android.util.Log;

import android.widget.*;
import android.widget.TabHost.OnTabChangeListener;
import android.util.*;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

public class OVMSActivity extends TabActivity implements OnTabChangeListener {
	/** Called when the activity is first created. */
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.main);

		// restore saved cars
		loadCars();

		// check for C2DM registration
		// Restore saved registration id
		SharedPreferences settings = getSharedPreferences("C2DM", 0);
		String registrationID = settings.getString("RegID", "");
		if (registrationID.length() == 0) {
			Log.d("C2DM", "Doing first time registration.");

			// No C2DM ID available. Register now.
			ProgressDialog progress = ProgressDialog.show(this,
					"Push Notification Network",
					"Sending one-time registration...");
			Intent registrationIntent = new Intent(
					"com.google.android.c2dm.intent.REGISTER");
			registrationIntent.putExtra("app",
					PendingIntent.getBroadcast(this, 0, new Intent(), 0)); // boilerplate
			registrationIntent.putExtra("sender", "openvehicles@gmail.com");
			startService(registrationIntent);
			progress.dismiss();

			c2dmReportTimerHandler.postDelayed(reportC2DMRegistrationID, 2000);

			/*
			 * unregister Intent unregIntent = new
			 * Intent("com.google.android.c2dm.intent.UNREGISTER");
			 * unregIntent.putExtra("app", PendingIntent.getBroadcast(this, 0,
			 * new Intent(), 0)); startService(unregIntent);
			 */
		} else {
			Log.d("C2DM", "Loaded Saved C2DM registration ID: "
					+ registrationID);
			c2dmReportTimerHandler.postDelayed(reportC2DMRegistrationID, 2000);
		}

		// setup tabs
		TabHost tabHost = getTabHost(); // The activity TabHost
		TabHost.TabSpec spec; // Reusable TabSpec for each tab
		Intent intent; // Reusable Intent for each tab
		// Create an Intent to launch an Activity for the tab (to be reused)
		intent = new Intent().setClass(this, TabInfo.class);
		// Initialize a TabSpec for each tab and add it to the TabHost
		spec = tabHost.newTabSpec("tabInfo");
		spec.setContent(intent);
		spec.setIndicator("",
				getResources().getDrawable(android.R.drawable.ic_menu_info_details));
		tabHost.addTab(spec);

		intent = new Intent().setClass(this, TabCar.class);
		// Initialize a TabSpec for each tab and add it to the TabHost
		spec = tabHost.newTabSpec("tabCar");
		spec.setContent(intent);
		spec.setIndicator("",
				getResources().getDrawable(android.R.drawable.ic_menu_preferences));// ic_menu_zoom
		tabHost.addTab(spec);

		intent = new Intent().setClass(this, TabMap.class);
		// Initialize a TabSpec for each tab and add it to the TabHost
		spec = tabHost.newTabSpec("tabMap");
		spec.setContent(intent);
		spec.setIndicator("",
 				getResources().getDrawable(android.R.drawable.ic_menu_compass));// ic_menu_mapmode
		tabHost.addTab(spec);

		intent = new Intent().setClass(this, TabNotifications.class);
		// Initialize a TabSpec for each tab and add it to the TabHost
		spec = tabHost.newTabSpec("tabNotifications");
		spec.setContent(intent);
		spec.setIndicator("",
 				getResources().getDrawable(android.R.drawable.ic_menu_agenda));
		tabHost.addTab(spec);

		intent = new Intent().setClass(this, TabCars.class);
		// Initialize a TabSpec for each tab and add it to the TabHost
		spec = tabHost.newTabSpec("tabCars");
		spec.setContent(intent);
		spec.setIndicator("",
				getResources().getDrawable(android.R.drawable.ic_menu_manage));// ic_menu_preferences
		tabHost.addTab(spec);

		// tcp task isnow started in the onResume method
		//tcpTask = new TCPTask(carData);
		//tcpTask.execute();
		
		getTabHost().setOnTabChangedListener(this);
		
		if (tabHost.getCurrentTabTag() == "")
			getTabHost().setCurrentTabByTag("tabInfo");
	}
	
	@Override
	public void onNewIntent(Intent launchingIntent)
	{
		TabHost tabHost = getTabHost();
		if ((launchingIntent != null) && launchingIntent.hasExtra("SetTab"))
			tabHost.setCurrentTabByTag(launchingIntent.getStringExtra("SetTab"));
		else
			tabHost.setCurrentTabByTag("tabInfo");
	}
	
	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		MenuInflater inflater = getMenuInflater();
		inflater.inflate(R.layout.main_menu, menu);
		return true;
	}

	@Override
	public boolean onOptionsItemSelected(MenuItem item) {
		// Handle item selection
		switch (item.getItemId()) {
		case R.id.menuQuit:
			finish();
			return true;
		case R.id.menuDeleteSavedNotifications:
			OVMSNotifications notifications = new OVMSNotifications(this);
			notifications.Notifications = new ArrayList<NotificationData>();
			notifications.Save();
			this.UpdateStatus();
			return true;
		default:
			return super.onOptionsItemSelected(item);
		}
	}

	private TCPTask tcpTask;
	private ArrayList<CarData> allSavedCars;
	private CarData carData;
	private final String settingsFileName = "OVMSSavedCars.obj";
	private boolean isLoggedIn;
	private Handler c2dmReportTimerHandler = new Handler();
	private Handler pingServerTimerHandler = new Handler();
	private Exception lastServerException;
	private AlertDialog alertDialog;
	public boolean SuppressServerErrorDialog = false;
	ProgressDialog progressLogin = null;

	private Runnable progressLoginCloseDialog = new Runnable() {
		public void run() {
			try {
				progressLogin.dismiss();
			} catch (Exception e)
			{}
		}
	};
	private Runnable progressLoginShowDialog = new Runnable() {
		public void run() {
			progressLogin = ProgressDialog.show(OVMSActivity.this, "",
					"Connecting to OVMS Server...");
		}
	};

	private Runnable serverSocketErrorDialog = new Runnable() {

		public void run() {
			if (SuppressServerErrorDialog)
				return;
			else if ((alertDialog != null) && alertDialog.isShowing())
				return; // do not show duplicated alert dialogs

			if (progressLogin != null)
				progressLogin.hide();

			AlertDialog.Builder builder = new AlertDialog.Builder(
					OVMSActivity.this);
			builder.setMessage(
					(isLoggedIn) ? String.format("OVMS Server Connection Lost")
							: String.format("Please check the following:\n1. OVMS Server address\n2. Your vehicle ID and passwords"))
					.setTitle("Communications Problem")
					.setCancelable(false)
					.setPositiveButton("Open Settings",
							new DialogInterface.OnClickListener() {
								public void onClick(DialogInterface dialog,
										int id) {
									OVMSActivity.this.getTabHost()
											.setCurrentTabByTag("tabCars");
								}
							});
			alertDialog = builder.create();
			alertDialog.show();
		}

	};

	private Runnable pingServer = new Runnable() {
		public void run() {
			if (isLoggedIn) {
				Log.d("OVMS", "Pinging server...");
				tcpTask.Ping();
			}

			pingServerTimerHandler.postDelayed(pingServer, 60000);
		}
	};

	private Runnable reportC2DMRegistrationID = new Runnable() {
		public void run() {
			// check if tcp connection is still active (it may be closed as the user leaves the program)
			if (tcpTask == null)
				return;
			
			SharedPreferences settings = getSharedPreferences("C2DM", 0);
			String registrationID = settings.getString("RegID", "");
			String uuid;

			if (!settings.contains("UUID")) {
				// generate a new UUID
				uuid = UUID.randomUUID().toString();
				Editor editor = getSharedPreferences("C2DM",
						Context.MODE_PRIVATE).edit();
				editor.putString("UUID", uuid);
				editor.commit();

				Log.d("OVMS", "Generated New C2DM UUID: " + uuid);
			} else {
				uuid = settings.getString("UUID", "");
				Log.d("OVMS", "Loaded Saved C2DM UUID: " + uuid);
			}

			// MP-0
			// p<appid>,<pushtype>,<pushkeytype>{,<vehicleid>,<netpass>,<pushkeyvalue>}
			if ((registrationID.length() == 0)
					|| !tcpTask
							.SendCommand(String.format(
									"MP-0 p%s,c2dm,production,%s,%s,%s", uuid,
									carData.VehicleID, carData.CarPass,
									registrationID))) {
				// command not successful, reschedule reporting after 5 seconds
				Log.d("OVMS", "Reporting C2DM ID failed. Rescheduling.");
				c2dmReportTimerHandler.postDelayed(reportC2DMRegistrationID,
						5000);
			}
		}
	};

	public void onTabChanged(String currentActivityId) {
		notifyTabUpdate(currentActivityId);
	}

	private void loginComplete() {
		isLoggedIn = true;
		this.runOnUiThread(progressLoginCloseDialog);
	}

	public void ChangeCar(CarData car) {
		this.runOnUiThread(progressLoginShowDialog);
		
		Log.d("OVMS", "Changed car to: " + car.VehicleID);

		isLoggedIn = false;

		// kill previous connection
		if (tcpTask != null) {
			Log.d("TCP", "Shutting down pervious TCP connection (ChangeCar())");
			SuppressServerErrorDialog = true;
			tcpTask.ConnClose();
			tcpTask.cancel(true);
			tcpTask = null;
			SuppressServerErrorDialog = false;
		}

		// start new connection
		carData = car;
		// reset the paranoid mode flag in car data
		// it will be set again when the TCP task detects paranoid mode messages
		car.ParanoidMode = false;
		tcpTask = new TCPTask(carData);
		Log.d("TCP", "Starting TCP Connection (ChangeCar())");
		tcpTask.execute();
		getTabHost().setCurrentTabByTag("tabInfo");
		UpdateStatus();
	}

	public void UpdateStatus() {
		Log.d("OVMS", "Status Update");
		String currentActivityId = getLocalActivityManager().getCurrentId();
		notifyTabUpdate(currentActivityId);
	}

	private void notifyTabUpdate(String currentActivityId) {
		Log.d("Tab", "Tab change to: " + currentActivityId);

		if (currentActivityId == "tabInfo") {
			TabInfo tab = (TabInfo) getLocalActivityManager().getActivity(
					currentActivityId);
			tab.RefreshStatus(carData);
		} else if (currentActivityId == "tabCar") {
			Log.d("Tab", "Telling tabCar to update");
			TabCar tab = (TabCar) getLocalActivityManager().getActivity(
					currentActivityId);
			tab.RefreshStatus(carData);
		} else if (currentActivityId == "tabMap") {
			TabMap tab = (TabMap) getLocalActivityManager().getActivity(
					currentActivityId);
			tab.RefreshStatus(carData);
		} else if (currentActivityId == "tabNotifications") {
			TabNotifications tab = (TabNotifications) getLocalActivityManager().getActivity(
					currentActivityId);
			tab.Refresh();
		} else if (currentActivityId == "tabCars") {
			TabCars tab = (TabCars) getLocalActivityManager().getActivity(
					currentActivityId);
			tab.LoadCars(allSavedCars);
		} else
			getTabHost().setCurrentTabByTag("tabInfo");
	}

	private void notifyServerSocketError(Exception e) {
		// only show alerts for specific errors
		lastServerException = e;
		OVMSActivity.this.runOnUiThread(serverSocketErrorDialog);
	}

	@Override
	protected void onResume() {
		super.onResume();

		//getTabHost().setOnTabChangedListener(this);
		ChangeCar(carData);
	}

	@Override
	protected void onPause() {
		super.onPause();

		//getTabHost().setOnTabChangedListener(null);

		try {
			if (tcpTask != null) {
				Log.d("TCP", "Shutting down TCP connection");
				tcpTask.ConnClose();
				tcpTask.cancel(true);
				tcpTask = null;
			}
		} catch (Exception e) {
		}

		saveCars();
	}
	
	protected void onDestory() {
		
	}

	private void loadCars() {
		try {
			Log.d("OVMS", "Loading saved cars from internal storage file: "
					+ settingsFileName);
			FileInputStream fis = this.openFileInput(settingsFileName);
			ObjectInputStream is = new ObjectInputStream(fis);
			allSavedCars = (ArrayList<CarData>) is.readObject();
			is.close();

			SharedPreferences settings = getSharedPreferences("OVMS", 0);
			String lastVehicleID = settings.getString("LastVehicleID", "")
					.trim();
			if (lastVehicleID.length() == 0) {
				{
					// no vehicle ID saved
					carData = allSavedCars.get(0);
				}
			} else {
				Log.d("OVMS", String.format(
						"Loaded %s cars. Last used car is ",
						allSavedCars.size(), lastVehicleID));
				for (int idx = 0; idx < allSavedCars.size(); idx++) {
					if ((allSavedCars.get(idx)).VehicleID.equals(lastVehicleID)) {
						carData = allSavedCars.get(idx);
						break;
					}
				}
				if (carData == null)
					carData = allSavedCars.get(0);
			}

		} catch (Exception e) {
			e.printStackTrace();
			Log.d("ERR", e.getMessage());

			Log.d("OVMS", "Invalid save file. Initializing with demo car.");
			// No settings file found. Initialize settings.
			allSavedCars = new ArrayList<CarData>();
			CarData demoCar = new CarData();
			demoCar.VehicleID = "DEMO";
			demoCar.CarPass = "DEMO";
			demoCar.UserPass = "DEMO";
			demoCar.ServerNameOrIP = "www.openvehicles.com";
			demoCar.VehicleImageDrawable = "car_models_signaturered";
			allSavedCars.add(demoCar);

			carData = demoCar;
			
			saveCars();
		}
	}

	public void saveCars() {
		try {
			Log.d("OVMS", "Saving cars to interal storage...");

			// save last used vehicleID
			SharedPreferences settings = getSharedPreferences("OVMS", 0);
			SharedPreferences.Editor editor = settings.edit();
			editor.putString("LastVehicleID", carData.VehicleID);
			editor.commit();

			FileOutputStream fos = this.openFileOutput(settingsFileName,
					Context.MODE_PRIVATE);
			ObjectOutputStream os = new ObjectOutputStream(fos);
			os.writeObject(allSavedCars);
			os.close();
		} catch (Exception e) {
			e.printStackTrace();
			Log.d("ERR", e.getMessage());
		}
	}

	private class TCPTask extends AsyncTask<Void, Integer, Void> {

		public Socket Sock;
		private CarData carData;
		private Cipher txcipher;
		private Cipher rxcipher;

		private byte[] pmDigest;
		private Cipher pmcipher;

		private PrintWriter Outputstream;
		private BufferedReader Inputstream;

		public TCPTask(CarData car) {
			carData = OVMSActivity.this.carData;
		}

		@Override
		protected void onProgressUpdate(Integer... values) {
		}

		@Override
		protected Void doInBackground(Void... arg0) {

			try {
				ConnInit();

				String rx, msg;
				while (Sock.isConnected()) {
					// if (Inputstream.ready()) {
					rx = Inputstream.readLine().trim();
					msg = new String(rxcipher.update(Base64.decode(rx, 0)))
							.trim();
					Log.d("OVMS", String.format("RX: %s (%s)", msg, rx));
					if (msg.substring(0, 5).equals("MP-0 "))
						HandleMessage(msg.substring(5));
					else
						Log.d("OVMS", "Unknown protection scheme");
					// }
					// short pause after receiving message
					try {
						Thread.sleep(100, 0);
					} catch (InterruptedException e) {
						// ??
					}
				}
			} catch (SocketException e) {
				// connection lost, attempt to reconnect
				try {
					Sock.close();
					Sock = null;
				} catch (Exception ex) {
				}
				ConnInit();
			} catch (IOException e) {
				if (!SuppressServerErrorDialog)
					notifyServerSocketError(e);
				e.printStackTrace();
			} catch (Exception e) {
				e.printStackTrace();
			}

			return null;
		}

		private void HandleMessage(String msg) {
			char code = msg.charAt(0);
			String cmd = msg.substring(1);

			if (code == 'E') {
				// We have a paranoid mode message
				char innercode = msg.charAt(1);
				if (innercode == 'T') {
					// Set the paranoid token
					Log.d("TCP", "ET MSG Received: " + msg);

					try {
						String pmToken = msg.substring(2);
						Mac pm_hmac = Mac.getInstance("HmacMD5");
						javax.crypto.spec.SecretKeySpec sk = new javax.crypto.spec.SecretKeySpec(
								carData.UserPass.getBytes(), "HmacMD5");
						pm_hmac.init(sk);
						pmDigest = pm_hmac.doFinal(pmToken.getBytes());
						Log.d("OVMS", "Paranoid Mode Token Accepted. Entering Privacy Mode.");
					} catch (Exception e) {
						Log.d("ERR", e.getMessage());
						e.printStackTrace();
					}
				} else if (innercode == 'M') {
					// Decrypt the paranoid message
					Log.d("TCP", "EM MSG Received: " + msg);

					code = msg.charAt(2);
					cmd = msg.substring(3);
					
					byte[] decodedCmd = Base64.decode(cmd,
							Base64.NO_WRAP);

					try {
						pmcipher = Cipher.getInstance("RC4");
						pmcipher.init(Cipher.DECRYPT_MODE,
								new javax.crypto.spec.SecretKeySpec(pmDigest,
										"RC4"));

						// prime ciphers
						String primeData = "";
						for (int cnt = 0; cnt < 1024; cnt++)
							primeData += "0";

						pmcipher.update(primeData.getBytes());
						cmd = new String(pmcipher.update(decodedCmd));
					} catch (Exception e) {
						Log.d("ERR", e.getMessage());
						e.printStackTrace();
					}

					// notify main process of paranoid mode detection
					if (!carData.ParanoidMode) {
						Log.d("OVMS", "Paranoid Mode Detected");
						carData.ParanoidMode = true;
						OVMSActivity.this.UpdateStatus();
					}
				}
			}

			Log.d("TCP", code + " MSG Received: " + cmd);
			switch (code) {
			case 'Z': // Number of connected cars
			{
				carData.Data_CarsConnected = Integer.parseInt(cmd);
				OVMSActivity.this.UpdateStatus();
				break;
			}
			case 'S': // STATUS
			{
				String[] dataParts = cmd.split(",\\s*");
				if (dataParts.length >= 8) {
					Log.d("TCP", "S MSG Validated");
					carData.Data_SOC = Integer.parseInt(dataParts[0]);
					carData.Data_DistanceUnit = dataParts[1].toString();
					carData.Data_LineVoltage = Integer.parseInt(dataParts[2]);
					carData.Data_ChargeCurrent = Integer.parseInt(dataParts[3]);
					carData.Data_ChargeState = dataParts[4].toString();
					carData.Data_ChargeMode = dataParts[5].toString();
					carData.Data_IdealRange = Integer.parseInt(dataParts[6]);
					carData.Data_EstimatedRange = Integer
							.parseInt(dataParts[7]);
				}

				Log.d("TCP", "Notify Vehicle Status Update: " + carData.VehicleID);
				if (OVMSActivity.this != null) // OVMSActivity may be null if it is not in foreground
					OVMSActivity.this.UpdateStatus();
				break;
			}
			case 'T': // TIME
			{
				if (cmd.length() > 0) {
					carData.Data_LastCarUpdate = new Date(
							(new Date()).getTime() - Long.parseLong(cmd) * 1000);
					carData.Data_LastCarUpdate_raw = Long.parseLong(cmd);
					OVMSActivity.this.UpdateStatus();
				} else
					Log.d("TCP", "T MSG Invalid");
				break;
			}
			case 'L': // LOCATION
			{
				String[] dataParts = cmd.split(",\\s*");
				if (dataParts.length >= 2) {
					Log.d("TCP", "L MSG Validated");
					carData.Data_Latitude = Double.parseDouble(dataParts[0]);
					carData.Data_Longitude = Double.parseDouble(dataParts[1]);
					// Update the visible location
					OVMSActivity.this.UpdateStatus();
				}
				break;
			}
			case 'D': // Doors and switches
			{
				String[] dataParts = cmd.split(",\\s*");
				if (dataParts.length >= 9) {
					Log.d("TCP", "D MSG Validated");
					int dataField = Integer.parseInt(dataParts[0]);
					carData.Data_LeftDoorOpen = ((dataField & 0x1) == 0x1);
					carData.Data_RightDoorOpen = ((dataField & 0x2) == 0x2);
					carData.Data_ChargePortOpen = ((dataField & 0x4) == 0x4);
					carData.Data_PilotPresent = ((dataField & 0x8) == 0x8);
					carData.Data_Charging = ((dataField & 0x10) == 0x10);
					// bit 5 is always 1
					carData.Data_HandBrakeApplied = ((dataField & 0x40) == 0x40);
					carData.Data_CarPoweredON = ((dataField & 0x80) == 0x80);

					dataField = Integer.parseInt(dataParts[1]);
					carData.Data_BonnetOpen = ((dataField & 0x40) == 0x40);
					carData.Data_TrunkOpen = ((dataField & 0x80) == 0x80);
					
					dataField = Integer.parseInt(dataParts[2]);
					carData.Data_CarLocked = (dataField == 4); // 4=locked, 5=unlocked
					
					carData.Data_TemperaturePEM = Double.parseDouble(dataParts[3]);
					carData.Data_TemperatureMotor = Double.parseDouble(dataParts[4]);
					carData.Data_TemperatureBattery = Double.parseDouble(dataParts[5]);
					carData.Data_TripMeter = Double.parseDouble(dataParts[6]);
					carData.Data_Odometer = Double.parseDouble(dataParts[7]);
					carData.Data_Speed = Double.parseDouble(dataParts[8]);
					
					// Update the displayed tab
					OVMSActivity.this.UpdateStatus();
				}
				break;
			}
			case 'F': // VIN and Firmware
			{
				String[] dataParts = cmd.split(",\\s*");
				if (dataParts.length >= 3) {
					Log.d("TCP", "F MSG Validated");
					carData.Data_CarModuleFirmwareVersion = dataParts[0].toString();
					carData.Data_VIN = dataParts[1].toString();
					carData.Data_CarModuleGSMSignalLevel = dataParts[2].toString();
					
					// Update the displayed tab
					OVMSActivity.this.UpdateStatus();
				}
			}
			case 'f': // OVMS Server Firmware
			{
				String[] dataParts = cmd.split(",\\s*");
				if (dataParts.length >= 1) {
					Log.d("TCP", "f MSG Validated");
					carData.Data_OVMSServerFirmwareVersion = dataParts[0].toString();
					
					// Update the displayed tab
					OVMSActivity.this.UpdateStatus();
				}
				break;
			}
			case 'W': // TPMS
			{
				String[] dataParts = cmd.split(",\\s*");
				if (dataParts.length >= 8) {
					Log.d("TCP", "W MSG Validated");
					carData.Data_FRWheelPressure = Double.parseDouble(dataParts[0]);
					carData.Data_FRWheelTemperature = Double.parseDouble(dataParts[1]);
					carData.Data_RRWheelPressure = Double.parseDouble(dataParts[2]);
					carData.Data_RRWheelTemperature = Double.parseDouble(dataParts[3]);
					carData.Data_FLWheelPressure = Double.parseDouble(dataParts[4]);
					carData.Data_FLWheelTemperature = Double.parseDouble(dataParts[5]);
					carData.Data_RLWheelPressure = Double.parseDouble(dataParts[6]);
					carData.Data_RLWheelTemperature = Double.parseDouble(dataParts[7]);

					// Update the displayed tab
					OVMSActivity.this.UpdateStatus();
				}
				break;
			}
			case 'a': {
				Log.d("TCP", "Server acknowleged ping");
				break;
			}
			}
		}

		public void Ping() {
			String msg = "TX: MP-0 A";
			Outputstream.println(Base64.encodeToString(
					txcipher.update(msg.getBytes()), Base64.NO_WRAP));
			Log.d("OVMS", msg);
		}

		public void ConnClose() {
			try {
				if ((Sock != null) && Sock.isConnected())
					Sock.close();
			} catch (Exception e) {
				e.printStackTrace();
			}
		}

		public boolean SendCommand(String command) {
			Log.d("OVMS", "TX: " + command);
			if (!OVMSActivity.this.isLoggedIn) {
				Log.d("OVMS", "Server not ready. TX aborted.");
				return false;
			}

			try {
				Outputstream.println(Base64.encodeToString(
						txcipher.update(command.getBytes()), Base64.NO_WRAP));
			} catch (Exception e) {
				notifyServerSocketError(e);
			}
			return true;
		}

		private void ConnInit() {
			String shared_secret = carData.CarPass;
			String vehicleID = carData.VehicleID;
			String b64tabString = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
			char[] b64tab = b64tabString.toCharArray();

			// generate session client token
			Random rnd = new Random();
			String client_tokenString = "";
			for (int cnt = 0; cnt < 22; cnt++)
				client_tokenString += b64tab[rnd.nextInt(b64tab.length - 1)];

			byte[] client_token = client_tokenString.getBytes();
			try {
				Mac client_hmac = Mac.getInstance("HmacMD5");
				javax.crypto.spec.SecretKeySpec sk = new javax.crypto.spec.SecretKeySpec(
						shared_secret.getBytes(), "HmacMD5");
				client_hmac.init(sk);
				byte[] hashedBytes = client_hmac.doFinal(client_token);
				String client_digest = Base64.encodeToString(hashedBytes,
						Base64.NO_WRAP);

				Sock = new Socket(carData.ServerNameOrIP, 6867);

				Outputstream = new PrintWriter(
						new java.io.BufferedWriter(
								new java.io.OutputStreamWriter(
										Sock.getOutputStream())), true);
				Log.d("OVMS", String.format("TX: MP-A 0 %s %s %s",
						client_tokenString, client_digest, vehicleID));

				Outputstream.println(String.format("MP-A 0 %s %s %s",
						client_tokenString, client_digest, vehicleID));// \r\n

				Inputstream = new BufferedReader(new InputStreamReader(
						Sock.getInputStream()));

				// get server welcome message
				String[] serverWelcomeMsg = null;
				try {
					serverWelcomeMsg = Inputstream.readLine().trim()
							.split("[ ]+");
				} catch (Exception e) {
					// if server disconnects us here, our auth is probably wrong
					if (!SuppressServerErrorDialog)
						notifyServerSocketError(e);
					return;
				}
				Log.d("OVMS", String.format("RX: %s %s %s %s",
						serverWelcomeMsg[0], serverWelcomeMsg[1],
						serverWelcomeMsg[2], serverWelcomeMsg[3]));

				String server_tokenString = serverWelcomeMsg[2];
				byte[] server_token = server_tokenString.getBytes();
				byte[] server_digest = Base64.decode(serverWelcomeMsg[3], 0);

				if (!Arrays.equals(client_hmac.doFinal(server_token),
						server_digest)) {
					// server hash failed
					Log.d("OVMS", String.format(
							"Server authentication failed. Expected %s Got %s",
							Base64.encodeToString(client_hmac
									.doFinal(serverWelcomeMsg[2].getBytes()),
									Base64.NO_WRAP), serverWelcomeMsg[3]));
				} else {
					Log.d("OVMS", "Server authentication OK.");
				}

				// generate client_key
				String server_client_token = server_tokenString
						+ client_tokenString;
				byte[] client_key = client_hmac.doFinal(server_client_token
						.getBytes());

				Log.d("OVMS", String.format(
						"Client version of the shared key is %s - (%s) %s",
						server_client_token, toHex(client_key).toLowerCase(),
						Base64.encodeToString(client_key, Base64.NO_WRAP)));

				// generate ciphers
				rxcipher = Cipher.getInstance("RC4");
				rxcipher.init(Cipher.DECRYPT_MODE,
						new javax.crypto.spec.SecretKeySpec(client_key, "RC4"));

				txcipher = Cipher.getInstance("RC4");
				txcipher.init(Cipher.ENCRYPT_MODE,
						new javax.crypto.spec.SecretKeySpec(client_key, "RC4"));

				// prime ciphers
				String primeData = "";
				for (int cnt = 0; cnt < 1024; cnt++)
					primeData += "0";

				rxcipher.update(primeData.getBytes());
				txcipher.update(primeData.getBytes());

				Log.d("OVMS", String.format(
						"Connected to %s. Ciphers initialized. Listening...",
						carData.ServerNameOrIP));

				loginComplete();

			} catch (UnknownHostException e) {
				notifyServerSocketError(e);
				e.printStackTrace();
			} catch (IOException e) {
				notifyServerSocketError(e);
				e.printStackTrace();
			} catch (NullPointerException e) {
				// notifyServerSocketError(e);
				e.printStackTrace();
			} catch (Exception e) {
				e.printStackTrace();
			} /*
			 * catch (java.net.UnknownHostException e) { // TODO Auto-generated
			 * catch block e.printStackTrace(); } catch (java.io.IOException e)
			 * { // TODO Auto-generated catch block e.printStackTrace(); }
			 */

		}

		private String toHex(byte[] bytes) {
			BigInteger bi = new BigInteger(1, bytes);
			return String.format("%0" + (bytes.length << 1) + "X", bi);
		}

	}

}
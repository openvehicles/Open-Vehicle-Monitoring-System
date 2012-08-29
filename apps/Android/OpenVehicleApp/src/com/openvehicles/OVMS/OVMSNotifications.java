package com.openvehicles.OVMS;

import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.Serializable;
import java.util.ArrayList;
import java.util.Date;

import android.content.Context;
import android.util.Log;

public class OVMSNotifications {
	private final String settingsFileName = "OVMSSavedNotifications.obj";
	
	public ArrayList<NotificationData> Notifications;
	private Context mContext;
	
	@SuppressWarnings("unchecked")
	public OVMSNotifications(Context context) {
		mContext = context;
		try {
			Log.d("OVMS", "Loading saved notifications list from internal storage file: "
					+ settingsFileName);
			FileInputStream fis = context.openFileInput(settingsFileName);
			ObjectInputStream is = new ObjectInputStream(fis);
			Notifications = (ArrayList<NotificationData>) is.readObject();
			is.close();
			Log.d("OVMS", String.format("Loaded %s saved notifications.", Notifications.size()));
		} catch (Exception e) {
			//e.printStackTrace();
			Log.d("ERR", e.getMessage());

			Log.d("OVMS", "Initializing with save notifications list.");
			Notifications = new ArrayList<NotificationData>();
			
			// load demos
			this.AddNotification("Push Notifications", "Push notifications received for your registered vehicles are archived here.");
			this.Save();
		}
	}
	
	public void AddNotification(NotificationData notification)
	{
		Notifications.add(notification);
	}
	
	public void AddNotification(String title, String message)
	{
		Date timestamp = new Date();
		Notifications.add(new NotificationData(timestamp, title, message));
	}
	
	public void Save()
	{
		try {
			Log.d("OVMS", "Saving notifications list to interal storage...");

			FileOutputStream fos = mContext.openFileOutput(settingsFileName,
					Context.MODE_PRIVATE);
			ObjectOutputStream os = new ObjectOutputStream(fos);
			os.writeObject(this.Notifications);
			os.close();
			Log.d("OVMS", String.format("Saved %s notifications.", Notifications.size()));
		} catch (Exception e) {
			e.printStackTrace();
			Log.d("ERR", e.getMessage());
		}
	}
}

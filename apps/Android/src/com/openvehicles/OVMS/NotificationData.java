package com.openvehicles.OVMS;

import java.io.Serializable;
import java.util.Date;

public class NotificationData implements Serializable {
	public Date Timestamp;
	public String Title;
	public String Message;
	
	public NotificationData() {
		
	}
	
	public NotificationData(Date timestamp, String title, String message) {
		this.Timestamp = timestamp;
		this.Title = title;
		this.Message = message;
	}
}

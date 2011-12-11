package com.openvehicles.OVMS;

import java.text.SimpleDateFormat;
import java.util.ArrayList;

import com.google.android.maps.GeoPoint;
import com.google.android.maps.OverlayItem;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.ListActivity;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.LayoutInflater;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.AdapterView.AdapterContextMenuInfo;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

public class TabNotifications extends ListActivity {

	/** Called when the activity is first created. */
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.tabnotifications);
		
		notifications = new OVMSNotifications(this);
		mContext = this;
	}
	
	private ItemsAdapter adapter;
	private NotificationData[] cachedData;
	private OVMSNotifications notifications;
	private Context mContext;
	
	private class ItemsAdapter extends ArrayAdapter<NotificationData> {

		private NotificationData[] items;

		public ItemsAdapter(Context context, int textViewResourceId,
				NotificationData[] items) {
			super(context, textViewResourceId, items);
			this.items = items;
		}

		@Override
		public View getView(int position, View convertView, ViewGroup parent) {
			View v = convertView;
			if (v == null) {
				LayoutInflater vi = (LayoutInflater) getSystemService(Context.LAYOUT_INFLATER_SERVICE);
				v = vi.inflate(R.layout.tabnotifications_listitem, null);
			}

			NotificationData it = items[position];
			if (it != null) {
				TextView tv = (TextView) v.findViewById(R.id.textNotificationsListTitle);
				tv.setText(it.Title);
				tv = (TextView) v.findViewById(R.id.textNotificationsListMessage);
				tv.setText(it.Message);

				SimpleDateFormat fmt = new SimpleDateFormat("MMM d, k:mm");
				tv = (TextView) v.findViewById(R.id.textNotificationsListTimestamp);
				tv.setText(fmt.format(it.Timestamp));

			}
			
			return v;
		}
	}

	@Override
	protected void onListItemClick(ListView l, View v, int position, long id) {
		Log.d("OVMS", "Displaying notification: #" + position);
		
		AlertDialog.Builder builder = new AlertDialog.Builder(
				TabNotifications.this);
		builder.setMessage(cachedData[position].Message)
				.setTitle(cachedData[position].Title)
				.setCancelable(false)
				.setPositiveButton("Close",
						new DialogInterface.OnClickListener() {
							public void onClick(DialogInterface dialog,
									int id) {
								dialog.dismiss();
							}
						});
		builder.create().show();
	}
	
	private Handler handler = new Handler() {
		public void handleMessage(Message msg) {
			NotificationData[] data = new NotificationData[notifications.Notifications.size()];
			notifications.Notifications.toArray(data);
			
			// reverse the array so that the latest notification is displayed on top
			cachedData = new NotificationData[data.length];
			for (int idx=0; idx<cachedData.length; idx++)
				cachedData[idx] = data[data.length - 1 - idx];
			
			adapter = new ItemsAdapter(mContext, R.layout.tabnotifications_listitem,
					cachedData);
			setListAdapter(adapter);
		}
	};

	public void Refresh() {
		notifications = new OVMSNotifications(this);
		handler.sendEmptyMessage(0);

	}

}

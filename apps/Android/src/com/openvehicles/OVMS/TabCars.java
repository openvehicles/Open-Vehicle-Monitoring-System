package com.openvehicles.OVMS;

import java.util.ArrayList;

import com.google.android.maps.GeoPoint;
import com.google.android.maps.OverlayItem;

import android.app.Activity;
import android.app.ListActivity;
import android.content.Context;
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

public class TabCars extends ListActivity {

	/** Called when the activity is first created. */
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.tabcars);
		
		mContext = this;
		
		ImageButton btn = (ImageButton) findViewById(R.id.btnAddCar);
		btn.setOnClickListener(new OnClickListener() {
			public void onClick(View v) {
				editCar(null); // add new car
			}
		});

	}
	
	@Override
	public void onCreateContextMenu(ContextMenu menu, View v,
	                                ContextMenuInfo menuInfo) {
	  super.onCreateContextMenu(menu, v, menuInfo);
	  MenuInflater inflater = getMenuInflater();
	  inflater.inflate(R.layout.carlist_menu, menu);
	}
	
	@Override
	public boolean onContextItemSelected(MenuItem item) {
	  AdapterContextMenuInfo info = (AdapterContextMenuInfo) item.getMenuInfo();
	  switch (item.getItemId()) {
	  case R.id.menuCarListEdit:
	    return true;
	  case R.id.menuCarListDelete:
	    return true;
	  default:
	    return super.onContextItemSelected(item);
	  }
	}

	private ItemsAdapter adapter;
	private ArrayList<CarData> _allSavedCars;
	private CarData[] carsList;
	private Context mContext;
	
	private void carClicked(CarData carSelected)
	{
		((OVMSActivity)this.getParent()).ChangeCar(carSelected);
	}
	
	private void editCar(CarData car)
	{
		// Build a list of existing vehicle IDs so the editor can check for duplication upon exit
		ArrayList<String> existingVehicleIDs = new ArrayList<String>();		
		for (int idx=0;idx<_allSavedCars.size();idx++)
			existingVehicleIDs.add(_allSavedCars.get(idx).VehicleID);

		Log.d("OVMS", String.format("Starting car editor (%s in existing cars list)", existingVehicleIDs.size()));

		Intent intent = new Intent(mContext, CarEditor.class);
		intent.putExtra("ExistingVehicleIDs", existingVehicleIDs);
		if (car != null)
		{
			// Edit Existing Car
			existingVehicleIDs.remove(car.VehicleID);
			intent.putExtra("Car", car);
		}
		startActivityForResult(intent, 0);
	}
	
	@Override
	protected void onActivityResult(int requestCode, int resultCode, Intent data) {
	    super.onActivityResult(requestCode, resultCode, data);
	    if (data == null) // user pressed cancel or the back button
	    	{
		    Log.d("OVMS", "Editor cancelled.");
	    	return;	    	
	    	}
	    
		CarData editedCar = (CarData)data.getExtras().getSerializable("Car");
		String actionCode = data.getExtras().getString("ActionCode");
		String originalVehicleID = data.getExtras().getString("OriginalVehicleID");

	    Log.d("OVMS", String.format("Editor closed with result action: %s %s", actionCode, originalVehicleID));
	    

	    if (actionCode.equals("CANCEL"))
	    {
	    	Toast.makeText(getBaseContext(), "Cancelled", 1000).show();
	    	return;
	    }
	    else if (actionCode.equals("SAVE"))
	    {
			// check vehicle ID for duplicates (only if ID is changed)
	    	if (!editedCar.VehicleID.equals(editedCar.VehicleID))
	    	{
				for (int idx=0; idx<_allSavedCars.size(); idx++)
			    {
			    	if ((_allSavedCars.get(idx)).VehicleID.equals(editedCar.VehicleID))
			    	{
			    		Log.d("OVMS", String.format("Vehicle ID duplicated. Aborting save."));
				    	Toast.makeText(getBaseContext(), "Duplicated vehicle ID - Changes not saved", 1000).show();
				    	return;
			    	}
		    	}
		    }
			
			if (originalVehicleID.length() > 0)
			{
				for (int idx=0; idx<_allSavedCars.size(); idx++)
			    {
			    	// save vehicle
			    	if ((_allSavedCars.get(idx)).VehicleID.equals(originalVehicleID))
			    	{
			    		Log.d("OVMS", String.format("Saved: %s", originalVehicleID));
			    		_allSavedCars.set(idx, editedCar);
				    	Toast.makeText(getBaseContext(), String.format("%s saved", editedCar.VehicleID), 1000).show();
			    		break;
			    	}
			    }
	    	} else
	    	{
	    		// add vehicle
	    		Log.d("OVMS", String.format("Added: %s", editedCar.VehicleID));
	    		_allSavedCars.add(editedCar);
		    	Toast.makeText(getBaseContext(), String.format("%s added", editedCar.VehicleID), 1000).show();
	    	}
	    }
	    else if (actionCode.equals("DELETE"))
	    {
			for (int idx=0; idx<_allSavedCars.size(); idx++)
		    {
		    	if ((_allSavedCars.get(idx)).VehicleID.equals(originalVehicleID))
		    	{
		    		Log.d("OVMS", String.format("Deleted: %s", originalVehicleID));
		    		_allSavedCars.remove(idx);
			    	Toast.makeText(getBaseContext(), String.format("%s deleted", editedCar.VehicleID), 1000).show();
		    		break;
		    	}
		    }
	    }
	    
	    //save cars to file
	    ((OVMSActivity)this.getParent()).saveCars();
	    
	    //refresh cars listview
	    //handler.sendEmptyMessage(0);
		CarData[] carArray = new CarData[_allSavedCars.size()];
		_allSavedCars.toArray(carArray);
		
		carsList = carArray;
		adapter = new ItemsAdapter(mContext, R.layout.tabcars_listitem,
				carsList);
		setListAdapter(adapter);
	}

	private class ItemsAdapter extends ArrayAdapter<CarData> {

		private CarData[] items;

		public ItemsAdapter(Context context, int textViewResourceId,
				CarData[] items) {
			super(context, textViewResourceId, items);
			this.items = items;
		}

		@Override
		public View getView(int position, View convertView, ViewGroup parent) {
			View v = convertView;
			if (v == null) {
				LayoutInflater vi = (LayoutInflater) getSystemService(Context.LAYOUT_INFLATER_SERVICE);
				v = vi.inflate(R.layout.tabcars_listitem, null);
			}

			CarData it = items[position];
			if (it != null) {
				ImageView iv = (ImageView) v
						.findViewById(R.id.imgCarListCarImage);
				int resId = getResources().getIdentifier(it.VehicleImageDrawable, "drawable", "com.openvehicles.OVMS");
				iv.setImageResource(resId);
				TextView tv = (TextView) v.findViewById(R.id.textCarListVehicleID);
				tv.setText(String.format("%s\n@%s", it.VehicleID, it.ServerNameOrIP));
				
				ImageButton btn = (ImageButton) v.findViewById(R.id.btnCatListEditCar);
				if (it.VehicleID.equals("DEMO"))
					btn.setVisibility(View.INVISIBLE);
				else {
					btn.setClickable(true);
					btn.setTag(it);
					btn.setOnClickListener(new OnClickListener() {

						public void onClick(View arg0) {
							CarData car = (CarData)arg0.getTag();
							Log.d("OVMS", "Editing car: " + car.VehicleID);
							editCar(car);
						}
					
					});
				}
			}
			
			return v;
		}
	}

	@Override
	protected void onListItemClick(ListView l, View v, int position, long id) {
		Log.d("OVMS", "Changing car to: " + carsList[position].VehicleID);
		carClicked(carsList[position]);
	}
	
	private Handler handler = new Handler() {
		public void handleMessage(Message msg) {
			adapter = new ItemsAdapter(mContext, R.layout.tabcars_listitem,
					carsList);
			setListAdapter(adapter);
		}
	};

	public void LoadCars(ArrayList<CarData> allSavedCars) {
		CarData[] carArray = new CarData[allSavedCars.size()];
		allSavedCars.toArray(carArray);
		
		_allSavedCars = allSavedCars;
		carsList = carArray;
		handler.sendEmptyMessage(0);

	}

}

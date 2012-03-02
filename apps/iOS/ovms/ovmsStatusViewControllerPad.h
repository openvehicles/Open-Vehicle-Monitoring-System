//
//  ovmsStatusViewControllerPad.h
//  ovms
//
//  Created by Mark Webb-Johnson on 10/12/11.
//  Copyright (c) 2011 Hong Hay Villa. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "ovmsLocationViewController.h"
#import "ovmsAppDelegate.h"
#import "ovmsControlPINEntry.h"

@interface ovmsStatusViewControllerPad : UIViewController <MKMapViewDelegate, ovmsStatusDelegate, ovmsCarDelegate, ovmsLocationDelegate, ovmsControlPINEntryDelegate, UIActionSheetDelegate>
@property (strong, nonatomic) IBOutlet UIImageView *m_car_connection_image;
@property (strong, nonatomic) IBOutlet UILabel *m_car_connection_state;
@property (strong, nonatomic) IBOutlet UIImageView *m_car_image;
@property (strong, nonatomic) IBOutlet UILabel *m_car_charge_state;
@property (strong, nonatomic) IBOutlet UILabel *m_car_charge_type;
@property (strong, nonatomic) IBOutlet UILabel *m_car_charge_mode;
@property (strong, nonatomic) IBOutlet UILabel *m_car_soc;
@property (strong, nonatomic) IBOutlet UIImageView *m_battery_front;
@property (strong, nonatomic) IBOutlet UIImageView *m_battery_charging;
@property (strong, nonatomic) IBOutlet UIImageView *m_car_outlineimage;
@property (strong, nonatomic) IBOutlet UIImageView *m_car_parking_image;
@property (strong, nonatomic) IBOutlet UILabel *m_car_parking_state;
@property (strong, nonatomic) IBOutlet UIImageView *m_charger_plug;
@property (strong, nonatomic) IBOutlet UIImageView *m_charger_button;
@property (strong, nonatomic) IBOutlet UILabel *m_car_range_ideal;
@property (strong, nonatomic) IBOutlet UILabel *m_car_range_estimated;
@property (strong, nonatomic) IBOutlet UILabel *m_car_charge_message;

@property (strong, nonatomic) IBOutlet UIImageView *m_car_lockunlock;
@property (strong, nonatomic) IBOutlet UIImageView *m_car_lights;
@property (strong, nonatomic) IBOutlet UIImageView *m_car_door_ld;
@property (strong, nonatomic) IBOutlet UIImageView *m_car_door_rd;
@property (strong, nonatomic) IBOutlet UIImageView *m_car_door_hd;
@property (strong, nonatomic) IBOutlet UIImageView *m_car_door_cp;
@property (strong, nonatomic) IBOutlet UIImageView *m_car_door_tr;
@property (strong, nonatomic) IBOutlet UILabel *m_car_wheel_fr_pressure;
@property (strong, nonatomic) IBOutlet UILabel *m_car_wheel_fr_temp;
@property (strong, nonatomic) IBOutlet UILabel *m_car_wheel_rr_pressure;
@property (strong, nonatomic) IBOutlet UILabel *m_car_wheel_rr_temp;
@property (strong, nonatomic) IBOutlet UILabel *m_car_wheel_fl_pressure;
@property (strong, nonatomic) IBOutlet UILabel *m_car_wheel_fl_temp;
@property (strong, nonatomic) IBOutlet UILabel *m_car_wheel_rl_pressure;
@property (strong, nonatomic) IBOutlet UILabel *m_car_wheel_rl_temp;
@property (strong, nonatomic) IBOutlet UILabel *m_car_temp_pem;
@property (strong, nonatomic) IBOutlet UILabel *m_car_temp_motor;
@property (strong, nonatomic) IBOutlet UILabel *m_car_temp_battery;
@property (strong, nonatomic) IBOutlet UILabel *m_car_temp_pem_l;
@property (strong, nonatomic) IBOutlet UILabel *m_car_temp_motor_l;
@property (strong, nonatomic) IBOutlet UILabel *m_car_temp_battery_l;
@property (strong, nonatomic) IBOutlet UILabel *m_car_ambient_temp;
@property (strong, nonatomic) IBOutlet UIImageView *m_car_valetonoff;

@property (strong, nonatomic) IBOutlet MKMapView *myMapView;
@property (nonatomic, retain) TeslaAnnotation *m_car_location;
@property (strong, nonatomic) IBOutlet UIBarButtonItem *m_control_button;
@property (strong, nonatomic) IBOutlet UISlider *m_charger_slider;
@property (strong, nonatomic) IBOutlet UIButton *m_battery_button;
@property (strong, nonatomic) IBOutlet UIButton *m_wakeup_button;
@property (strong, nonatomic) IBOutlet UIButton *m_lock_button;
@property (strong, nonatomic) IBOutlet UIButton *m_valet_button;

-(void)displayMYMap;
- (IBAction)ChargeSliderTouch:(id)sender;
- (IBAction)ChargeSliderValue:(id)sender;
- (IBAction)WakeupButton:(id)sender;

@end

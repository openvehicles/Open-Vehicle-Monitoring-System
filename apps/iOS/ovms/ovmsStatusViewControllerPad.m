//
//  ovmsStatusViewControllerPad.m
//  ovms
//
//  Created by Mark Webb-Johnson on 10/12/11.
//  Copyright (c) 2011 Hong Hay Villa. All rights reserved.
//

#import "UIKit/UIDevice.h"
#import "ovmsStatusViewControllerPad.h"

@implementation ovmsStatusViewControllerPad

@synthesize m_car_connection_image;
@synthesize m_car_connection_state;
@synthesize m_car_image;
@synthesize m_car_charge_state;
@synthesize m_car_charge_type;
@synthesize m_car_charge_mode;
@synthesize m_car_soc;
@synthesize m_battery_front;
@synthesize m_car_outlineimage;
@synthesize m_car_parking_image;
@synthesize m_car_parking_state;
@synthesize m_charger_plug;
@synthesize m_charger_button;
@synthesize m_car_range_ideal;
@synthesize m_car_range_estimated;
@synthesize m_car_charge_message;
@synthesize m_car_lights;
@synthesize m_battery_charging;

@synthesize m_car_lockunlock;
@synthesize m_car_door_ld;
@synthesize m_car_door_rd;
@synthesize m_car_door_hd;
@synthesize m_car_door_cp;
@synthesize m_car_door_tr;
@synthesize m_car_wheel_fr_pressure;
@synthesize m_car_wheel_fr_temp;
@synthesize m_car_wheel_rr_pressure;
@synthesize m_car_wheel_rr_temp;
@synthesize m_car_wheel_fl_pressure;
@synthesize m_car_wheel_fl_temp;
@synthesize m_car_wheel_rl_pressure;
@synthesize m_car_wheel_rl_temp;
@synthesize m_car_temp_pem;
@synthesize m_car_temp_motor;
@synthesize m_car_temp_battery;
@synthesize m_car_temp_pem_l;
@synthesize m_car_temp_motor_l;
@synthesize m_car_temp_battery_l;
@synthesize m_car_ambient_temp;
@synthesize m_car_valetonoff;

@synthesize myMapView;
@synthesize m_car_location;
@synthesize m_control_button;
@synthesize m_charger_slider;
@synthesize m_battery_button;
@synthesize m_wakeup_button;
@synthesize m_lock_button;
@synthesize m_valet_button;
@synthesize m_info_button;

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
        // Custom initialization
    }
    return self;
}

- (void)didReceiveMemoryWarning
{
    // Releases the view if it doesn't have a superview.
    [super didReceiveMemoryWarning];
    
    // Release any cached data, images, etc that aren't in use.
}

#pragma mark - View lifecycle

/*
// Implement loadView to create a view hierarchy programmatically, without using a nib.
- (void)loadView
{
}
*/

- (void)viewDidLoad
{
  [super viewDidLoad];

  UIImage *stetchLeftTrack= [[UIImage imageNamed:@"Nothing.png"]
                             stretchableImageWithLeftCapWidth:30.0 topCapHeight:0.0];
  UIImage *stetchRightTrack= [[UIImage imageNamed:@"Nothing.png"]
                              stretchableImageWithLeftCapWidth:30.0 topCapHeight:0.0];
  
  // this code to set the slider ball image
  [m_charger_slider setThumbImage: [UIImage imageNamed:@"charger_button.png"] forState:UIControlStateNormal];
  [m_charger_slider setMinimumTrackImage:stetchLeftTrack forState:UIControlStateNormal];
  [m_charger_slider setMaximumTrackImage:stetchRightTrack forState:UIControlStateNormal];

  [ovmsAppDelegate myRef].status_delegate = self;
  [ovmsAppDelegate myRef].location_delegate = self;
  [ovmsAppDelegate myRef].car_delegate = self;

  self.navigationItem.title = [ovmsAppDelegate myRef].sel_label;
  
  [self updateStatus];
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  self.navigationItem.title = [ovmsAppDelegate myRef].sel_label;
  
  CGRect bvframe = m_car_temp_pem_l.frame;
  
  UIDeviceOrientation orientation = [[UIDevice currentDevice] orientation];
  
  if (orientation == UIInterfaceOrientationLandscapeLeft ||
      orientation == UIInterfaceOrientationLandscapeRight)
    {
    myMapView.frame = CGRectMake(20, bvframe.origin.y+bvframe.size.height+44,
                                 984, 635-(bvframe.origin.y+bvframe.size.height+44));
    }
  else
    {
    myMapView.frame = CGRectMake(20, 417, 728, 474);
    }

  [self updateStatus];
  [self displayMYMap];
  [self updateCar];
}

- (void)viewDidUnload
{
    [self setM_car_connection_image:nil];
    [self setM_car_connection_state:nil];
    [self setM_car_image:nil];
    [self setM_car_charge_state:nil];
    [self setM_car_charge_type:nil];
    [self setM_car_soc:nil];
    [self setM_battery_front:nil];
    [self setM_car_lockunlock:nil];
    [self setM_car_door_ld:nil];
    [self setM_car_door_rd:nil];
    [self setM_car_door_hd:nil];
    [self setM_car_door_cp:nil];
    [self setM_car_door_tr:nil];
    [self setM_car_wheel_fr_pressure:nil];
    [self setM_car_wheel_fr_temp:nil];
    [self setM_car_wheel_rr_pressure:nil];
    [self setM_car_wheel_rr_temp:nil];
    [self setM_car_wheel_fl_pressure:nil];
    [self setM_car_wheel_fl_temp:nil];
    [self setM_car_wheel_rl_pressure:nil];
    [self setM_car_wheel_rl_temp:nil];
    [self setM_car_temp_pem:nil];
    [self setM_car_temp_motor:nil];
    [self setM_car_temp_battery:nil];
    [self setM_car_temp_pem_l:nil];
    [self setM_car_temp_motor_l:nil];
    [self setM_car_temp_battery_l:nil];

    [self setMyMapView:nil];
    [self setM_car_outlineimage:nil];
    [self setM_car_parking_image:nil];
    [self setM_car_parking_state:nil];
  
    [self setM_car_ambient_temp:nil];
    [self setM_car_charge_mode:nil];
    [self setM_battery_charging:nil];
  [self setM_charger_plug:nil];
  [self setM_charger_button:nil];
  [self setM_car_range_estimated:nil];
  [self setM_car_range_ideal:nil];
  [self setM_car_valetonoff:nil];
    [self setM_car_lights:nil];
    [self setM_car_charge_message:nil];
    [self setM_control_button:nil];
    [self setM_charger_slider:nil];
    [self setM_battery_button:nil];
    [self setM_wakeup_button:nil];
    [self setM_lock_button:nil];
    [self setM_valet_button:nil];
    [self setM_info_button:nil];
    [super viewDidUnload];
    // Release any retained subviews of the main view.
    // e.g. self.myOutlet = nil;
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
    // Return YES for supported orientations
	return YES;
}

- (void)willAnimateRotationToInterfaceOrientation:(UIInterfaceOrientation)toInterfaceOrientation duration:(NSTimeInterval)duration
{
  CGRect bvframe = m_car_temp_pem_l.frame;
  
  if (toInterfaceOrientation == UIInterfaceOrientationLandscapeLeft ||
      toInterfaceOrientation == UIInterfaceOrientationLandscapeRight)
    {
    myMapView.frame = CGRectMake(20, bvframe.origin.y+bvframe.size.height+44,
                                 984, 635-(bvframe.origin.y+bvframe.size.height+44));
    }
  else
    {
    myMapView.frame = CGRectMake(20, 417, 728, 474);
    }
}

-(void) updateStatus
{
  NSString* units;
  if ([[ovmsAppDelegate myRef].car_units isEqualToString:@"K"])
    units = @"km";
  else
    units = @"m";
  
  int connected = [ovmsAppDelegate myRef].car_connected;
  time_t lastupdated = [ovmsAppDelegate myRef].car_lastupdated;
  int seconds = (time(0)-lastupdated);
  int minutes = (time(0)-lastupdated)/60;
  int hours = minutes/60;
  int days = minutes/(60*24);
  
  NSString* c_good;
  NSString* c_bad;
  NSString* c_unknown;
  if ([ovmsAppDelegate myRef].car_paranoid)
    {
    c_good = [[NSString alloc] initWithString:@"connection_good_paranoid.png"];
    c_bad = [[NSString alloc] initWithString:@"connection_bad_paranoid.png"];
    c_unknown = [[NSString alloc] initWithString:@"connection_unknown_paranoid.png"];
    }
  else
    {
    c_good = [[NSString alloc] initWithString:@"connection_good.png"];
    c_bad = [[NSString alloc] initWithString:@"connection_bad.png"];
    c_unknown = [[NSString alloc] initWithString:@"connection_unknown.png"];
    }
  
  NSString* imagewanted;
  
  if (connected>0)
    {
    imagewanted = c_good;
    }
  else
    {
    imagewanted = c_unknown;
    }
  
  if (lastupdated == 0)
    {
    m_car_connection_state.text = @"";
    m_car_connection_state.textColor = [UIColor whiteColor];
    }
  else if (minutes == 0)
    {
    m_car_connection_state.text = @"live";
    m_car_connection_state.textColor = [UIColor whiteColor];
    }
  else if (minutes == 1)
    {
    m_car_connection_state.text = @"1 min";
    m_car_connection_state.textColor = [UIColor whiteColor];
    }
  else if (days > 1)
    {
    m_car_connection_state.text = [NSString stringWithFormat:@"%d days",days];
    m_car_connection_state.textColor = [UIColor redColor];
    imagewanted = c_bad;
    }
  else if (hours > 1)
    {
    m_car_connection_state.text = [NSString stringWithFormat:@"%d hours",hours];
    m_car_connection_state.textColor = [UIColor redColor];
    imagewanted = c_bad;
    }
  else if (minutes >= 20)
    {
    m_car_connection_state.text = [NSString stringWithFormat:@"%d mins",minutes];
    m_car_connection_state.textColor = [UIColor redColor];
    imagewanted = c_bad;
    }
  else
    {
    m_car_connection_state.text = [NSString stringWithFormat:@"%d mins",minutes];
    m_car_connection_state.textColor = [UIColor whiteColor];
    }
  
  if ([ovmsAppDelegate myRef].car_online)
    {
    self.navigationItem.rightBarButtonItem = m_control_button;
    m_control_button.enabled=YES;
    m_battery_button.enabled=YES;
    m_info_button.hidden=NO;
    [m_car_connection_image stopAnimating];
    m_car_connection_image.animationImages = nil;
    m_car_connection_image.image=[UIImage imageNamed:imagewanted];
    }
  else
    {
    self.navigationItem.rightBarButtonItem = nil;
    m_control_button.enabled=NO;
    m_battery_button.enabled=NO;
    m_info_button.hidden=YES;
    NSArray *images = [[NSArray alloc] initWithObjects:
                       [UIImage imageNamed:@"Nothing.png"],
                       [UIImage imageNamed:imagewanted],
                       nil];
    m_car_connection_image.image = nil;
    m_car_connection_image.animationImages = images;
    m_car_connection_image.animationDuration = 1.0;
    m_car_connection_image.animationRepeatCount = 0;
    [m_car_connection_image startAnimating];
    }
  
  int parktime = [ovmsAppDelegate myRef].car_parktime;
  if ((parktime > 0)&&(lastupdated>0)) parktime += seconds;
  
  if (parktime == 0)
    {
    m_car_parking_image.hidden = 1;
    m_car_parking_state.text = @"";
    }
  else if (parktime < 120)
    {
    m_car_parking_image.hidden = 0;
    m_car_parking_state.text = @"just now";
    }
  else if (parktime < (3600*2))
    {
    m_car_parking_image.hidden = 0;
    m_car_parking_state.text = [NSString stringWithFormat:@"%d mins",parktime/60];
    }
  else if (parktime < (3600*24))
    {
    m_car_parking_image.hidden = 0;
    m_car_parking_state.text = [NSString stringWithFormat:@"%02d:%02d",
                                parktime/3600,
                                (parktime%3600)/60];
    }
  else
    {
    m_car_parking_image.hidden = 0;
    m_car_parking_state.text = [NSString stringWithFormat:@"%d hours",parktime/3600];
    }
  
  m_car_image.image=[UIImage imageNamed:[ovmsAppDelegate myRef].sel_imagepath];
  m_car_soc.text = [NSString stringWithFormat:@"%d%%",[ovmsAppDelegate myRef].car_soc];
  m_car_range_ideal.text = [NSString stringWithFormat:@"%d%s",
                            [ovmsAppDelegate myRef].car_idealrange,
                            [units UTF8String]];
  m_car_range_estimated.text = [NSString stringWithFormat:@"%d%s",
                                [ovmsAppDelegate myRef].car_estimatedrange,
                                [units UTF8String]];
  
  CGRect bounds = m_battery_front.bounds;
  CGPoint center = m_battery_front.center;
  CGFloat oldwidth = bounds.size.width;
  CGFloat newwidth = (((0.0+[ovmsAppDelegate myRef].car_soc)/100.0)*(233-17))+17;
  bounds.size.width = newwidth;
  center.x = center.x + ((newwidth - oldwidth)/2);
  m_battery_front.bounds = bounds;
  m_battery_front.center = center;
  bounds = m_battery_front.bounds;
  
  if ((([ovmsAppDelegate myRef].car_doors1 & 0x04)==0)||
      ([ovmsAppDelegate myRef].car_chargesubstate == 0x07))
    { // Charge port is closed, or connect-pwr-cable charge sub-state
      m_charger_plug.hidden = 1;            // The plug image
      m_charger_slider.hidden = 1;          // The slider control on the plug
      m_charger_slider.enabled = 0;         // The slider control on the plug
      m_car_charge_state.hidden = 1;        // The car charge state label (left of slider)
      m_car_charge_type.hidden = 1;         // The car charge type label (left of slider)
      m_car_charge_message.hidden = 1;      // The car charge message (right of slider)
      m_battery_charging.hidden = 1;        // Copper tops on the battery
      m_car_charge_mode.hidden = 1;         // The car charge mode message (copper on battery)
    }
  else
    { // Charge port is open and plugged in
      m_charger_plug.hidden = 0;            // The plug image
      m_charger_slider.hidden = 0;          // The slider control on the plug
      m_charger_slider.enabled =            // The slider control on the plug
      connected &&
      ([ovmsAppDelegate myRef].car_chargestateN<0x100) &&
      ([ovmsAppDelegate myRef].car_online);
      m_car_charge_state.hidden = 0;        // The car charge state label (left of slider)
      m_car_charge_type.hidden = 0;         // The car charge type label (left of slider)
      switch ([ovmsAppDelegate myRef].car_chargestateN)
      {
        case 0x04:    // Done
        case 0x115:   // Stopping
        case 0x15:    // Stopped
        case 0x16:    // Stopped
        case 0x17:    // Stopped
        case 0x18:    // Stopped
        case 0x19:    // Stopped
        m_car_charge_message.text = @"SLIDE TO CHARGE";
        m_car_charge_state.text = @"";
        m_car_charge_type.text = @"";
        m_car_charge_mode.text = @"";
        // Slider on the left, message is "Slide to charge"
        m_charger_slider.value = 0.0;
        m_car_charge_message.hidden = 0;
        m_car_charge_state.hidden = 1;
        m_car_charge_type.hidden = 1;
        m_battery_charging.hidden = 1;
        m_car_charge_mode.hidden = 1;
        break;
        
        case 0x01:    // Charging
        case 0x101:   // Starting
        case 0x02:    // Top-off
        case 0x0d:    // Preparing to charge
        case 0x0f:    // Heating
        m_car_charge_state.text = [[ovmsAppDelegate myRef].car_chargestate uppercaseString];
        m_car_charge_type.text = [NSString stringWithFormat:@"%dV @%dA",
                                  [ovmsAppDelegate myRef].car_linevoltage,
                                  [ovmsAppDelegate myRef].car_chargecurrent];
        m_car_charge_mode.text = [NSString stringWithFormat:@"%@ %dA",
                                  [[ovmsAppDelegate myRef].car_chargemode uppercaseString],
                                  [ovmsAppDelegate myRef].car_chargelimit];
        // Slider on the right, message blank
        m_charger_slider.value = 1.0;        
        m_car_charge_message.hidden = 1;
        m_car_charge_state.hidden = 0;
        m_car_charge_type.hidden = 0;
        m_battery_charging.hidden = 0;
        m_car_charge_mode.hidden = 0;
        break;
        
        default:
        m_car_charge_state.text = @"";
        m_car_charge_type.text = @"";
        m_car_charge_mode.text = @"";
        // Slider on the right, message blank
        m_charger_slider.value = 1.0;
        m_car_charge_message.hidden = 1;
        m_car_charge_state.hidden = 0;
        m_car_charge_type.hidden = 0;
        m_battery_charging.hidden = 1;
        m_car_charge_mode.hidden = 1;
        break;
      }
    }
}

-(void) updateCar
{
  if ([ovmsAppDelegate myRef].car_online)
    {
    m_lock_button.enabled=YES;
    m_valet_button.enabled=YES;
    m_wakeup_button.enabled=YES;
    }
  else
    {
    m_lock_button.enabled=NO;
    m_valet_button.enabled=NO;
    m_wakeup_button.enabled=NO;    
    }
  
  m_car_outlineimage.image=[UIImage
                            imageNamed:[NSString stringWithFormat:@"ol_%@",
                                        [ovmsAppDelegate myRef].sel_imagepath]];
  
  if ([ovmsAppDelegate myRef].car_doors2 & 0x08)
    m_car_lockunlock.image = [UIImage imageNamed:@"carlock.png"];
  else
    m_car_lockunlock.image = [UIImage imageNamed:@"carunlock.png"];
  
  if ([ovmsAppDelegate myRef].car_doors2 & 0x10)
    m_car_valetonoff.image = [UIImage imageNamed:@"carvaleton.png"];
  else
    m_car_valetonoff.image = [UIImage imageNamed:@"carvaletoff.png"];
  
  if ([ovmsAppDelegate myRef].car_doors2 & 0x20)
    m_car_lights.hidden = 0;
  else
    m_car_lights.hidden = 1;
  
  if ([ovmsAppDelegate myRef].car_doors1 & 0x01)
    m_car_door_ld.hidden = 0;
  else
    m_car_door_ld.hidden = 1;
  
  if ([ovmsAppDelegate myRef].car_doors1 & 0x02)
    m_car_door_rd.hidden = 0;
  else
    m_car_door_rd.hidden = 1;
  
  if ([ovmsAppDelegate myRef].car_doors1 & 0x04)
    m_car_door_cp.hidden = 0;
  else
    m_car_door_cp.hidden = 1;
  
  if ([ovmsAppDelegate myRef].car_doors2 & 0x40)
    m_car_door_hd.hidden = 0;
  else
    m_car_door_hd.hidden = 1;
  
  if ([ovmsAppDelegate myRef].car_doors2 & 0x80)
    m_car_door_tr.hidden = 0;
  else
    m_car_door_tr.hidden = 1;
  
  if ([ovmsAppDelegate myRef].car_tpem > 0)
    {
    m_car_temp_pem_l.hidden = 0;
    m_car_temp_pem.text = [NSString stringWithFormat:@"%dºC",
                           [ovmsAppDelegate myRef].car_tpem];
    }
  else
    {
    m_car_temp_pem_l.hidden = 1;
    m_car_temp_pem.text = @"";
    }
  
  if ([ovmsAppDelegate myRef].car_tmotor > 0)
    {
    m_car_temp_motor_l.hidden = 0;
    m_car_temp_motor.text = [NSString stringWithFormat:@"%dºC",
                             [ovmsAppDelegate myRef].car_tmotor];
    }
  else
    {
    m_car_temp_motor_l.hidden = 1;
    m_car_temp_motor.text = @"";
    }
  
  if ([ovmsAppDelegate myRef].car_tbattery > 0)
    {
    m_car_temp_battery_l.hidden = 0;
    m_car_temp_battery.text = [NSString stringWithFormat:@"%dºC",
                               [ovmsAppDelegate myRef].car_tbattery];
    }
  else
    {
    m_car_temp_battery_l.hidden = 1;
    m_car_temp_battery.text = @"";
    }
  
  if ([ovmsAppDelegate myRef].car_ambient_temp > -127)
    {
    m_car_ambient_temp.text = [NSString stringWithFormat:@"%dºC",
                               [ovmsAppDelegate myRef].car_ambient_temp];
    }
  else
    {
    m_car_ambient_temp.text = @"";
    }
  
  if ([ovmsAppDelegate myRef].car_tpms_fr_temp > 0)
    {
    m_car_wheel_fr_pressure.text = [NSString stringWithFormat:@"%0.1f PSI",
                                    [ovmsAppDelegate myRef].car_tpms_fr_pressure];
    m_car_wheel_fr_temp.text = [NSString stringWithFormat:@"%dºC",
                                [ovmsAppDelegate myRef].car_tpms_fr_temp];
    }
  else
    {
    m_car_wheel_fr_pressure.text = @"";
    m_car_wheel_fr_temp.text = @"";
    }
  
  if ([ovmsAppDelegate myRef].car_tpms_rr_temp > 0)
    {
    m_car_wheel_rr_pressure.text = [NSString stringWithFormat:@"%0.1f PSI",
                                    [ovmsAppDelegate myRef].car_tpms_rr_pressure];
    m_car_wheel_rr_temp.text = [NSString stringWithFormat:@"%dºC",
                                [ovmsAppDelegate myRef].car_tpms_rr_temp];
    }
  else
    {
    m_car_wheel_rr_pressure.text = @"";
    m_car_wheel_rr_temp.text = @"";
    }
  
  if ([ovmsAppDelegate myRef].car_tpms_fl_temp > 0)
    {
    m_car_wheel_fl_pressure.text = [NSString stringWithFormat:@"%0.1f PSI",
                                    [ovmsAppDelegate myRef].car_tpms_fl_pressure];
    m_car_wheel_fl_temp.text = [NSString stringWithFormat:@"%dºC",
                                [ovmsAppDelegate myRef].car_tpms_fl_temp];
    }
  else
    {
    m_car_wheel_fl_pressure.text = @"";
    m_car_wheel_fl_temp.text = @"";
    }
  
  if ([ovmsAppDelegate myRef].car_tpms_rl_temp > 0)
    {
    m_car_wheel_rl_pressure.text = [NSString stringWithFormat:@"%0.1f PSI",
                                    [ovmsAppDelegate myRef].car_tpms_rl_pressure];
    m_car_wheel_rl_temp.text = [NSString stringWithFormat:@"%dºC",
                                [ovmsAppDelegate myRef].car_tpms_rl_temp];
    }
  else
    {
    m_car_wheel_rl_pressure.text = @"";
    m_car_wheel_rl_temp.text = @"";
    }
}

-(void) updateLocation
{
  CLLocationCoordinate2D location = [ovmsAppDelegate myRef].car_location;
  
  MKCoordinateRegion region = myMapView.region;
  if ( (region.center.latitude != location.latitude)&&
      (region.center.longitude != location.longitude) )
    {
    if (self.m_car_location)
      {
      [self.m_car_location setCoordinate: location];
      }
    else
      {
      // Remove all existing annotations
      for (int k=0; k < [myMapView.annotations count]; k++)
        { 
          if ([[myMapView.annotations objectAtIndex:k] isKindOfClass:[TeslaAnnotation class]])
            {
            [myMapView removeAnnotation:[myMapView.annotations objectAtIndex:k]];
            }
        }
    
      TeslaAnnotation *pa = [[TeslaAnnotation alloc] initWithCoordinate:location];
      pa.name = @"EV915";
      pa.description = [NSString stringWithFormat:@"%f, %f", pa.coordinate.latitude, pa.coordinate.longitude];
      [myMapView addAnnotation:pa];
      self.m_car_location = pa;
      }
    
    region.center=location;
    [myMapView setRegion:region animated:YES];
    }
}

-(void)displayMYMap
{
  MKCoordinateRegion region; 
  MKCoordinateSpan span; 
  span.latitudeDelta=0.01; 
  span.longitudeDelta=0.01; 
  
  CLLocationCoordinate2D location = [ovmsAppDelegate myRef].car_location;
  
  region.span=span; 
  region.center=location; 
  
  // Remove all existing annotations
  for (int k=0; k < [myMapView.annotations count]; k++)
    { 
      if ([[myMapView.annotations objectAtIndex:k] isKindOfClass:[TeslaAnnotation class]])
        {
        [myMapView removeAnnotation:[myMapView.annotations objectAtIndex:k]];
        }
    }
  
  if ((location.latitude != 0)||(location.longitude != 0))
    {
    // Add in the new annotation for current car location
    TeslaAnnotation *pa = [[TeslaAnnotation alloc] initWithCoordinate:location];
    pa.name = @"EV915";
    pa.description = [NSString stringWithFormat:@"%f, %f", pa.coordinate.latitude, pa.coordinate.longitude];
    [myMapView addAnnotation:pa];
    self.m_car_location = pa;
    }
  
  [myMapView setRegion:region animated:YES]; 
  [myMapView regionThatFits:region]; 
}

- (IBAction)ChargeSliderTouch:(id)sender {
  if (([[ovmsAppDelegate myRef].car_chargestate isEqualToString:@"done"])||
      ([[ovmsAppDelegate myRef].car_chargestate isEqualToString:@"stopped"]))
    {
    // The slider is on the left, and should spring back there
    if (m_charger_slider.value == 1.0)
      {
      // We are done, and should start the charge
      [[ovmsAppDelegate myRef] commandDoStartCharge];
      }
    else
      {
      // Spring back
      [UIView beginAnimations: @"SlideCanceled" context: nil];
      [UIView setAnimationDelegate: self];
      [UIView setAnimationDuration: 0.35];
      // use CurveEaseOut to create "spring" effect
      [UIView setAnimationCurve: UIViewAnimationCurveEaseOut]; 
      m_charger_slider.value = 0.0;      
      [UIView commitAnimations];
      }
    }
  else
    {
    // The slider is on the right, and should sprint back there
    if (m_charger_slider.value == 0.0)
      {
      // We are done, and should stop the charge
      [[ovmsAppDelegate myRef] commandDoStopCharge];
      }
    else
      {
      // Spring back
      [UIView beginAnimations: @"SlideCanceled" context: nil];
      [UIView setAnimationDelegate: self];
      [UIView setAnimationDuration: 0.35];
      // use CurveEaseOut to create "spring" effect
      [UIView setAnimationCurve: UIViewAnimationCurveEaseOut]; 
      m_charger_slider.value = 1.0;      
      [UIView commitAnimations];
      }
    }
}

- (IBAction)ChargeSliderValue:(id)sender {
}

- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender
{
  if ([[segue identifier] isEqualToString:@"ValetMode"])
    {
    if ([ovmsAppDelegate myRef].car_doors2 & 0x10)
      { // Valet is ON, let's offer to deactivate it
        [[segue destinationViewController] setInstructions:@"Enter PIN\nto deactivate valet mode"];
        [[segue destinationViewController] setHeading:@"Valet Mode"];
        [[segue destinationViewController] setFunction:@"Valet Off"];
        [[segue destinationViewController] setDelegate:self];
      }
    else
      { // Valet is OFF, let's offer to activate it
        [[segue destinationViewController] setInstructions:@"Enter PIN\nto activate valet mode"];
        [[segue destinationViewController] setHeading:@"Valet Mode"];
        [[segue destinationViewController] setFunction:@"Valet On"];
        [[segue destinationViewController] setDelegate:self];
      }
    }
  else if ([[segue identifier] isEqualToString:@"LockUnlock"])
    {
    if ([ovmsAppDelegate myRef].car_doors2 & 0x08)
      { // Car is locked, let's offer to unlock it
        [[segue destinationViewController] setInstructions:@"Enter PIN\nto unlock car"];
        [[segue destinationViewController] setHeading:@"Unlock Car"];
        [[segue destinationViewController] setFunction:@"Unlock Car"];
        [[segue destinationViewController] setDelegate:self];
      }
    else
      { // Car is unlocked, let's offer to lock it
        [[segue destinationViewController] setInstructions:@"Enter PIN\nto lock car"];
        [[segue destinationViewController] setHeading:@"Lock Car"];
        [[segue destinationViewController] setFunction:@"Lock Car"];
        [[segue destinationViewController] setDelegate:self];
      }
    }
}

- (void)omvsControlPINEntryDelegateDidCancel:(NSString*)fn
{
}

- (void)omvsControlPINEntryDelegateDidSave:(NSString*)fn pin:(NSString*)pin
{
  if ([fn isEqualToString:@"Valet On"])
    {
    [[ovmsAppDelegate myRef] commandDoActivateValet:pin];
    }
  else if ([fn isEqualToString:@"Valet Off"])
    {
    [[ovmsAppDelegate myRef] commandDoDeactivateValet:pin];
    }    
  else if ([fn isEqualToString:@"Lock Car"])
    {
    [[ovmsAppDelegate myRef] commandDoLockCar:pin];
    }
  else if ([fn isEqualToString:@"Unlock Car"])
    {
    [[ovmsAppDelegate myRef] commandDoUnlockCar:pin];
    }    
}

- (IBAction)WakeupButton:(id)sender
{
  // The wakeup button has been pressed - let's wakeup the car
  UIActionSheet *actionSheet = [[UIActionSheet alloc] initWithTitle:@"Wakeup Car"
                                                           delegate:self
                                                  cancelButtonTitle:@"Cancel"
                                             destructiveButtonTitle:nil
                                                  otherButtonTitles:@"Wakeup",nil];
  [actionSheet showInView:self.view];
}

- (void)actionSheet:(UIActionSheet *)sender clickedButtonAtIndex:(int)index
{
  if (index == [sender firstOtherButtonIndex])
    {
    [[ovmsAppDelegate myRef] commandDoWakeupCar];
    }
}

- (MKAnnotationView *)mapView:(MKMapView *)mapView
            viewForAnnotation:(id <MKAnnotation>)annotation
{
  static NSString *teslaAnnotationIdentifier=@"TeslaAnnotationIdentifier";
  
  if ([annotation isKindOfClass:[MKUserLocation class]])
    return nil;
  
  if([annotation isKindOfClass:[TeslaAnnotation class]]){
    //Try to get an unused annotation, similar to uitableviewcells
    //    MKAnnotationView *annotationView=[mapView dequeueReusableAnnotationViewWithIdentifier:teslaAnnotationIdentifier];
    //If one isn't available, create a new one
    //    if(!annotationView){
    MKAnnotationView *annotationView=[[MKAnnotationView alloc] initWithAnnotation:annotation reuseIdentifier:teslaAnnotationIdentifier];
    //Here's where the magic happens
    annotationView.image=[UIImage imageNamed:[ovmsAppDelegate myRef].sel_imagepath];
    annotationView.contentMode = UIViewContentModeScaleAspectFill;
    annotationView.bounds = CGRectMake(0, 0, 32, 32);
    //    }
    return annotationView;
  }
  return nil;
}

@end

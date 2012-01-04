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
@synthesize m_car_soc;
@synthesize m_battery_front;
@synthesize m_car_range;
@synthesize m_car_outlineimage;
@synthesize m_car_parking_image;
@synthesize m_car_parking_state;

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

@synthesize myMapView;
@synthesize m_car_location;

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
    [self setM_car_range:nil];
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
  
  if (connected>0)
    {
    m_car_connection_image.image=[UIImage imageNamed:c_good];
    }
  else
    {
    m_car_connection_image.image=[UIImage imageNamed:c_unknown];
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
    m_car_connection_image.image=[UIImage imageNamed:c_bad];
    }
  else if (hours > 1)
    {
    m_car_connection_state.text = [NSString stringWithFormat:@"%d hours",hours];
    m_car_connection_state.textColor = [UIColor redColor];
    m_car_connection_image.image=[UIImage imageNamed:c_bad];
    }
  else if (minutes >= 20)
    {
    m_car_connection_state.text = [NSString stringWithFormat:@"%d mins",minutes];
    m_car_connection_state.textColor = [UIColor redColor];
    m_car_connection_image.image=[UIImage imageNamed:c_bad];
    }
  else
    {
    m_car_connection_state.text = [NSString stringWithFormat:@"%d mins",minutes];
    m_car_connection_state.textColor = [UIColor whiteColor];
    }
  
  int parktime = [ovmsAppDelegate myRef].car_parktime;
  if (parktime > 0) parktime += seconds;
  
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
  m_car_range.text = [NSString stringWithFormat:@"Range: %d%s (%d%s estimated)",
                      [ovmsAppDelegate myRef].car_idealrange,
                      [units UTF8String],
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
  if ([[ovmsAppDelegate myRef].car_chargestate isEqualToString:@"charging"])
    {
    m_car_charge_state.text = [NSString stringWithFormat:@"Charging (%@)", [ovmsAppDelegate myRef].car_chargemode];
    m_car_charge_type.text = [NSString stringWithFormat:@"%dV @%dA",
                              [ovmsAppDelegate myRef].car_linevoltage,
                              [ovmsAppDelegate myRef].car_chargecurrent];
    }
  else if ([[ovmsAppDelegate myRef].car_chargestate isEqualToString:@"topoff"])
    {
    m_car_charge_state.text = @"Topping off";
    m_car_charge_type.text = [NSString stringWithFormat:@"%dV @%dA",
                              [ovmsAppDelegate myRef].car_linevoltage,
                              [ovmsAppDelegate myRef].car_chargecurrent];
    }
  else if ([[ovmsAppDelegate myRef].car_chargestate isEqualToString:@"done"])
    {
    m_car_charge_state.text = @"";
    m_car_charge_type.text = @"";
    }
  else if ([[ovmsAppDelegate myRef].car_chargestate isEqualToString:@"stopped"])
    {
    m_car_charge_state.text = @"Charging stopped";
    m_car_charge_type.text = @"";
    }
  else
    {
    m_car_charge_state.text = @"";
    m_car_charge_type.text = @"";
    }
}

-(void) updateCar
{
  m_car_outlineimage.image=[UIImage
                            imageNamed:[NSString stringWithFormat:@"ol_%@",
                                        [ovmsAppDelegate myRef].sel_imagepath]];

  if ([ovmsAppDelegate myRef].car_doors2 & 0x08)
    m_car_lockunlock.image = [UIImage imageNamed:@"carlock.png"];
  else
    m_car_lockunlock.image = [UIImage imageNamed:@"carunlock.png"];
  
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
  
  if ([ovmsAppDelegate myRef].car_tpms_fr_temp > 0)
    {
    m_car_wheel_fr_pressure.text = [NSString stringWithFormat:@"%0.1fpsi",
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
    m_car_wheel_rr_pressure.text = [NSString stringWithFormat:@"%0.1fpsi",
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
    m_car_wheel_fl_pressure.text = [NSString stringWithFormat:@"%0.1fpsi",
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
    m_car_wheel_rl_pressure.text = [NSString stringWithFormat:@"%0.1fpsi",
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

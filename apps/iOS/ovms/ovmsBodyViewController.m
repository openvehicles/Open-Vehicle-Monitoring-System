//
//  ovmsBodyViewController.m
//  ovms
//
//  Created by Mark Webb-Johnson on 9/12/11.
//  Copyright (c) 2011 Hong Hay Villa. All rights reserved.
//

#import "ovmsBodyViewController.h"

@implementation ovmsBodyViewController
@synthesize m_car_lockunlock;
@synthesize m_car_outlineimage;
@synthesize m_car_valetonoff;
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

// Implement viewDidLoad to do additional setup after loading the view, typically from a nib.
- (void)viewDidLoad
{
  [super viewDidLoad];
  [ovmsAppDelegate myRef].car_delegate = self;
  
  self.navigationItem.title = [ovmsAppDelegate myRef].sel_label;
}

- (void)viewDidUnload
{
  [self setM_car_lockunlock:nil];
  [self setM_car_door_ld:nil];
  [self setM_car_door_rd:nil];
  [self setM_car_door_hd:nil];
  [self setM_car_door_cp:nil];
  [self setM_car_door_tr:nil];
  [self setM_car_temp_pem:nil];
  [self setM_car_temp_motor:nil];
  [self setM_car_temp_battery:nil];
  [self setM_car_wheel_fr_pressure:nil];
  [self setM_car_wheel_fr_temp:nil];
  [self setM_car_wheel_rr_pressure:nil];
  [self setM_car_wheel_rr_temp:nil];
  [self setM_car_wheel_fl_pressure:nil];
  [self setM_car_wheel_fl_temp:nil];
  [self setM_car_wheel_rl_pressure:nil];
  [self setM_car_wheel_rl_temp:nil];
  [self setM_car_temp_pem_l:nil];
  [self setM_car_temp_motor_l:nil];
  [self setM_car_temp_battery_l:nil];
  [self setM_car_outlineimage:nil];
  [self setM_car_ambient_temp:nil];
    [self setM_car_valetonoff:nil];
  [super viewDidUnload];
  // Release any retained subviews of the main view.
  // e.g. self.myOutlet = nil;
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  self.navigationItem.title = [ovmsAppDelegate myRef].sel_label;
  [self updateCar];
}

- (void)viewDidAppear:(BOOL)animated
{
  [super viewDidAppear:animated];
}

- (void)viewWillDisappear:(BOOL)animated
{
	[super viewWillDisappear:animated];
}

- (void)viewDidDisappear:(BOOL)animated
{
	[super viewDidDisappear:animated];
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
  // Return YES for supported orientations
  if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPhone)
    {
    return (interfaceOrientation == UIInterfaceOrientationPortrait);
    }
  else
    {
    return YES;
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

  if ([ovmsAppDelegate myRef].car_doors2 & 0x10)
    m_car_valetonoff.image = [UIImage imageNamed:@"carvaleton.png"];
  else
    m_car_valetonoff.image = [UIImage imageNamed:@"carvaletoff.png"];

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

@end

//
//  ovmsControlViewController.m
//  Open Vehicle
//
//  Created by Mark Webb-Johnson on 2/2/12.
//  Copyright (c) 2012 Open Vehicle Systems. All rights reserved.
//

#import "ovmsControlViewController.h"
#import "ovmsControlPINEntry.h"
#import "JHNotificationManager.h"

@implementation ovmsControlViewController
@synthesize m_chargemode;

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
//  self.navigationItem.title = [ovmsAppDelegate myRef].sel_label;
}

- (void)viewDidUnload
{
  [self setM_chargemode:nil];
    [super viewDidUnload];
    // Release any retained subviews of the main view.
    // e.g. self.myOutlet = nil;
}

-(void) viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
}

-(void) viewWillDisappear:(BOOL)animated
{
  [super viewWillDisappear:animated];
  
  [[ovmsAppDelegate myRef] commandCancel];
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
    // Return YES for supported orientations
    return (interfaceOrientation == UIInterfaceOrientationPortrait);
}

- (IBAction)doneButton:(id)sender {
  [self dismissModalViewControllerAnimated:YES];
}

- (IBAction)startChargingButton:(id)sender {
  [JHNotificationManager notificationWithMessage:@"Starting Charge"];
  NSString* cmd = @"11";
  [[ovmsAppDelegate myRef] commandRegister:cmd callback:self];
}

- (IBAction)stopChargingButton:(id)sender {
  [JHNotificationManager notificationWithMessage:@"Stopping Charge"];
  NSString* cmd = @"12";
  [[ovmsAppDelegate myRef] commandRegister:cmd callback:self];
}

- (IBAction)chargeModeButton:(id)sender {
}

- (IBAction)lockButton:(id)sender {
}

- (IBAction)valetButton:(id)sender {
}

- (IBAction)featuresButton:(id)sender {
}

- (IBAction)parametersButton:(id)sender {
}

- (IBAction)cellularUsageButton:(id)sender {
}

- (IBAction)resetModuleButton:(id)sender {
}

- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender
{
  if ([[segue identifier] isEqualToString:@"ValetMode"])
    {
    if ([ovmsAppDelegate myRef].car_doors2 & 0x10)
      { // Valet is ON, let's offer to deactivate it
      [[segue destinationViewController] setInstructions:@"Enter PIN to deactivate valet mode"];
      [[segue destinationViewController] setHeading:@"Valet Mode"];
      [[segue destinationViewController] setFunction:@"Valet Off"];
      [[segue destinationViewController] setDelegate:self];
      }
    else
      { // Valet is OFF, let's offer to activate it
      [[segue destinationViewController] setInstructions:@"Enter PIN to activate valet mode"];
      [[segue destinationViewController] setHeading:@"Valet Mode"];
      [[segue destinationViewController] setFunction:@"Valet On"];
      [[segue destinationViewController] setDelegate:self];
      }
    }
  else if ([[segue identifier] isEqualToString:@"LockUnlock"])
    {
    if ([ovmsAppDelegate myRef].car_doors2 & 0x08)
      { // Car is locked, let's offer to unlock it
        [[segue destinationViewController] setInstructions:@"Enter PIN to unlock car"];
        [[segue destinationViewController] setHeading:@"Unlock Car"];
        [[segue destinationViewController] setFunction:@"Unlock Car"];
        [[segue destinationViewController] setDelegate:self];
      }
    else
      { // Car is unlocked, let's offer to lock it
        [[segue destinationViewController] setInstructions:@"Enter PIN to lock car"];
        [[segue destinationViewController] setHeading:@"Lock Car"];
        [[segue destinationViewController] setFunction:@"Lock Car"];
        [[segue destinationViewController] setDelegate:self];
      }
    }}

- (void)omvsControlPINEntryDelegateDidCancel:(NSString*)fn
{
  [JHNotificationManager notificationWithMessage:@"PIN Cancelled"];

}

- (void)omvsControlPINEntryDelegateDidSave:(NSString*)fn pin:(NSString*)pin
{
  if ([fn isEqualToString:@"Valet On"])
    {
    [JHNotificationManager notificationWithMessage:@"Activating Valet Mode"];
    NSString* cmd = [NSString stringWithFormat:@"21,%@",pin];
    [[ovmsAppDelegate myRef] commandRegister:cmd callback:self];
    }
  else if ([fn isEqualToString:@"Valet Off"])
    {
    [JHNotificationManager notificationWithMessage:@"Deactivating Valet Mode"];
    NSString* cmd = [NSString stringWithFormat:@"23,%@",pin];
    [[ovmsAppDelegate myRef] commandRegister:cmd callback:self];
    }    
  else if ([fn isEqualToString:@"Lock Car"])
    {
    [JHNotificationManager notificationWithMessage:@"Locking Car"];
    NSString* cmd = [NSString stringWithFormat:@"20,%@",pin];
    [[ovmsAppDelegate myRef] commandRegister:cmd callback:self];
    }
  else if ([fn isEqualToString:@"Unlock Car"])
    {
    [JHNotificationManager notificationWithMessage:@"Unlocking Car"];
    NSString* cmd = [NSString stringWithFormat:@"22,%@",pin];
    [[ovmsAppDelegate myRef] commandRegister:cmd callback:self];
    }    
}

- (void)commandResult:(NSArray*)result
{
  [[ovmsAppDelegate myRef] commandCancel];
  if ([result count]>1)
    {
//    int command = [[result objectAtIndex:0] intValue];
    int rcode = [[result objectAtIndex:1] intValue];
    switch (rcode)
      {
      case 0: // ok
        switch ([[result objectAtIndex:0] intValue])
          {
          case 11:
            [JHNotificationManager notificationWithMessage:@"Started Charge"];
            break;
          case 12:
            [JHNotificationManager notificationWithMessage:@"Stopped Charge"];
            break;
          case 20:
            [JHNotificationManager notificationWithMessage:@"Car Locked"];
            break;
          case 21:
            [JHNotificationManager notificationWithMessage:@"Valet Mode Activated"];
            break;
          case 22:
            [JHNotificationManager notificationWithMessage:@"Car Unlocked"];
            break;
          case 23:
            [JHNotificationManager notificationWithMessage:@"Valet Mode Deactivated"];
            break;
          }
        break;
      case 1: // failed
        [JHNotificationManager
         notificationWithMessage:
         [NSString stringWithFormat:@"Failed: %@",[[result objectAtIndex:2] stringValue]]];
        break;
      case 2: // unsupported
        [JHNotificationManager notificationWithMessage:@"Unsupported operation"];
        break;
      case 3: // unimplemented
        [JHNotificationManager notificationWithMessage:@"Unimplemented operation"];
        break;
      default:
        [JHNotificationManager
         notificationWithMessage:
         [NSString stringWithFormat:@"Error: %@",[[result objectAtIndex:2] stringValue]]];
        break;
      }
    }
}

@end

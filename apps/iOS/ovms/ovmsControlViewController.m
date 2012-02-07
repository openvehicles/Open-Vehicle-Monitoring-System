//
//  ovmsControlViewController.m
//  Open Vehicle
//
//  Created by Mark Webb-Johnson on 2/2/12.
//  Copyright (c) 2012 Open Vehicle Systems. All rights reserved.
//

#import "ovmsControlViewController.h"

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

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
    // Return YES for supported orientations
    return (interfaceOrientation == UIInterfaceOrientationPortrait);
}

- (IBAction)doneButton:(id)sender {
  [self dismissModalViewControllerAnimated:YES];
}

- (IBAction)startChargingButton:(id)sender {
}

- (IBAction)stopChargingButton:(id)sender {
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

@end

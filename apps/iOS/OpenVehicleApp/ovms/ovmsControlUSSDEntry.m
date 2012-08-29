//
//  ovmsControlUSSDEntry.m
//  Open Vehicle
//
//  Created by Mark Webb-Johnson on 13/5/12.
//  Copyright (c) 2012 Open Vehicle Systems. All rights reserved.
//

#import "ovmsControlUSSDEntry.h"

@interface ovmsControlUSSDEntry ()

@end

@implementation ovmsControlUSSDEntry
@synthesize m_ussd;

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
  self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
  if (self) {
    // Custom initialization
  }
  return self;
}

- (void)viewDidLoad
{
  [super viewDidLoad];
	// Do any additional setup after loading the view.
}

- (void)viewDidUnload
{
  [self setM_ussd:nil];
  [super viewDidUnload];
  // Release any retained subviews of the main view.
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
  if (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPad)
    return YES;
  else
    return (interfaceOrientation == UIInterfaceOrientationPortrait);
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  [m_ussd becomeFirstResponder];
  
  [TestFlight passCheckpoint:@"MMIUSSD_ENTRY"];
}

- (IBAction)Send:(id)sender
  {
  [[ovmsAppDelegate myRef] commandDoUSSD:m_ussd.text];
  [self.navigationController popViewControllerAnimated:YES];
  }

@end

//
//  ovmsControlPINEntry.m
//  Open Vehicle
//
//  Created by Mark Webb-Johnson on 3/2/12.
//  Copyright (c) 2012 Open Vehicle Systems. All rights reserved.
//

#import "ovmsControlPINEntry.h"

@implementation ovmsControlPINEntry
@synthesize m_pin;
@synthesize m_done;
@synthesize m_message;

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

/*
// Implement viewDidLoad to do additional setup after loading the view, typically from a nib.
- (void)viewDidLoad
{
    [super viewDidLoad];
}
*/

- (void)viewDidUnload
{
  [self setM_pin:nil];
  [self setM_done:nil];
  [self setM_done:nil];
  [self setM_message:nil];
    [super viewDidUnload];
    // Release any retained subviews of the main view.
    // e.g. self.myOutlet = nil;
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
    // Return YES for supported orientations
    return (interfaceOrientation == UIInterfaceOrientationPortrait);
}

-(void) viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  [m_pin becomeFirstResponder];
  m_message.text = @"Please enter the vehicle PIN code to unlock";
  m_done.title = @"Unlock";
}

- (IBAction)Edited:(id)sender {
  [self dismissModalViewControllerAnimated:YES];
}

- (IBAction)Cancel:(id)sender {
  [self dismissModalViewControllerAnimated:YES];
}

- (IBAction)Done:(id)sender {
  [self dismissModalViewControllerAnimated:YES];
}
@end

//
//  ovmsControlViewController.h
//  Open Vehicle
//
//  Created by Mark Webb-Johnson on 2/2/12.
//  Copyright (c) 2012 Open Vehicle Systems. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "ovmsAppDelegate.h"

@interface ovmsControlViewController : UIViewController

@property (strong, nonatomic) IBOutlet UISegmentedControl *m_chargemode;

- (IBAction)doneButton:(id)sender;
- (IBAction)startChargingButton:(id)sender;
- (IBAction)stopChargingButton:(id)sender;
- (IBAction)chargeModeButton:(id)sender;
- (IBAction)lockButton:(id)sender;
- (IBAction)valetButton:(id)sender;
- (IBAction)featuresButton:(id)sender;
- (IBAction)parametersButton:(id)sender;
- (IBAction)cellularUsageButton:(id)sender;
- (IBAction)resetModuleButton:(id)sender;

@end

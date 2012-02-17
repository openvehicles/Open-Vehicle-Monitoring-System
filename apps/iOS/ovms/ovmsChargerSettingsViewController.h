//
//  ovmsChargerSettingsViewController.h
//  Open Vehicle
//
//  Created by Mark Webb-Johnson on 17/2/12.
//  Copyright (c) 2012 Open Vehicle Systems. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "ovmsAppDelegate.h"

@interface ovmsChargerSettingsViewController : UIViewController <UIPickerViewDataSource, UIPickerViewDelegate>

@property (strong, nonatomic) IBOutlet UISegmentedControl *m_charger_mode;
@property (strong, nonatomic) IBOutlet UIPickerView *m_charger_current;
@property (strong, nonatomic) IBOutlet UILabel *m_charger_warning;

- (IBAction)ModeChanged:(id)sender;
- (IBAction)CancelButton:(id)sender;
- (IBAction)DoneButton:(id)sender;

@end

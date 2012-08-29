//
//  ovmsControlFeaturesViewController.h
//  Open Vehicle
//
//  Created by Mark Webb-Johnson on 2/2/12.
//  Copyright (c) 2012 Open Vehicle Systems. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "ovmsAppDelegate.h"
#import "MBProgressHUD.h"

#define FEATURES_MAX 16
#define FEATURES_MAP_PARAM 8
#define FEATURE_SPEEDO       0x00 // Speedometer feature
#define FEATURE_STREAM       0x08 // Location streaming feature
#define FEATURE_MINSOC       0x09 // Minimum SOC feature
#define FEATURE_CARBITS      0x0E // Various ON/OFF features (bitmap)
#define FEATURE_CANWRITE     0x0F // CAN bus can be written to

// The FEATURE_CARBITS feature is a set of ON/OFF bits to control different
// miscelaneous aspects of the system. The following bits are defined:
#define FEATURE_CB_2008      0x01 // Set to 1 to mark the car as 2008/2009
#define FEATURE_CB_SAD_SMS   0x02 // Set to 1 to suppress "Access Denied" SMS
#define FEATURE_CB_SOUT_SMS  0x04 // Set to 1 to suppress all outbound SMS

@interface ovmsControlFeaturesViewController : UITableViewController <ovmsCommandDelegate, MBProgressHUDDelegate>
  {
  int feature[FEATURES_MAX];
  BOOL features_ready;
  }

@property (strong, nonatomic) IBOutlet UITableView *m_table;

- (void)startSpinner:(NSString *)label;
- (void)stopSpinner;
- (void)textFieldDidEndEditing:(UITextField *)textField;

@end

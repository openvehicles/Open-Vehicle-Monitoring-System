//
//  ovmsControlParametersViewController.h
//  Open Vehicle
//
//  Created by Mark Webb-Johnson on 2/2/12.
//  Copyright (c) 2012 Open Vehicle Systems. All rights reserved.
//

#import <UIKit/UIKit.h>
#include "ovmsAppDelegate.h"
#import "MBProgressHUD.h"

#define PARAM_MAX 32
#define PARAM_MAX_LENGTH 32
#define PARAM_BANKSIZE 8

#define PARAM_REGPHONE  0x00
#define PARAM_REGPASS   0x01
#define PARAM_MILESKM   0x02
#define PARAM_NOTIFIES  0x03
#define PARAM_SERVERIP  0x04
#define PARAM_GPRSAPN   0x05
#define PARAM_GPRSUSER  0x06
#define PARAM_GPRSPASS  0x07
#define PARAM_MYID      0x08
#define PARAM_NETPASS1  0x09
#define PARAM_PARANOID  0x0A
#define PARAM_S_GROUP1  0x0B
#define PARAM_S_GROUP2  0x0C

#define PARAM_FEATURE_S 0x18
#define PARAM_FEATURE8  0x18
#define PARAM_FEATURE9  0x19
#define PARAM_FEATURE10 0x1A
#define PARAM_FEATURE11 0x1B
#define PARAM_FEATURE12 0x1C
#define PARAM_FEATURE13 0x1D
#define PARAM_FEATURE14 0x1E
#define PARAM_FEATURE15 0x1F

@interface ovmsControlParametersViewController : UITableViewController <ovmsCommandDelegate, UITextFieldDelegate, MBProgressHUDDelegate>
{
  NSString* param[PARAM_FEATURE_S];
  BOOL params_ready;
}

@property (strong, nonatomic) IBOutlet UITableView *m_table;

- (void)startSpinner:(NSString *)label;
- (void)stopSpinner;
- (void)textFieldDidEndEditing:(UITextField *)textField;

@end

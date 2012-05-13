//
//  ovmsControlUSSDEntry.h
//  Open Vehicle
//
//  Created by Mark Webb-Johnson on 13/5/12.
//  Copyright (c) 2012 Open Vehicle Systems. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "ovmsAppDelegate.h"

@interface ovmsControlUSSDEntry : UIViewController

@property (strong, nonatomic) IBOutlet UITextField *m_ussd;

- (IBAction)Send:(id)sender;

@end

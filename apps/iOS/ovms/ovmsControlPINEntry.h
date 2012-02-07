//
//  ovmsControlPINEntry.h
//  Open Vehicle
//
//  Created by Mark Webb-Johnson on 3/2/12.
//  Copyright (c) 2012 Open Vehicle Systems. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface ovmsControlPINEntry : UIViewController

@property (strong, nonatomic) IBOutlet UITextField *m_pin;
@property (strong, nonatomic) IBOutlet UIBarButtonItem *m_done;
@property (strong, nonatomic) IBOutlet UITextView *m_message;

- (IBAction)Edited:(id)sender;
- (IBAction)Cancel:(id)sender;
- (IBAction)Done:(id)sender;

@end

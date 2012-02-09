//
//  ovmsControlPINEntry.h
//  Open Vehicle
//
//  Created by Mark Webb-Johnson on 3/2/12.
//  Copyright (c) 2012 Open Vehicle Systems. All rights reserved.
//

#import <UIKit/UIKit.h>

@protocol ovmsControlPINEntryDelegate <NSObject>
- (void)omvsControlPINEntryDelegateDidCancel:(NSString*)fn;
- (void)omvsControlPINEntryDelegateDidSave:(NSString*)fn pin:(NSString*)pin;
@end

@interface ovmsControlPINEntry : UIViewController
{
  NSString *_instructions;
  NSString *_heading;
  NSString *_function;
}

@property (strong, nonatomic) NSString *instructions;
@property (strong, nonatomic) NSString *heading;
@property (strong, nonatomic) NSString *function;
@property (nonatomic, weak) id <ovmsControlPINEntryDelegate> delegate;

@property (strong, nonatomic) IBOutlet UITextField *m_pin;
@property (strong, nonatomic) IBOutlet UIBarButtonItem *m_done;
@property (strong, nonatomic) IBOutlet UITextView *m_message;
@property (strong, nonatomic) IBOutlet UINavigationBar *m_navbar;

- (IBAction)Edited:(id)sender;
- (IBAction)Cancel:(id)sender;
- (IBAction)Done:(id)sender;

@end

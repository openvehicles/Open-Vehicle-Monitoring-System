//
//  ovmsCarsFormViewController.h
//  ovms
//
//  Created by Mark Webb-Johnson on 23/11/11.
//  Copyright (c) 2011 Hong Hay Villa. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "ovmsAppDelegate.h"

@interface ovmsCarsFormViewController : UIViewController <ovmsUpdateDelegate, UITextFieldDelegate, UIPickerViewDataSource, UIPickerViewDelegate>
{
  NSMutableArray            *carImages;
  NSString *_carEditing;
  NSManagedObjectContext *_context;
}

@property (strong, nonatomic) NSMutableArray *carImages;
@property (nonatomic, retain) NSManagedObjectContext *context;
@property (strong, nonatomic) NSString *carEditing;

@property (strong, nonatomic) IBOutlet UITextField *vehicleid;
@property (strong, nonatomic) IBOutlet UITextField *vehiclelabel;
@property (strong, nonatomic) IBOutlet UITextField *vehicleNetPass;
@property (strong, nonatomic) IBOutlet UITextField *vehicleUserPass;
@property (strong, nonatomic) IBOutlet UIPickerView *vehicleImage;

@property (strong, nonatomic) IBOutlet UIBarButtonItem *m_control_button;

@end

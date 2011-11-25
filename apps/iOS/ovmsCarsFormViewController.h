//
//  ovmsCarsFormViewController.h
//  ovms
//
//  Created by Mark Webb-Johnson on 23/11/11.
//  Copyright (c) 2011 Hong Hay Villa. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface ovmsCarsFormViewController : UIViewController <UIPickerViewDataSource, UIPickerViewDelegate>
{
  NSMutableArray            *carImages;
}
@property (strong, nonatomic) NSMutableArray *carImages;

@property (strong, nonatomic) IBOutlet UITextField *vehicleid;
@property (strong, nonatomic) IBOutlet UITextField *vehiclelabel;
@property (strong, nonatomic) IBOutlet UITextField *vehicleNetPass;
@property (strong, nonatomic) IBOutlet UITextField *vehicleUserPass;
@property (strong, nonatomic) IBOutlet UIPickerView *vehicleImage;

- (IBAction)textFieldReturn:(id)sender;

@end
